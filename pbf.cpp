#include "pbf.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <stdexcept>
#include "pbf/fileformat.pb.h"
#include "pbf/osmformat.pb.h"
#include <arpa/inet.h>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include "OsmData.h"
#include "utils.h"
using namespace std;

// https://stackoverflow.com/questions/27529570/simple-zlib-c-string-compression-and-decompression
std::string CompressData(const std::string &data)
{
    std::stringstream compressed;
    std::stringstream decompressed;
    decompressed << data;
    boost::iostreams::filtering_streambuf<boost::iostreams::input> out;
    out.push(boost::iostreams::zlib_compressor());
    out.push(decompressed);
    boost::iostreams::copy(out, compressed);
    return compressed.str();
}

std::string DecompressData(const std::string &data)
{
    std::stringstream compressed;
    std::stringstream decompressed;
    compressed << data;
    boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
    in.push(boost::iostreams::zlib_decompressor());
    in.push(compressed);
    boost::iostreams::copy(in, decompressed);
    return decompressed.str();
}

void ReadExactLengthPbf(std::istream &str, char *out, size_t len)
{
	size_t total = 0;
	while(total < len)
	{
		str.read(&out[total], len-total);
		total += str.gcount();
		//std::cout << "Read " << total << " of " << len << std::endl;
		if(str.fail())
			throw std::runtime_error("Input underflow");
	}
}

bool DecodeOsmHeader(std::string &decBlob,
	std::shared_ptr<class IDataStreamHandler> output)
{
	OSMPBF::HeaderBlock hb;
	std::istringstream iss(decBlob);
	bool ok = hb.ParseFromIstream(&iss);
	if(!ok)
		throw runtime_error("Error decoding PBF HeaderBlock");

	if(hb.has_bbox())
	{
		const OSMPBF::HeaderBBox &bbox = hb.bbox();
		
		if(bbox.has_left() and bbox.has_right() and bbox.has_top() and bbox.has_bottom())
		{
			bool halt = false;
			if(output)
				output->StoreBounds(bbox.left()*1e-9, bbox.bottom()*1e-9, bbox.right()*1e-9, bbox.top()*1e-9);
			if(halt) return true;
		}
	}
	return false;
}

void DecodeOsmInfo(const OSMPBF::Info &info, int32_t date_granularity, const std::vector<std::string> &stringTab, class MetaData &out)
{
	if(info.has_version())
		out.version = info.version();
	if(info.has_timestamp())
		out.timestamp = info.timestamp()*date_granularity / 1000;
	if(info.has_changeset())
		out.changeset = info.changeset();
	if(info.has_uid())
		out.uid = info.uid();
	if(info.has_user_sid() and info.user_sid() > 0 and info.user_sid() < stringTab.size())
		out.username = stringTab[info.user_sid()];
	if(info.has_visible())
		out.visible = info.visible();
}

bool DecodeOsmNodes(const OSMPBF::PrimitiveGroup& pg,
	int64_t lat_offset, int64_t lon_offset,
	int32_t granularity, 
	int32_t date_granularity,
	const std::vector<std::string> &stringTab,
	std::shared_ptr<class IDataStreamHandler> output)
{
	for(int i=0; i<pg.nodes_size(); i++)
	{
		const OSMPBF::Node &node = pg.nodes(i);
		class MetaData metaData;
		TagMap tags;
		
		for(int j=0; j<node.keys_size() and j<node.vals_size(); j++)
		{
			uint32_t keyIndex = node.keys(j);
			uint32_t valIndex = node.vals(j);
			if(keyIndex > 0 and keyIndex < stringTab.size() and valIndex > 0 and valIndex < stringTab.size())
				tags[stringTab[keyIndex]] = stringTab[valIndex];
		}

		if(node.has_info())
		{
			const OSMPBF::Info &info = node.info();
			DecodeOsmInfo(info, date_granularity, stringTab, metaData);
		}

		bool halt = false;
		if(output)
			output->StoreNode(node.id(), metaData, 
				tags, 
				1e-9 * (lat_offset + (granularity * node.lat())), 
				1e-9 * (lon_offset + (granularity * node.lon())));
		if(halt)
			return true;
	}
	return false;
}

