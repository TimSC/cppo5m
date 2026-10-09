#include "pbf.h"
#include "intmath.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include "pbf/fileformat.pb.h"
#include "pbf/osmformat.pb.h"
#include <arpa/inet.h>
#include <cstring>
#include <zlib.h>
using namespace std;

// Size limits from the PBF specification
static const uint32_t PBF_MAX_BLOB_HEADER_SIZE = 64 * 1024;
static const uint32_t PBF_MAX_BLOB_SIZE = 32 * 1024 * 1024;

static std::string CompressData(const std::string &data)
{
	uLongf len = compressBound(data.size());
	std::string out(len, '\0');
	if(compress2((Bytef *)&out[0], &len, (const Bytef *)data.data(), data.size(), Z_DEFAULT_COMPRESSION) != Z_OK)
		throw std::runtime_error("Error compressing PBF blob");
	out.resize(len);
	return out;
}

///Inflates zlib data, stopping with an error as soon as the output would pass
///maxSize. A small blob can expand enormously, so the limit is enforced while
///inflating, not afterwards.
static std::string DecompressData(const std::string &data, size_t maxSize)
{
	z_stream zs;
	memset(&zs, 0, sizeof(zs));
	if(inflateInit(&zs) != Z_OK)
		throw OsmDecodeError("Error starting PBF blob decompression");

	std::string out;
	char chunk[64 * 1024];
	zs.next_in = (Bytef *)data.data();
	zs.avail_in = data.size();
	int ret = Z_OK;
	while(ret != Z_STREAM_END)
	{
		zs.next_out = (Bytef *)chunk;
		zs.avail_out = sizeof(chunk);
		ret = inflate(&zs, Z_NO_FLUSH);
		size_t produced = sizeof(chunk) - zs.avail_out;
		if(ret != Z_OK && ret != Z_STREAM_END)
		{
			inflateEnd(&zs);
			throw OsmDecodeError("PBF blob has corrupt compressed data");
		}
		if(out.size() + produced > maxSize)
		{
			inflateEnd(&zs);
			throw OsmDecodeError("PBF blob decompresses to more than the format allows");
		}
		out.append(chunk, produced);
		if(ret == Z_OK && produced == 0 && zs.avail_in == 0)
		{
			inflateEnd(&zs);
			throw OsmDecodeError("PBF blob compressed data is incomplete");
		}
	}
	inflateEnd(&zs);
	return out;
}

///File timestamps are in units of date_granularity milliseconds.
static int64_t DecodeTimestamp(int64_t value, int32_t date_granularity)
{
	int64_t milliseconds = 0;
	if(__builtin_mul_overflow(value, (int64_t)date_granularity, &milliseconds))
		throw OsmDecodeError("PBF timestamp is out of range");
	return milliseconds / 1000;
}

static int64_t EncodeTimestamp(int64_t seconds, int32_t date_granularity)
{
	int64_t milliseconds = 0;
	if(__builtin_mul_overflow(seconds, (int64_t)1000, &milliseconds))
		throw std::range_error("Timestamp is too large for the PBF format");
	return milliseconds / date_granularity;
}

static void ReadExactLengthPbf(std::istream &str, char *out, size_t len)
{
	size_t total = 0;
	while(total < len)
	{
		str.read(&out[total], len-total);
		total += str.gcount();
		if(str.fail() && total < len)
			throw OsmDecodeError("PBF input ends part way through a block");
	}
}

///Looks up a string table entry. Entry zero is always the empty string, which
///is how an empty tag value, role or user name is stored.
static bool LookupString(const std::vector<std::string> &stringTab, int64_t index, const std::string *&out)
{
	if(index < 0 || (uint64_t)index >= stringTab.size())
		return false;
	out = &stringTab[index];
	return true;
}

template <class T> static void DecodeTags(const T &obj, const std::vector<std::string> &stringTab, TagMap &tags)
{
	for(int j=0; j<obj.keys_size() and j<obj.vals_size(); j++)
	{
		const std::string *key = nullptr, *val = nullptr;
		if(LookupString(stringTab, obj.keys(j), key) and LookupString(stringTab, obj.vals(j), val))
			tags[*key] = *val;
	}
}

static void DecodeOsmHeader(const std::string &decBlob, IDataStreamHandler &output)
{
	OSMPBF::HeaderBlock hb;
	if(!hb.ParseFromString(decBlob))
		throw OsmDecodeError("Error decoding PBF HeaderBlock");

	for(int i=0; i<hb.required_features_size(); i++)
	{
		const std::string &feature = hb.required_features(i);
		if(feature != "OsmSchema-V0.6" and feature != "DenseNodes" and feature != "HistoricalInformation")
			throw OsmDecodeError("PBF file requires an unsupported feature: " + feature);
	}

	if(hb.has_bbox())
	{
		const OSMPBF::HeaderBBox &bbox = hb.bbox();
		if(bbox.has_left() and bbox.has_right() and bbox.has_top() and bbox.has_bottom())
			output.StoreBounds(Bounds(bbox.left()*1e-9, bbox.bottom()*1e-9, bbox.right()*1e-9, bbox.top()*1e-9));
	}
}

