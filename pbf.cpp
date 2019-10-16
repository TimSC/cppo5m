#include "pbf.h"
#include <iostream>
#include <fstream>
#include <sstream>
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

// ******************************************

PbfEncodeBase::PbfEncodeBase()
{

}

PbfEncodeBase::~PbfEncodeBase()
{

}

bool PbfEncodeBase::Sync()
{
	return false;
}

bool PbfEncodeBase::Reset()
{
	return false;
}

bool PbfEncodeBase::Finish()
{
	return false;
}

bool PbfEncodeBase::StoreIsDiff(bool)
{
	return false;
}

bool PbfEncodeBase::StoreBounds(double x1, double y1, double x2, double y2)
{
	return false;
}

bool PbfEncodeBase::StoreNode(int64_t objId, const class MetaData &metaData, 
	const TagMap &tags, double lat, double lon)
{
	return false;
}

bool PbfEncodeBase::StoreWay(int64_t objId, const class MetaData &metaData, 
	const TagMap &tags, const std::vector<int64_t> &refs)
{
	return false;
}

bool PbfEncodeBase::StoreRelation(int64_t objId, const class MetaData &metaData, const TagMap &tags, 
	const std::vector<std::string> &refTypeStrs, const std::vector<int64_t> &refIds, 
	const std::vector<std::string> &refRoles)
{
	return false;
}

void PbfEncodeBase::write (const char* s, std::streamsize n)
{

}

void PbfEncodeBase::operator<< (const std::string &val)
{

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