bool DecodeOsmDenseNodes(const OSMPBF::DenseNodes &dense,
	int64_t lat_offset, int64_t lon_offset,
	int32_t granularity, int32_t date_granularity,
	const std::vector<std::string> &stringTab,
	std::shared_ptr<class IDataStreamHandler> output)
{
	//Decode tags
	vector<map<string, string> > tags;
	map<string, string> current;
	for(int j=0; j<dense.keys_vals_size(); j++)
	{
		int32_t sti = dense.keys_vals(j);
		if(sti > 0 and sti < stringTab.size() and j<dense.keys_vals_size()-1)
		{
			int32_t sti2 = dense.keys_vals(j+1);
			current[stringTab[sti]] = stringTab[sti2];
			j++;
		}
		else
		{
			tags.push_back(current);
			current.clear();
		}
	}

	int64_t idc = 0, latc = 0, lonc = 0, timestampc = 0, changesetc = 0;
	int32_t uidc = 0, user_sidc = 0;
	for(int j=0; j<dense.id_size() and j<dense.lat_size() and j<dense.lon_size(); j++)
	{
		idc += dense.id(j);
		latc += dense.lat(j);
		lonc += dense.lon(j);

		class MetaData metaData;
		TagMap empty;
		const TagMap *tagMapPtr = &empty;
		if(j < tags.size())
			tagMapPtr = &tags[j];
		
		if(dense.has_denseinfo())
		{
			const OSMPBF::DenseInfo &di = dense.denseinfo();
			if(j < di.version_size())
				metaData.version = di.version(j);

			if(j < di.timestamp_size())
			{
				timestampc += di.timestamp(j);
				metaData.timestamp = timestampc*date_granularity / 1000;
			}

			if(j < di.changeset_size())
			{
				changesetc += di.changeset(j);
				metaData.changeset = changesetc;
			}

			if(j < di.uid_size())
			{
				uidc += di.uid(j);
				metaData.uid = uidc;
			}

			if(j < di.user_sid_size())
			{
				user_sidc += di.user_sid(j);
				if(user_sidc > 0 and user_sidc <= stringTab.size())
					metaData.username = stringTab[user_sidc];
			}

			if(j < di.visible_size())
				metaData.visible = di.visible(j);

		}

		bool halt = false;
		if(output)
			output->StoreNode(idc, metaData, 
				*tagMapPtr, 
				1e-9 * (lat_offset + (granularity * latc)), 
				1e-9 * (lon_offset + (granularity * lonc)));
		if(halt)
			return true;
	}
	return false;
}

bool DecodeOsmWays(const OSMPBF::PrimitiveGroup& pg,
	int32_t date_granularity,
	const std::vector<std::string> &stringTab,
	std::shared_ptr<class IDataStreamHandler> output)
{
	for(int i=0; i<pg.ways_size(); i++)
	{
		const OSMPBF::Way &way = pg.ways(i);
		class MetaData metaData;
		TagMap tags;
		std::vector<int64_t> refs;
		
		for(int j=0; j<way.keys_size() and j<way.vals_size(); j++)
		{
			uint32_t keyIndex = way.keys(j);
			uint32_t valIndex = way.vals(j);
			if(keyIndex > 0 and keyIndex < stringTab.size() and valIndex > 0 and valIndex < stringTab.size())
				tags[stringTab[keyIndex]] = stringTab[valIndex];
		}

		if(way.has_info())
		{
			const OSMPBF::Info &info = way.info();
			DecodeOsmInfo(info, date_granularity, stringTab, metaData);
		}

		int64_t refsc = 0;
		for(int j=0; j<way.refs_size(); j++)
		{
			refsc += way.refs(j);
			refs.push_back(refsc);
		}
		
		bool halt = false;
		if(output)
			output->StoreWay(way.id(), metaData, 
				tags, refs);
		if(halt)
			return true;
	}
	return false;
}