static void DecodeOsmInfo(const OSMPBF::Info &info, int32_t date_granularity, const std::vector<std::string> &stringTab, MetaData &out)
{
	if(info.has_version())
		out.version = info.version();
	if(info.has_timestamp())
		out.timestamp = DecodeTimestamp(info.timestamp(), date_granularity);
	if(info.has_changeset())
		out.changeset = info.changeset();
	if(info.has_uid())
		out.uid = info.uid();
	const std::string *username = nullptr;
	if(info.has_user_sid() and LookupString(stringTab, info.user_sid(), username))
		out.username = *username;
	if(info.has_visible())
		out.visible = info.visible();
}

static void DecodeOsmNodes(const OSMPBF::PrimitiveGroup& pg,
	int64_t lat_offset, int64_t lon_offset,
	int32_t granularity, 
	int32_t date_granularity,
	const std::vector<std::string> &stringTab,
	IDataStreamHandler &output)
{
	for(int i=0; i<pg.nodes_size(); i++)
	{
		const OSMPBF::Node &pbfNode = pg.nodes(i);
		OsmNode node;
		node.objId = pbfNode.id();
		DecodeTags(pbfNode, stringTab, node.tags);
		if(pbfNode.has_info())
			DecodeOsmInfo(pbfNode.info(), date_granularity, stringTab, node.metaData);
		node.lat = 1e-9 * ((double)lat_offset + (double)granularity * (double)pbfNode.lat());
		node.lon = 1e-9 * ((double)lon_offset + (double)granularity * (double)pbfNode.lon());
		output.StoreNode(node);
	}
}

static void DecodeOsmDenseNodes(const OSMPBF::DenseNodes &dense,
	int64_t lat_offset, int64_t lon_offset,
	int32_t granularity, int32_t date_granularity,
	const std::vector<std::string> &stringTab,
	IDataStreamHandler &output)
{
	//Tags for all the nodes are in one list: key and value indices for each
	//node in turn, with a zero ending each node's entries
	int kvc = 0;

	int64_t idc = 0, latc = 0, lonc = 0, timestampc = 0, changesetc = 0;
	int64_t uidc = 0, user_sidc = 0;
	OsmNode node;
	for(int j=0; j<dense.id_size() and j<dense.lat_size() and j<dense.lon_size(); j++)
	{
		idc = WrapAdd(idc, dense.id(j));
		latc = WrapAdd(latc, dense.lat(j));
		lonc = WrapAdd(lonc, dense.lon(j));

		node.objId = idc;
		node.metaData = MetaData();
		node.tags.clear();

		while(kvc < dense.keys_vals_size())
		{
			int32_t keyIndex = dense.keys_vals(kvc);
			kvc ++;
			if(keyIndex == 0)
				break;
			if(kvc >= dense.keys_vals_size())
				throw OsmDecodeError("PBF dense node tag has a key without a value");
			int32_t valIndex = dense.keys_vals(kvc);
			kvc ++;
			const std::string *key = nullptr, *val = nullptr;
			if(LookupString(stringTab, keyIndex, key) and LookupString(stringTab, valIndex, val))
				node.tags[*key] = *val;
		}

		if(dense.has_denseinfo())
		{
			const OSMPBF::DenseInfo &di = dense.denseinfo();
			if(j < di.version_size())
				node.metaData.version = di.version(j);

			if(j < di.timestamp_size())
			{
				timestampc = WrapAdd(timestampc, di.timestamp(j));
				node.metaData.timestamp = DecodeTimestamp(timestampc, date_granularity);
			}

			if(j < di.changeset_size())
			{
				changesetc = WrapAdd(changesetc, di.changeset(j));
				node.metaData.changeset = changesetc;
			}

			if(j < di.uid_size())
			{
				uidc = WrapAdd(uidc, di.uid(j));
				node.metaData.uid = uidc;
			}

			if(j < di.user_sid_size())
			{
				user_sidc = WrapAdd(user_sidc, di.user_sid(j));
				const std::string *username = nullptr;
				if(LookupString(stringTab, user_sidc, username))
					node.metaData.username = *username;
			}

			if(j < di.visible_size())
				node.metaData.visible = di.visible(j);
		}

		node.lat = 1e-9 * ((double)lat_offset + (double)granularity * (double)latc);
		node.lon = 1e-9 * ((double)lon_offset + (double)granularity * (double)lonc);
		output.StoreNode(node);
	}
}

static void DecodeOsmWays(const OSMPBF::PrimitiveGroup& pg,
	int32_t date_granularity,
	const std::vector<std::string> &stringTab,
	IDataStreamHandler &output)
{
	for(int i=0; i<pg.ways_size(); i++)
	{
		const OSMPBF::Way &pbfWay = pg.ways(i);
		OsmWay way;
		way.objId = pbfWay.id();
		DecodeTags(pbfWay, stringTab, way.tags);
		if(pbfWay.has_info())
			DecodeOsmInfo(pbfWay.info(), date_granularity, stringTab, way.metaData);

		int64_t refsc = 0;
		way.refs.reserve(pbfWay.refs_size());
		for(int j=0; j<pbfWay.refs_size(); j++)
		{
			refsc = WrapAdd(refsc, pbfWay.refs(j));
			way.refs.push_back(refsc);
		}
		output.StoreWay(way);
	}
}

static void DecodeOsmRelations(const OSMPBF::PrimitiveGroup& pg,
	int32_t date_granularity,
	const std::vector<std::string> &stringTab,
	IDataStreamHandler &output)
{
	for(int i=0; i<pg.relations_size(); i++)
	{
		const OSMPBF::Relation &pbfRelation = pg.relations(i);
		OsmRelation relation;
		relation.objId = pbfRelation.id();
		DecodeTags(pbfRelation, stringTab, relation.tags);
		if(pbfRelation.has_info())
			DecodeOsmInfo(pbfRelation.info(), date_granularity, stringTab, relation.metaData);

		if(pbfRelation.memids_size() != pbfRelation.types_size() or
			pbfRelation.memids_size() != pbfRelation.roles_sid_size())
			throw OsmDecodeError("PBF relation member lists have different lengths");

		int64_t memidsc = 0;
		for(int j=0; j<pbfRelation.memids_size(); j++)
		{
			memidsc = WrapAdd(memidsc, pbfRelation.memids(j));
			RelationMember member;
			member.ref = memidsc;
			switch(pbfRelation.types(j))
			{
				case OSMPBF::Relation_MemberType_NODE:
					member.type = ObjectType::Node;
					break;
				case OSMPBF::Relation_MemberType_WAY:
					member.type = ObjectType::Way;
					break;
				case OSMPBF::Relation_MemberType_RELATION:
					member.type = ObjectType::Relation;
					break;
				default:
					throw OsmDecodeError("PBF relation member has an unknown type");
			}
			const std::string *role = nullptr;
			if(LookupString(stringTab, pbfRelation.roles_sid(j), role))
				member.role = *role;
			relation.members.push_back(member);
		}
		output.StoreRelation(relation);
	}
}

// ********************************************

PbfDecode::PbfDecode(std::streambuf &input, IDataStreamHandler &outputIn):
	OsmDecoder(outputIn),
	handle(&input),
	anyObject(false),
	prevObjType(ObjectType::Node)
{

}

bool PbfDecode::DecodeNext()
{
	if(this->finished)
		return false;

	if(this->handle.peek() == std::char_traits<char>::eof())
	{
		this->MarkFinished();
		return false;
	}

	uint32_t blobHeaderLenNbo = 0;
	ReadExactLengthPbf(handle, (char*)&blobHeaderLenNbo, sizeof(uint32_t));
	uint32_t blobHeaderLen = ntohl(blobHeaderLenNbo);
	if(blobHeaderLen > PBF_MAX_BLOB_HEADER_SIZE)
		throw OsmDecodeError("PBF blob header is larger than the format allows");

	std::string refData;
	refData.resize(blobHeaderLen);
	if(blobHeaderLen > 0)
		ReadExactLengthPbf(handle, &refData[0], blobHeaderLen);

	OSMPBF::BlobHeader header;
	if(!header.ParseFromString(refData))
		throw OsmDecodeError("Error decoding PBF BlobHeader");

	if(header.datasize() < 0 or (uint32_t)header.datasize() > PBF_MAX_BLOB_SIZE)
		throw OsmDecodeError("PBF blob is larger than the format allows");
	std::string refData2;
	refData2.resize(header.datasize());
	if(header.datasize() > 0)
		ReadExactLengthPbf(handle, &refData2[0], header.datasize());

	const std::string &headerType = header.type();
	if(headerType != "OSMHeader" and headerType != "OSMData")
		return true; //The specification says to skip blob types we do not know

	OSMPBF::Blob blob;
	if(!blob.ParseFromString(refData2))
		throw OsmDecodeError("Error decoding PBF Blob");

	string decBlob;
	if(blob.has_raw())
	{
		if(blob.raw().size() > PBF_MAX_BLOB_SIZE)
			throw OsmDecodeError("PBF blob is larger than the format allows");
		decBlob = blob.raw();
	}
	else if(blob.has_zlib_data())
	{
		if(blob.has_raw_size() and (blob.raw_size() < 0 or (uint32_t)blob.raw_size() > PBF_MAX_BLOB_SIZE))
			throw OsmDecodeError("PBF blob is larger than the format allows");
		size_t maxSize = blob.has_raw_size() ? (size_t)blob.raw_size() : PBF_MAX_BLOB_SIZE;
		decBlob = DecompressData(blob.zlib_data(), maxSize);
		if(blob.has_raw_size() and decBlob.size() != (size_t)blob.raw_size())
			throw OsmDecodeError("PBF blob does not decompress to its stated size");
	}
	else
		throw OsmDecodeError("PBF blob uses an unsupported compression method");

	if(headerType == "OSMHeader")
		DecodeOsmHeader(decBlob, this->output);
	else
		this->DecodeOsmData(decBlob);
	return true;
}