bool DecodeOsmRelations(const OSMPBF::PrimitiveGroup& pg,
	int32_t date_granularity,
	const std::vector<std::string> &stringTab,
	std::shared_ptr<class IDataStreamHandler> output)
{
	for(int i=0; i<pg.relations_size(); i++)
	{
		const OSMPBF::Relation &relation = pg.relations(i);
		class MetaData metaData;
		TagMap tags;
		std::vector<std::string> refTypeStrs;
		std::vector<int64_t> refIds;
		std::vector<std::string> refRoles;
		
		for(int j=0; j<relation.keys_size() and j<relation.vals_size(); j++)
		{
			uint32_t keyIndex = relation.keys(j);
			uint32_t valIndex = relation.vals(j);
			if(keyIndex > 0 and keyIndex < stringTab.size() and valIndex > 0 and valIndex < stringTab.size())
				tags[stringTab[keyIndex]] = stringTab[valIndex];
		}

		if(relation.has_info())
		{
			const OSMPBF::Info &info = relation.info();
			DecodeOsmInfo(info, date_granularity, stringTab, metaData);
		}

		int64_t memidsc = 0;
		for(int j=0; j<relation.memids_size() and j<relation.types_size() and j<relation.roles_sid_size(); j++)
		{
			memidsc += relation.memids(j);
			switch(relation.types(j))
			{
				case OSMPBF::Relation_MemberType_NODE:
					refTypeStrs.push_back("node");
					break;
				case OSMPBF::Relation_MemberType_WAY:
					refTypeStrs.push_back("way");
					break;
				case OSMPBF::Relation_MemberType_RELATION:
					refTypeStrs.push_back("relation");
					break;
				default:
					refTypeStrs.push_back("");
					break;
			}
			refIds.push_back(memidsc);
			int32_t roleIndex = relation.roles_sid(j);
			if(roleIndex > 0 and roleIndex < stringTab.size())
				refRoles.push_back(stringTab[roleIndex]);
			else
				refRoles.push_back("");
		}
		
		bool halt = false;
		if(output)
			output->StoreRelation(relation.id(), metaData, 
				tags, refTypeStrs, refIds, refRoles);
		if(halt)
			return true;	
	}
	return false;
}

// ********************************************

PbfDecode::PbfDecode(std::streambuf &handleIn):
	handle(&handleIn)
{

}

PbfDecode::~PbfDecode()
{

}

bool PbfDecode::DecodeNext()
{
	int32_t blobHeaderLenNbo;
	ReadExactLengthPbf(handle, (char*)&blobHeaderLenNbo, sizeof(int32_t));
	int32_t blobHeaderLen = ntohl(blobHeaderLenNbo);

	std::string refData;
	refData.resize(blobHeaderLen);
	ReadExactLengthPbf(handle, &refData[0], blobHeaderLen);

	OSMPBF::BlobHeader header;
	std::istringstream iss(refData);
	bool ok = header.ParseFromIstream(&iss);
	if(!ok)
		throw runtime_error("Error decoding PBF BlobHeader");

	std::string headerType = header.type();

	int32_t blobSize = header.datasize();
	std::string refData2;
	refData2.resize(blobSize);
	ReadExactLengthPbf(handle, &refData2[0], blobSize);

	OSMPBF::Blob blob;
	std::istringstream iss2(refData2);
	ok = blob.ParseFromIstream(&iss2);
	if(!ok)
		throw runtime_error("Error decoding PBF Blob");

	string decBlob;
	if(blob.has_raw())
		decBlob = blob.raw();
	else if(blob.has_zlib_data())
		decBlob = DecompressData(blob.zlib_data());

	bool halt = false;
	if(headerType == "OSMHeader")
		halt = DecodeOsmHeader(decBlob, this->output);

	else if(headerType == "OSMData")
		halt = DecodeOsmData(decBlob);

	if(halt) return false;
	return true;
}

void PbfDecode::DecodeFinish()
{
	if(this->output)
		this->output->Finish();
}

bool PbfDecode::DecodeOsmData(std::string &decBlob)
{
	OSMPBF::PrimitiveBlock pb;
	std::istringstream iss(decBlob);
	bool ok = pb.ParseFromIstream(&iss);
	if(!ok)
		throw runtime_error("Error decoding PBF PrimitiveBlock");

	std::vector<std::string> stringTab;
	if(pb.has_stringtable())
	{
		const OSMPBF::StringTable &st = pb.stringtable();
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
		lon_offset = pb.date_granularity();

	int pbs = pb.primitivegroup_size();
	for(int i=0; i<pbs; i++)
	{
		bool halt = false;
		const OSMPBF::PrimitiveGroup& pg = pb.primitivegroup(i);

		if(pg.nodes_size() > 0)
		{
			halt |= CheckOutputType("n");
			halt |= DecodeOsmNodes(pg, lat_offset, lon_offset,
				granularity, date_granularity,
				stringTab, this->output);	
		}

		if(pg.has_dense())
		{
			halt |= CheckOutputType("n");
			const OSMPBF::DenseNodes &dense = pg.dense();
			halt |= DecodeOsmDenseNodes(dense, lat_offset, lon_offset,
				granularity, date_granularity,
				stringTab, this->output);
		}

		if(pg.ways_size() > 0)
		{
			halt |= CheckOutputType("w");
			halt |= DecodeOsmWays(pg, date_granularity,
				stringTab, this->output);
		}

		if(pg.relations_size() > 0)
		{
			halt |= CheckOutputType("r");
			halt |= DecodeOsmRelations(pg, date_granularity,
				stringTab, this->output);
		}

		//Changeset decoding not supported

		if(halt)
			return true;
	}
	return false;
}

bool PbfDecode::CheckOutputType(const char *objType)
{
	//Some encoders need to know when we switch object type
	bool halt = false;
	if(prevObjType != objType and prevObjType.length() > 0)		
		if(output)
		{
			halt |= output->Sync();
			halt |= output->Reset();
		}

	prevObjType = objType;
	return halt;
}

// *******************************************


void GenerateStringTable(const std::vector<const class OsmObject *> &ways, size_t startc, 
	bool encodeMetaData, 
	size_t &maxWaysToProcess, OSMPBF::StringTable *st, 
	std::map<std::string, int32_t> &strIndexOut)
{
	std::map<std::string, std::uint32_t> strFreq;
	size_t processed = 0;
	for(size_t i=startc; i<startc + maxWaysToProcess; i++)
	{
		const class OsmObject &n = *(ways[i]);
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

		const class OsmRelation *rel = dynamic_cast<const class OsmRelation *>(&n);
		if(rel != nullptr)
		{
			const std::vector<std::string> &refRoles = rel->refRoles;
			for(size_t j=0; j<refRoles.size(); j++)
			{
				auto it2 = strFreq.find(refRoles[j]);
				if(it2 != strFreq.end())
					it2->second ++;
				else
					strFreq[refRoles[j]] = 1;
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

PbfEncodeBase::PbfEncodeBase()
{
	encodeMetaData = true;
	encodeHistorical = false;
	maxGroupObjects = 8000;
	headerWritten = false;
	compressUsingZLib = true;
	writingProgram = "cppo5m";
	maxPayloadSize = 32 * 1024 * 1024;
	maxHeaderSize = 64 * 1024;
	optimalDenseNodes = 1200000;
	optimalWays = 1000000;
	optimalRelations = 100000;
	lat_offset = 0;
	lon_offset = 0;
	granularity = 100;
	date_granularity=1000;
}

PbfEncodeBase::~PbfEncodeBase()
{

}

bool PbfEncodeBase::Sync()
{
	EncodeBuffer();
	return false;
}

bool PbfEncodeBase::Reset()
{
	return false;
}

bool PbfEncodeBase::Finish()
{
	this->EncodeBuffer();
	return false;
}

bool PbfEncodeBase::StoreIsDiff(bool)
{
	return false;
}

bool PbfEncodeBase::StoreBounds(double x1, double y1, double x2, double y2)
{
	buffer.StoreBounds(x1, y1, x2, y2);
	return false;
}

bool PbfEncodeBase::StoreNode(int64_t objId, const class MetaData &metaData, 
	const TagMap &tags, double lat, double lon)
{
	if(prevObjType != "n" and prevObjType.size() > 0)
		this->EncodeBuffer();

	if(buffer.nodes.size() >= optimalDenseNodes)
		this->EncodeBuffer();

	buffer.StoreNode(objId, metaData, tags, lat, lon);

	prevObjType = "n";
	return false;
}

bool PbfEncodeBase::StoreWay(int64_t objId, const class MetaData &metaData, 
	const TagMap &tags, const std::vector<int64_t> &refs)
{
	if(prevObjType != "w" and prevObjType.size() > 0)
	{
		this->EncodeBuffer();
	}

	buffer.StoreWay(objId, metaData, tags, refs);

	prevObjType = "w";
	return false;
}

bool PbfEncodeBase::StoreRelation(int64_t objId, const class MetaData &metaData, const TagMap &tags, 
	const std::vector<std::string> &refTypeStrs, const std::vector<int64_t> &refIds, 
	const std::vector<std::string> &refRoles)
{
	if(prevObjType != "r" and prevObjType.size() > 0)
	{
		this->EncodeBuffer();
	}

	buffer.StoreRelation(objId, metaData, tags, refTypeStrs, refIds, refRoles);

	prevObjType = "r";
	return false;
}

void PbfEncodeBase::write (const char* s, std::streamsize n)
{

}

void PbfEncodeBase::operator<< (const std::string &val)
{

}

void PbfEncodeBase::WriteBlobPayload(const std::string &blobPayload, const char *type)
{
	cout << type << "," << blobPayload.size() << endl;
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
	this->write ((char*)&headerSizePk, sizeof(int32_t));
	this->write (packedHeader.c_str(), packedHeader.size());
	this->write (packedBlob.c_str(), packedBlob.size());
}

void PbfEncodeBase::EncodeBuffer()
{
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
		std::string errStr = "If you're seeing this, the code is in what I thought was an unreachable state.";
		errStr += " On a deep level, I know I'm not up to this task. I'm so sorry.";
		throw logic_error(errStr);
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

void PbfEncodeBase::EncodeHeaderBlock(std::string &out)
{
	OSMPBF::HeaderBlock hb;

	hb.add_required_features("OsmSchema-V0.6");
	hb.add_required_features("DenseNodes");
	if(this->encodeHistorical)
		hb.add_required_features("HistoricalInformation");
	hb.set_writingprogram(this->writingProgram);

	if(buffer.bounds.size() > 0 and buffer.bounds[0].size() >= 4)
	{
		OSMPBF::HeaderBBox *bbox = hb.mutable_bbox();
		std::vector<double> srcBbox = buffer.bounds[0]; //Only the first bbox is considered

		bbox->set_left(std::round(srcBbox[0] / 1e-9));
		bbox->set_bottom(std::round(srcBbox[1] / 1e-9));
		bbox->set_right(std::round(srcBbox[2] / 1e-9));
		bbox->set_top(std::round(srcBbox[3] / 1e-9));
	}

	hb.SerializeToString(&out);

	if(out.size() > this->maxHeaderSize)
		throw runtime_error("HeaderBlock size exceeds what PBF allows");
}

void PbfEncodeBase::EncodePbfDenseNodes(const std::vector<class OsmNode> &nodes, size_t &nodec, uint32_t limitGroups, 
	std::string &out, uint32_t &countGroupsOut)
{
	//Create string table
	size_t maxNodesToProcess = nodes.size()-nodec;
	if(maxNodesToProcess > this->optimalDenseNodes)
		maxNodesToProcess = this->optimalDenseNodes;
	const size_t startNodec = nodec;

	OSMPBF::PrimitiveBlock pb;
	pb.set_granularity(this->granularity);
	pb.set_lat_offset(this->lat_offset);
	pb.set_lon_offset(this->lon_offset);
	pb.set_date_granularity(this->date_granularity);

	OSMPBF::StringTable *st = pb.mutable_stringtable();

	std::vector<const class OsmObject *> nodePtrs;
	for(size_t i=0; i<nodes.size(); i++)
		nodePtrs.push_back(&nodes[i]);
	std::map<std::string, int32_t> strIndex;

	GenerateStringTable(nodePtrs, nodec, this->encodeMetaData,
		maxNodesToProcess, st, 
		strIndex);

	//Write nodes in groups
	bool groupCountOk = true;
	size_t stopIndex = startNodec+maxNodesToProcess;
	countGroupsOut = 0;
	while(nodec < stopIndex and groupCountOk)
	{
		size_t nodesInGroup = stopIndex - nodec;
		if(nodesInGroup > maxGroupObjects)
			 nodesInGroup = maxGroupObjects;

		//Check if all tags are empty
		//TODO

		OSMPBF::PrimitiveGroup *pg = pb.add_primitivegroup();
		OSMPBF::DenseNodes *dn = pg->mutable_dense();
		OSMPBF::DenseInfo *di = nullptr; 
		if(this->encodeMetaData)
			di = dn->mutable_denseinfo();

		int64_t idc = 0, latc = 0, lonc = 0, timestampc = 0, changesetc = 0;
		int32_t uidc = 0, user_sidc = 0;
		for(size_t i=nodec; i<nodec+nodesInGroup; i++)
		{
			const class OsmNode &n = nodes[i];
			dn->add_id(n.objId-idc);
			idc = n.objId;
			int64_t lati = std::round((n.lat / 1e-9) - lat_offset) / granularity;
			dn->add_lat(lati - latc);
			latc = lati;
			int64_t loni = std::round((n.lon / 1e-9) - lon_offset) / granularity;
			dn->add_lon(loni - lonc);
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
				di->add_version(n.metaData.version);
				int64_t ts = n.metaData.timestamp * 1000 / date_granularity;
				di->add_timestamp(ts - timestampc);
				timestampc = ts;
				di->add_changeset(n.metaData.changeset - changesetc);
				changesetc = n.metaData.changeset;
				di->add_uid(n.metaData.uid - uidc);
				int32_t si = strIndex[n.metaData.username];
				di->add_user_sid(si-user_sidc);
				user_sidc = si;

				if(this->encodeHistorical)
					di->add_visible(n.metaData.visible);
			}
		}

		countGroupsOut ++;
		nodec += nodesInGroup;
		if(limitGroups > 0 and countGroupsOut >= limitGroups)
			groupCountOk = false;
	}

	pb.SerializeToString(&out);
}

void PbfEncodeBase::EncodePbfDenseNodesSizeLimited(const std::vector<class OsmNode> &nodes, size_t &nodec, std::string &out)
{
	const size_t startNodec = nodec;
	uint32_t countGroups = 0;
	this->EncodePbfDenseNodes(nodes, nodec, 0, out, countGroups);

	uint32_t groupLimit = countGroups;
	while(out.size() > maxPayloadSize)
	{
		//Payload is too big, so we need to re-encode by limiting number of groups
		groupLimit /= 2;
		nodec = startNodec;
		this->EncodePbfDenseNodes(nodes, nodec, groupLimit, out, countGroups);

		if(out.size() > maxPayloadSize and groupLimit <= 1)
			throw runtime_error("Failed to encode nodes without breaking maxPayloadSize limit");
	}
}

void PbfEncodeBase::EncodePbfWays(const std::vector<class OsmWay> &ways, size_t &wayc, uint32_t limitGroups, 
	std::string &out, uint32_t &countGroupsOut)
{
	//Create string table
	size_t maxWaysToProcess = ways.size()-wayc;
	if(maxWaysToProcess > this->optimalWays)
		maxWaysToProcess = this->optimalWays;
	const size_t startWayc = wayc;

	OSMPBF::PrimitiveBlock pb;
	pb.set_date_granularity(this->date_granularity);

	OSMPBF::StringTable *st = pb.mutable_stringtable();

	std::vector<const class OsmObject *> wayPtrs;
	for(size_t i=0; i<ways.size(); i++)
		wayPtrs.push_back(&ways[i]);
	std::map<std::string, int32_t> strIndex;

	GenerateStringTable(wayPtrs, wayc, this->encodeMetaData,
		maxWaysToProcess, st, 
		strIndex);
	
	//Write ways in groups
	bool groupCountOk = true;
	size_t stopIndex = startWayc+maxWaysToProcess;
	countGroupsOut = 0;
	while(wayc < stopIndex and groupCountOk)
	{
		size_t waysInGroup = stopIndex - wayc;
		if(waysInGroup > maxGroupObjects)
			 waysInGroup = maxGroupObjects;

		//Check if all tags are empty
		//TODO

		OSMPBF::PrimitiveGroup *pg = pb.add_primitivegroup();

		for(size_t i=wayc; i<wayc+waysInGroup; i++)
		{
			OSMPBF::Way *ow = pg->add_ways();

			const class OsmWay &w = ways[i];
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
				ow->add_refs(w.refs[j]-refc);
				refc = w.refs[j];
			}

			if(this->encodeMetaData)
			{
				OSMPBF::Info *info = ow->mutable_info();

				info->set_version(w.metaData.version);
				info->set_timestamp(w.metaData.timestamp * 1000 / date_granularity);
				info->set_changeset(w.metaData.changeset);
				info->set_uid(w.metaData.uid);
				info->set_user_sid(strIndex[w.metaData.username]);

				if(this->encodeHistorical)
					info->set_visible(w.metaData.visible);
			}
		}

		countGroupsOut ++;
		wayc += waysInGroup;
		if(limitGroups > 0 and countGroupsOut >= limitGroups)
			groupCountOk = false;
	}

	pb.SerializeToString(&out);
}

void PbfEncodeBase::EncodePbfWaysSizeLimited(const std::vector<class OsmWay> &ways, size_t &wayc, std::string &out)
{
	const size_t startWayc = wayc;
	uint32_t countGroups = 0;
	this->EncodePbfWays(ways, wayc, 0, out, countGroups);

	uint32_t groupLimit = countGroups;
	while(out.size() > maxPayloadSize)
	{
		//Payload is too big, so we need to re-encode by limiting number of groups
		groupLimit /= 2;
		wayc = startWayc;
		this->EncodePbfWays(ways, wayc, groupLimit, out, countGroups);

		if(out.size() > maxPayloadSize and groupLimit <= 1)
			throw runtime_error("Failed to encode ways without breaking maxPayloadSize limit");
	}
}

void PbfEncodeBase::EncodePbfRelations(const std::vector<class OsmRelation> &relations, size_t &relationc, uint32_t limitGroups, 
	std::string &out, uint32_t &countGroupsOut)
{
	//Create string table
	size_t maxRelationsToProcess = relations.size()-relationc;
	if(maxRelationsToProcess > this->optimalRelations)
		maxRelationsToProcess = this->optimalRelations;
	const size_t startRelationc = relationc;

	OSMPBF::PrimitiveBlock pb;
	pb.set_date_granularity(this->date_granularity);

	OSMPBF::StringTable *st = pb.mutable_stringtable();

	std::vector<const class OsmObject *> relPtrs;
	for(size_t i=0; i<relations.size(); i++)
		relPtrs.push_back(&relations[i]);
	std::map<std::string, int32_t> strIndex;

	GenerateStringTable(relPtrs, relationc, this->encodeMetaData,
		maxRelationsToProcess, st, 
		strIndex);
	
	//Write relations in groups
	bool groupCountOk = true;
	size_t stopIndex = startRelationc+maxRelationsToProcess;
	countGroupsOut = 0;
	while(relationc < stopIndex and groupCountOk)
	{
		size_t relsInGroup = stopIndex - relationc;
		if(relsInGroup > maxGroupObjects)
			 relsInGroup = maxGroupObjects;

		//Check if all tags are empty
		//TODO

		OSMPBF::PrimitiveGroup *pg = pb.add_primitivegroup();

		for(size_t i=relationc; i<relationc+relsInGroup; i++)
		{
			OSMPBF::Relation *orl = pg->add_relations();

			const class OsmRelation &r = relations[i];
			const TagMap &tags = r.tags;
			orl->set_id(r.objId);
			for(auto it = tags.begin(); it != tags.end(); it++)
			{
				orl->add_keys(strIndex[it->first]);
				orl->add_vals(strIndex[it->second]);
			}

			int64_t refc = 0;
			for(size_t j=0; j<r.refTypeStrs.size() and j<r.refIds.size() and j<r.refRoles.size(); j++)
			{
				OSMPBF::Relation_MemberType mt=OSMPBF::Relation_MemberType_NODE;
				if(r.refTypeStrs[j]=="way")
					mt=OSMPBF::Relation_MemberType_WAY;
				else if (r.refTypeStrs[j]=="relation")
					mt=OSMPBF::Relation_MemberType_RELATION;
				else if (r.refTypeStrs[j]!="node")
					continue;

				orl->add_roles_sid(strIndex[r.refRoles[j]]);
				orl->add_memids(r.refIds[j]-refc);
				refc = r.refIds[j];
				orl->add_types(mt);
			}

			if(this->encodeMetaData)
			{
				OSMPBF::Info *info = orl->mutable_info();

				info->set_version(r.metaData.version);
				info->set_timestamp(r.metaData.timestamp * 1000 / date_granularity);
				info->set_changeset(r.metaData.changeset);
				info->set_uid(r.metaData.uid);
				info->set_user_sid(strIndex[r.metaData.username]);

				if(this->encodeHistorical)
					info->set_visible(r.metaData.visible);
			}
		}

		countGroupsOut ++;
		relationc += relsInGroup;
		if(limitGroups > 0 and countGroupsOut >= limitGroups)
			groupCountOk = false;
	}

	pb.SerializeToString(&out);
}

void PbfEncodeBase::EncodePbfRelationsSizeLimited(const std::vector<class OsmRelation> &relations, size_t &relc, std::string &out)
{
	const size_t startRelc = relc;
	uint32_t countGroups = 0;
	this->EncodePbfRelations(relations, relc, 0, out, countGroups);

	uint32_t groupLimit = countGroups;
	while(out.size() > maxPayloadSize)
	{
		//Payload is too big, so we need to re-encode by limiting number of groups
		groupLimit /= 2;
		relc = startRelc;
		this->EncodePbfRelations(relations, relc, groupLimit, out, countGroups);

		if(out.size() > maxPayloadSize and groupLimit <= 1)
			throw runtime_error("Failed to encode relations without breaking maxPayloadSize limit");
	}
}

// *************************************

PbfEncode::PbfEncode(std::streambuf &handleIn): PbfEncodeBase(), handle(&handleIn)
{

}

PbfEncode::~PbfEncode()
{

}

//*******************************************

#ifdef PYTHON_AWARE

PyPbfEncode::PyPbfEncode(PyObject* obj): PbfEncodeBase()
{
	m_PyObj = obj;
	Py_INCREF(m_PyObj);
	m_Write = PyObject_GetAttrString(m_PyObj, "write");
	this->WriteStart();
}

PyPbfEncode::~PyPbfEncode()
{
	Py_XDECREF(m_Write);
	Py_XDECREF(m_PyObj);
}

void PyPbfEncode::write (const char* s, std::streamsize n)
{
	if(this->m_Write == NULL)
		return;
	#if PY_MAJOR_VERSION < 3
	PyObject* ret = PyObject_CallFunction(m_Write, (char *)"s#", s, n);
	#else
	PyObject* ret = PyObject_CallFunction(m_Write, (char *)"y#", s, n);
	#endif 
	Py_XDECREF(ret);
}

void PyPbfEncode::operator<< (const std::string &val)
{
	if(this->m_Write == NULL)
		return;
	#if PY_MAJOR_VERSION < 3
	PyObject* ret = PyObject_CallFunction(m_Write, (char *)"s#", val.c_str(), val.length());
	#else
	PyObject* ret = PyObject_CallFunction(m_Write, (char *)"y#", val.c_str(), val.length());
	#endif 
	Py_XDECREF(ret);
}

#endif //PYTHON_AWARE