void PbfDecode::DecodeOsmData(const std::string &decBlob)
{
	OSMPBF::PrimitiveBlock pb;
	if(!pb.ParseFromString(decBlob))
		throw OsmDecodeError("Error decoding PBF PrimitiveBlock");

	std::vector<std::string> stringTab;
	if(pb.has_stringtable())
	{
		const OSMPBF::StringTable &st = pb.stringtable();
		stringTab.reserve(st.s_size());
		for(int i=0; i<st.s_size(); i++)
			stringTab.push_back(st.s(i));
	}

	int64_t lat_offset = 0, lon_offset = 0;
	int32_t granularity = 100, date_granularity=1000;
	if(pb.has_granularity())
		granularity = pb.granularity();
	if(pb.has_lat_offset())
		lat_offset = pb.lat_offset();
	if(pb.has_lon_offset())
		lon_offset = pb.lon_offset();
	if(pb.has_date_granularity())
		date_granularity = pb.date_granularity();

	int pbs = pb.primitivegroup_size();
	for(int i=0; i<pbs; i++)
	{
		const OSMPBF::PrimitiveGroup& pg = pb.primitivegroup(i);

		if(pg.nodes_size() > 0)
		{
			CheckOutputType(ObjectType::Node);
			DecodeOsmNodes(pg, lat_offset, lon_offset,
				granularity, date_granularity,
				stringTab, this->output);	
		}

		if(pg.has_dense())
		{
			CheckOutputType(ObjectType::Node);
			DecodeOsmDenseNodes(pg.dense(), lat_offset, lon_offset,
				granularity, date_granularity,
				stringTab, this->output);
		}

		if(pg.ways_size() > 0)
		{
			CheckOutputType(ObjectType::Way);
			DecodeOsmWays(pg, date_granularity,
				stringTab, this->output);
		}

		if(pg.relations_size() > 0)
		{
			CheckOutputType(ObjectType::Relation);
			DecodeOsmRelations(pg, date_granularity,
				stringTab, this->output);
		}

		//Changeset decoding not supported
	}
}

void PbfDecode::CheckOutputType(ObjectType objType)
{
	//Some encoders need to know when we switch object type
	if(this->anyObject and this->prevObjType != objType)
		this->output.Reset();
	this->anyObject = true;
	this->prevObjType = objType;
}

// *******************************************

///PBF stores the version and user ID in 32 bits. Refuse values that do not
///fit instead of writing something else.
static void CheckPbfMetaData(const MetaData &metaData)
{
	if(metaData.version > (uint64_t)INT32_MAX)
		throw std::range_error("Object version is too large for the PBF format");
	if(metaData.uid > (uint64_t)INT32_MAX)
		throw std::range_error("User ID is too large for the PBF format");
}

static void GenerateStringTable(const std::vector<const OsmObject *> &ways, size_t startc, 
	bool encodeMetaData, 
	size_t &maxWaysToProcess, OSMPBF::StringTable *st, 
	std::map<std::string, int32_t> &strIndexOut)
{
	std::map<std::string, std::uint32_t> strFreq;
	size_t processed = 0;
	for(size_t i=startc; i<startc + maxWaysToProcess; i++)
	{
		const OsmObject &n = *(ways[i]);
		const TagMap &tags = n.tags;
		for(auto it = tags.begin(); it != tags.end(); it++)
		{
			auto it2 = strFreq.find(it->first);
			if(it2 != strFreq.end())
				it2->second ++;
			else
				strFreq[it->first] = 1;

			it2 = strFreq.find(it->second);
			if(it2 != strFreq.end())
				it2->second ++;
			else
				strFreq[it->second] = 1;
		}

		if(encodeMetaData)
		{
			auto it2 = strFreq.find(n.metaData.username);
			if(it2 != strFreq.end())
				it2->second ++;
			else
				strFreq[n.metaData.username] = 1;
		}

		const OsmRelation *rel = dynamic_cast<const OsmRelation *>(&n);
		if(rel != nullptr)
		{
			for(size_t j=0; j<rel->members.size(); j++)
			{
				const std::string &role = rel->members[j].role;
				auto it2 = strFreq.find(role);
				if(it2 != strFreq.end())
					it2->second ++;
				else
					strFreq[role] = 1;
			}
		}

		processed = i;
		if(strFreq.size() >= INT32_MAX - 1000)
			break; //Stop because we have a full string table
	}
	maxWaysToProcess = processed+1-startc;

	std::map<std::uint32_t, std::vector<std::string> > strByFreq;
	for(auto it=strFreq.begin(); it != strFreq.end(); it++)
	{
		strByFreq[it->second].push_back(it->first);
	}
	vector<std::uint32_t> freqBins;
	for(auto it = strByFreq.begin(); it != strByFreq.end(); it++)
	{
		freqBins.push_back(it->first);
	}
	std::sort(freqBins.begin(), freqBins.end(), greater<int>()); 

	//Add strings to block
	st->add_s(""); //First string is always empty in pbf
	for(size_t i=0; i<freqBins.size(); i++)
	{
		std::vector<std::string> &bin = strByFreq[freqBins[i]];
		std::sort(bin.begin(), bin.end());
		for(size_t j=0; j<bin.size(); j++)
			st->add_s(bin[j]);
	}

	//Index strings for fast lookup
	for(int i=1; i<st->s_size(); i++)
	{
		strIndexOut[st->s(i)] = i;
	}
}

// *******************************************

PbfEncode::PbfEncode(std::shared_ptr<ByteSink> sinkIn) :
	OsmEncoder(sinkIn),
	anyObject(false),
	prevObjType(ObjectType::Node)
{
	encodeMetaData = true;
	encodeHistorical = false;
	maxBlockObjects = 8000;
	headerWritten = false;
	compressUsingZLib = true;
	writingProgram = "cppo5m";
	maxPayloadSize = PBF_MAX_BLOB_SIZE;
	maxHeaderSize = PBF_MAX_BLOB_HEADER_SIZE;
	lat_offset = 0;
	lon_offset = 0;
	granularity = 100;
	date_granularity=1000;
}

PbfEncode::PbfEncode(std::streambuf &output) :
	PbfEncode(std::make_shared<StreamSink>(output))
{

}

void PbfEncode::Finish()
{
	this->EncodeBuffer();
}

void PbfEncode::StoreBounds(const Bounds &bounds)
{
	//Only the first is kept; it goes in the header, which is written with the first block
	if(!this->headerWritten and this->buffer.bounds.empty())
		this->buffer.bounds.push_back(bounds);
}

///A block holds one type of object, so write it out when the type changes, and
///when it is full. Only one block's worth of objects is ever held in memory.
void PbfEncode::BeforeStore(ObjectType objType, size_t buffered)
{
	if((this->anyObject and this->prevObjType != objType) or buffered >= std::max<size_t>(maxBlockObjects, 1))
		this->EncodeBuffer();
	this->anyObject = true;
	this->prevObjType = objType;
}

void PbfEncode::StoreNode(const OsmNode &node)
{
	this->BeforeStore(ObjectType::Node, buffer.nodes.size());
	buffer.nodes.push_back(node);
}

void PbfEncode::StoreWay(const OsmWay &way)
{
	this->BeforeStore(ObjectType::Way, buffer.ways.size());
	buffer.ways.push_back(way);
}

void PbfEncode::StoreRelation(const OsmRelation &relation)
{
	this->BeforeStore(ObjectType::Relation, buffer.relations.size());
	buffer.relations.push_back(relation);
}

void PbfEncode::WriteBlobPayload(const std::string &blobPayload, const char *type)
{
	//cout << type << "," << blobPayload.size() << endl;
	if(blobPayload.size() > this->maxPayloadSize)
		throw runtime_error("Blob payload size exceeds what PBF allows");

	OSMPBF::Blob blob;
	blob.set_raw_size(blobPayload.size());
	if(this->compressUsingZLib)
		blob.set_zlib_data(CompressData(blobPayload));
	else
		blob.set_raw(blobPayload);
	std::string packedBlob;
	blob.SerializeToString(&packedBlob);

	OSMPBF::BlobHeader header;
	header.set_type(type);
	header.set_datasize(packedBlob.size());
	std::string packedHeader;
	header.SerializeToString(&packedHeader);

	int32_t headerSizePk = htonl(packedHeader.size());
	this->Write((const char*)&headerSizePk, sizeof(int32_t));
	this->Write(packedHeader);
	this->Write(packedBlob);
}

void PbfEncode::EncodeBuffer()
{
	if(this->granularity <= 0 or this->date_granularity <= 0)
		throw std::invalid_argument("PBF granularity settings must be positive");
	if(!headerWritten)
	{
		std::string hbEncoded;
		this->EncodeHeaderBlock(hbEncoded);
		this->WriteBlobPayload(hbEncoded, "OSMHeader");
		this->headerWritten = true;
	}

	int countTypes = 0;
	countTypes += this->buffer.nodes.size() > 0;
	countTypes += this->buffer.ways.size() > 0;
	countTypes += this->buffer.relations.size() > 0;
	if(countTypes == 0)
		return;
	if(countTypes > 1)
	{
		throw logic_error("PBF encoder buffer holds more than one type of object");
	}
	if(this->buffer.nodes.size() > 0)
	{
		std::string denseNodes;
		size_t nodesc = 0;
		while(nodesc < this->buffer.nodes.size())
		{
			EncodePbfDenseNodesSizeLimited(this->buffer.nodes, nodesc, denseNodes);
			this->WriteBlobPayload(denseNodes, "OSMData");
		}
	}

	if(this->buffer.ways.size() > 0)
	{
		std::string waysPacked;
		size_t wayc = 0;
		while(wayc < this->buffer.ways.size())
		{
			EncodePbfWaysSizeLimited(this->buffer.ways, wayc, waysPacked);
			this->WriteBlobPayload(waysPacked, "OSMData");
		}
	}

	if(this->buffer.relations.size() > 0)
	{
		std::string relsPacked;
		size_t relc = 0;
		while(relc < this->buffer.relations.size())
		{
			EncodePbfRelationsSizeLimited(this->buffer.relations, relc, relsPacked);
			this->WriteBlobPayload(relsPacked, "OSMData");
		}
	}

	this->buffer.Clear();
}

void PbfEncode::EncodeHeaderBlock(std::string &out)
{
	OSMPBF::HeaderBlock hb;

	hb.add_required_features("OsmSchema-V0.6");
	hb.add_required_features("DenseNodes");
	if(this->encodeHistorical)
		hb.add_required_features("HistoricalInformation");
	hb.set_writingprogram(this->writingProgram);

	if(buffer.bounds.size() > 0)
	{
		OSMPBF::HeaderBBox *bbox = hb.mutable_bbox();
		const Bounds &srcBbox = buffer.bounds[0]; //Only the first bbox is considered

		bbox->set_left(RoundCoord(srcBbox.minLon / 1e-9));
		bbox->set_bottom(RoundCoord(srcBbox.minLat / 1e-9));
		bbox->set_right(RoundCoord(srcBbox.maxLon / 1e-9));
		bbox->set_top(RoundCoord(srcBbox.maxLat / 1e-9));
	}

	hb.SerializeToString(&out);

	if(out.size() > this->maxHeaderSize)
		throw runtime_error("HeaderBlock size exceeds what PBF allows");
}

void PbfEncode::EncodePbfDenseNodes(const std::vector<OsmNode> &nodes, size_t &nodec, 
	size_t maxNodesToProcess, 
	std::string &out)
{
	//Create string table
	size_t nodesRemaining = nodes.size()-nodec;
	if(maxNodesToProcess > nodesRemaining)
		maxNodesToProcess = nodesRemaining;
	if(maxNodesToProcess > std::max<size_t>(this->maxBlockObjects, 1))
		maxNodesToProcess = std::max<size_t>(this->maxBlockObjects, 1);
	const size_t startNodec = nodec;

	OSMPBF::PrimitiveBlock pb;
	pb.set_granularity(this->granularity);
	pb.set_lat_offset(this->lat_offset);
	pb.set_lon_offset(this->lon_offset);
	pb.set_date_granularity(this->date_granularity);

	OSMPBF::StringTable *st = pb.mutable_stringtable();

	std::vector<const OsmObject *> nodePtrs;
	for(size_t i=0; i<nodes.size(); i++)
		nodePtrs.push_back(&nodes[i]);
	std::map<std::string, int32_t> strIndex;

	GenerateStringTable(nodePtrs, nodec, this->encodeMetaData,
		maxNodesToProcess, st, 
		strIndex);

	//Write nodes in groups
	size_t stopIndex = startNodec+maxNodesToProcess;
	while(nodec < stopIndex)
	{
		size_t nodesInGroup = stopIndex - nodec;
		if(nodesInGroup > std::max<size_t>(this->maxBlockObjects, 1))
			 nodesInGroup = std::max<size_t>(this->maxBlockObjects, 1);

		//Check if all tags are empty
		//TODO

		OSMPBF::PrimitiveGroup *pg = pb.add_primitivegroup();
		OSMPBF::DenseNodes *dn = pg->mutable_dense();
		OSMPBF::DenseInfo *di = nullptr; 
		if(this->encodeMetaData)
			di = dn->mutable_denseinfo();

		int64_t idc = 0, latc = 0, lonc = 0, timestampc = 0, changesetc = 0;
		int64_t uidc = 0, user_sidc = 0;
		for(size_t i=nodec; i<nodec+nodesInGroup; i++)
		{
			const OsmNode &n = nodes[i];
			dn->add_id(WrapSub(n.objId, idc));
			idc = n.objId;
			int64_t lati = RoundCoord(((n.lat / 1e-9) - lat_offset) / granularity);
			dn->add_lat(WrapSub(lati, latc));
			latc = lati;
			int64_t loni = RoundCoord(((n.lon / 1e-9) - lon_offset) / granularity);
			dn->add_lon(WrapSub(loni, lonc));
			lonc = loni;

			const TagMap &tags = n.tags;
			for(auto it = tags.begin(); it != tags.end(); it++)
			{
				dn->add_keys_vals(strIndex[it->first]);
				dn->add_keys_vals(strIndex[it->second]);
			}
			dn->add_keys_vals(0);

			if(this->encodeMetaData)
			{
				CheckPbfMetaData(n.metaData);
				di->add_version(n.metaData.version);
				int64_t ts = EncodeTimestamp(n.metaData.timestamp, date_granularity);
				di->add_timestamp(WrapSub(ts, timestampc));
				timestampc = ts;
				di->add_changeset(WrapSub(n.metaData.changeset, changesetc));
				changesetc = n.metaData.changeset;
				di->add_uid((int64_t)n.metaData.uid - uidc);
				uidc = n.metaData.uid;
				int32_t si = strIndex[n.metaData.username];
				di->add_user_sid(si-user_sidc);
				user_sidc = si;

				if(this->encodeHistorical)
					di->add_visible(n.metaData.visible);
			}
		}

		nodec += nodesInGroup;
	}

	pb.SerializeToString(&out);
}

void PbfEncode::EncodePbfDenseNodesSizeLimited(const std::vector<OsmNode> &nodes, size_t &nodec, std::string &out)
{
	const size_t startNodec = nodec;
	uint32_t objLimit = std::max<size_t>(this->maxBlockObjects, 1);
	this->EncodePbfDenseNodes(nodes, nodec, objLimit, out);

	while(out.size() > maxPayloadSize)
	{
		//Payload is too big, so we need to re-encode by limiting number of groups
		objLimit /= 2;
		nodec = startNodec;
		this->EncodePbfDenseNodes(nodes, nodec, objLimit, out);

		if(out.size() > maxPayloadSize and objLimit <= 1)
			throw runtime_error("Failed to encode nodes without breaking maxPayloadSize limit");
	}
}

void PbfEncode::EncodePbfWays(const std::vector<OsmWay> &ways, size_t &wayc, size_t maxWaysToProcess, 
	std::string &out)
{
	//Create string table
	size_t waysRemaining = ways.size()-wayc;
	if(maxWaysToProcess > waysRemaining)
		maxWaysToProcess = waysRemaining;
	if(maxWaysToProcess > std::max<size_t>(this->maxBlockObjects, 1))
		maxWaysToProcess = std::max<size_t>(this->maxBlockObjects, 1);
	const size_t startWayc = wayc;

	OSMPBF::PrimitiveBlock pb;
	pb.set_date_granularity(this->date_granularity);

	OSMPBF::StringTable *st = pb.mutable_stringtable();

	std::vector<const OsmObject *> wayPtrs;
	for(size_t i=0; i<ways.size(); i++)
		wayPtrs.push_back(&ways[i]);
	std::map<std::string, int32_t> strIndex;

	GenerateStringTable(wayPtrs, wayc, this->encodeMetaData,
		maxWaysToProcess, st, 
		strIndex);
	
	//Write ways in groups
	size_t stopIndex = startWayc+maxWaysToProcess;
	while(wayc < stopIndex)
	{
		size_t waysInGroup = stopIndex - wayc;
		if(waysInGroup > std::max<size_t>(this->maxBlockObjects, 1))
			 waysInGroup = std::max<size_t>(this->maxBlockObjects, 1);

		//Check if all tags are empty
		//TODO

		OSMPBF::PrimitiveGroup *pg = pb.add_primitivegroup();

		for(size_t i=wayc; i<wayc+waysInGroup; i++)
		{
			OSMPBF::Way *ow = pg->add_ways();

			const OsmWay &w = ways[i];
			const TagMap &tags = w.tags;
			ow->set_id(w.objId);
			for(auto it = tags.begin(); it != tags.end(); it++)
			{
				ow->add_keys(strIndex[it->first]);
				ow->add_vals(strIndex[it->second]);
			}

			int64_t refc = 0;
			for(size_t j=0; j<w.refs.size(); j++)
			{
				ow->add_refs(WrapSub(w.refs[j], refc));
				refc = w.refs[j];
			}

			if(this->encodeMetaData)
			{
				OSMPBF::Info *info = ow->mutable_info();

				CheckPbfMetaData(w.metaData);
				info->set_version(w.metaData.version);
				info->set_timestamp(EncodeTimestamp(w.metaData.timestamp, date_granularity));
				info->set_changeset(w.metaData.changeset);
				info->set_uid(w.metaData.uid);
				info->set_user_sid(strIndex[w.metaData.username]);

				if(this->encodeHistorical)
					info->set_visible(w.metaData.visible);
			}
		}

		wayc += waysInGroup;
	}

	pb.SerializeToString(&out);
}

void PbfEncode::EncodePbfWaysSizeLimited(const std::vector<OsmWay> &ways, size_t &wayc, std::string &out)
{
	const size_t startWayc = wayc;
	uint32_t objLimit = std::max<size_t>(this->maxBlockObjects, 1);
	this->EncodePbfWays(ways, wayc, objLimit, out);

	while(out.size() > maxPayloadSize)
	{
		//Payload is too big, so we need to re-encode by limiting number of groups
		objLimit /= 2;
		wayc = startWayc;
		this->EncodePbfWays(ways, wayc, objLimit, out);

		if(out.size() > maxPayloadSize and objLimit <= 1)
			throw runtime_error("Failed to encode ways without breaking maxPayloadSize limit");
	}
}

void PbfEncode::EncodePbfRelations(const std::vector<OsmRelation> &relations, size_t &relationc, size_t maxRelationsToProcess, 
	std::string &out)
{
	//Create string table
	size_t relationsRemain = relations.size()-relationc;
	if(maxRelationsToProcess > relationsRemain)
		maxRelationsToProcess = relationsRemain;
	if(maxRelationsToProcess > std::max<size_t>(this->maxBlockObjects, 1))
		maxRelationsToProcess = std::max<size_t>(this->maxBlockObjects, 1);
	const size_t startRelationc = relationc;

	OSMPBF::PrimitiveBlock pb;
	pb.set_date_granularity(this->date_granularity);

	OSMPBF::StringTable *st = pb.mutable_stringtable();

	std::vector<const OsmObject *> relPtrs;
	for(size_t i=0; i<relations.size(); i++)
		relPtrs.push_back(&relations[i]);
	std::map<std::string, int32_t> strIndex;

	GenerateStringTable(relPtrs, relationc, this->encodeMetaData,
		maxRelationsToProcess, st, 
		strIndex);
	
	//Write relations in groups
	bool groupCountOk = true;
	size_t stopIndex = startRelationc+maxRelationsToProcess;
	while(relationc < stopIndex and groupCountOk)
	{
		size_t relsInGroup = stopIndex - relationc;
		if(relsInGroup > std::max<size_t>(this->maxBlockObjects, 1))
			 relsInGroup = std::max<size_t>(this->maxBlockObjects, 1);

		//Check if all tags are empty
		//TODO

		OSMPBF::PrimitiveGroup *pg = pb.add_primitivegroup();

		for(size_t i=relationc; i<relationc+relsInGroup; i++)
		{
			OSMPBF::Relation *orl = pg->add_relations();

			const OsmRelation &r = relations[i];
			const TagMap &tags = r.tags;
			orl->set_id(r.objId);
			for(auto it = tags.begin(); it != tags.end(); it++)
			{
				orl->add_keys(strIndex[it->first]);
				orl->add_vals(strIndex[it->second]);
			}

			int64_t refc = 0;
			for(size_t j=0; j<r.members.size(); j++)
			{
				const RelationMember &member = r.members[j];
				OSMPBF::Relation_MemberType mt=OSMPBF::Relation_MemberType_NODE;
				if(member.type == ObjectType::Way)
					mt=OSMPBF::Relation_MemberType_WAY;
				else if(member.type == ObjectType::Relation)
					mt=OSMPBF::Relation_MemberType_RELATION;

				orl->add_roles_sid(strIndex[member.role]);
				orl->add_memids(WrapSub(member.ref, refc));
				refc = member.ref;
				orl->add_types(mt);
			}

			if(this->encodeMetaData)
			{
				OSMPBF::Info *info = orl->mutable_info();

				CheckPbfMetaData(r.metaData);
				info->set_version(r.metaData.version);
				info->set_timestamp(EncodeTimestamp(r.metaData.timestamp, date_granularity));
				info->set_changeset(r.metaData.changeset);
				info->set_uid(r.metaData.uid);
				info->set_user_sid(strIndex[r.metaData.username]);

				if(this->encodeHistorical)
					info->set_visible(r.metaData.visible);
			}
		}

		relationc += relsInGroup;
	}

	pb.SerializeToString(&out);
}

void PbfEncode::EncodePbfRelationsSizeLimited(const std::vector<OsmRelation> &relations, size_t &relc, std::string &out)
{
	const size_t startRelc = relc;
	uint32_t objLimit = std::max<size_t>(this->maxBlockObjects, 1);
	this->EncodePbfRelations(relations, relc, objLimit, out);

	while(out.size() > maxPayloadSize)
	{
		//Payload is too big, so we need to re-encode by limiting number of groups
		objLimit /= 2;
		relc = startRelc;
		this->EncodePbfRelations(relations, relc, objLimit, out);

		if(out.size() > maxPayloadSize and objLimit <= 1)
			throw runtime_error("Failed to encode relations without breaking maxPayloadSize limit");
	}
}
