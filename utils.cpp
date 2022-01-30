#include "utils.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include "o5m.h"
#include "osmxml.h"
#include "pbf.h"
using namespace std;

// ******* Utility funcs **********

void LoadFromO5m(std::streambuf &fi, class IDataStreamHandler* output)
{
	class O5mDecode dec(fi);
	LoadFromDecoder(fi, &dec, output);
}

void LoadFromOsmXml(std::streambuf &fi, class IDataStreamHandler* output)
{
	class OsmXmlDecode dec(fi);
	LoadFromDecoder(fi, &dec, output);
}

void LoadFromPbf(std::streambuf &fi, class IDataStreamHandler* output)
{
	class PbfDecode pbfDecode(fi);
	LoadFromDecoder(fi, &pbfDecode, output);
}

void LoadFromOsmChangeXml(std::streambuf &fi, class IOsmChangeBlock* output)
{
	class OsmChangeXmlDecode dec(fi);
	dec.output = output;
	dec.DecodeHeader();

	while (fi.in_avail()>0)
	{
		bool ok = dec.DecodeNext();
		if(!ok)
		{
			cout << dec.errString << endl;
			break;
		}
	}

	dec.DecodeFinish();
}

void LoadFromDecoder(std::streambuf &fi, class OsmDecoder *osmDecoder, class IDataStreamHandler *output)
{
	osmDecoder->output = output;
	osmDecoder->DecodeHeader();

	while (fi.in_avail()>0)
	{
		bool ok = osmDecoder->DecodeNext();
		if(!ok)
		{
			cout << osmDecoder->errString << endl;
			break;
		}
	}

	osmDecoder->DecodeFinish();
	output->Finish();
}

// **********************************************************

void SaveToO5m(const class OsmData &osmData, std::streambuf &fi)
{
	class O5mEncode enc(fi);
	osmData.StreamTo(enc);
}

void SaveToOsmXml(const class OsmData &osmData, std::streambuf &fi)
{
	TagMap empty;
	class OsmXmlEncode enc(fi, empty);
	osmData.StreamTo(enc);
}

void SaveToOsmChangeXml(const class OsmChange &osmChange, bool separateActions, std::streambuf &fi)
{
	TagMap empty;
	class OsmChangeXmlEncode enc(fi, empty, separateActions);
	enc.Encode(osmChange);
	enc.Finish();
}

void LoadFromO5m(const std::string &fi, class IDataStreamHandler *output)
{
	std::istringstream buff(fi);
	LoadFromO5m(*buff.rdbuf(), output);
}

void LoadFromOsmXml(const std::string &fi, class IDataStreamHandler *output)
{
	std::istringstream buff(fi);
	LoadFromOsmXml(*buff.rdbuf(), output);
}

void LoadFromOsmChangeXml(const std::string &fi, class IOsmChangeBlock *output)
{
	std::istringstream buff(fi);
	LoadFromOsmChangeXml(*buff.rdbuf(), output);
}

// ************************************************

std::shared_ptr<class OsmDecoder> DecoderOsmFactory(std::streambuf &handleIn, const std::string &filename)
{
	if(filename.size() < 4)
		invalid_argument("Extension not recognised"); 

	string last4 = filename.substr(filename.size()-4);

	if(last4 == ".o5m")
	{
		std::shared_ptr<class OsmDecoder> dec = make_shared<O5mDecode>(handleIn);
		return dec;
	}

	if(last4 == ".osm")
	{
		std::shared_ptr<class OsmDecoder> dec = make_shared<OsmXmlDecode>(handleIn);
		return dec;
	}

	if(last4 == ".pbf")
	{
		std::shared_ptr<class OsmDecoder> dec = make_shared<PbfDecode>(handleIn);
		return dec;
	}

	throw invalid_argument("Extension not recognised"); 
}

// ************************************************

FindBbox::FindBbox() : IDataStreamHandler()
{
	bboxFound = false;
	x1 = 0.0;
	y1 = 0.0;
	x2 = 0.0;
	y2 = 0.0;
}

FindBbox::~FindBbox()
{

}

bool FindBbox::StoreIsDiff(bool)
{
	return false;
}

bool FindBbox::StoreBounds(double x1, double y1, double x2, double y2)
{
	this->bboxFound = true;
	this->x1 = x1;
	this->y1 = y1;
	this->x2 = x2;
	this->y2 = y2;
	return false;
}

bool FindBbox::StoreNode(int64_t objId, const class MetaData &metaData, 
	const TagMap &tags, double lat, double lon)
{
	return true; //Stop processing
}

bool FindBbox::StoreWay(int64_t objId, const class MetaData &metaData, 
	const TagMap &tags, const std::vector<int64_t> &refs)
{
	return true; //Stop processing
}

bool FindBbox::StoreRelation(int64_t objId, const class MetaData &metaData, const TagMap &tags, 
	const std::vector<std::string> &refTypeStrs, const std::vector<int64_t> &refIds, 
	const std::vector<std::string> &refRoles)
{
	return true; //Stop processing
}

// ******************************************************

DeduplicateOsm::DeduplicateOsm(class IDataStreamHandler &out) : IDataStreamHandler(),
	out(out)
{

}

DeduplicateOsm::~DeduplicateOsm()
{

}

bool DeduplicateOsm::StoreIsDiff(bool isDiff)
{
	return out.StoreIsDiff(isDiff);
}

bool DeduplicateOsm::StoreBounds(double x1, double y1, double x2, double y2)
{
	return out.StoreBounds(x1, y1, x2, y2);
}

bool DeduplicateOsm::StoreNode(int64_t objId, const class MetaData &metaData, 
	const TagMap &tags, double lat, double lon)
{
	auto it = nodeIds.find(objId);
	if(it != nodeIds.end())
		return false;
	nodeIds.insert(objId);

	return out.StoreNode(objId, metaData, tags, lat, lon);
}

bool DeduplicateOsm::StoreWay(int64_t objId, const class MetaData &metaData, 
	const TagMap &tags, const std::vector<int64_t> &refs)
{
	auto it = wayIds.find(objId);
	if(it != wayIds.end())
		return false;
	wayIds.insert(objId);

	return out.StoreWay(objId, metaData, tags, refs);
}

bool DeduplicateOsm::StoreRelation(int64_t objId, const class MetaData &metaData, const TagMap &tags, 
	const std::vector<std::string> &refTypeStrs, const std::vector<int64_t> &refIds, 
	const std::vector<std::string> &refRoles)
{
	auto it = relationIds.find(objId);
	if(it != relationIds.end())
		return false;
	relationIds.insert(objId);

	return out.StoreRelation(objId, metaData, tags, refTypeStrs, refIds, refRoles);
}

void DeduplicateOsm::ResetExisting()
{
	nodeIds.clear();
	wayIds.clear();
	relationIds.clear();
}

// *******************************************************************

bool CompareObjsById (class OsmObject *i, class OsmObject *j) { return (i->objId<j->objId); }

OsmFilterRenumber::OsmFilterRenumber(shared_ptr<class IDataStreamHandler> out): out(out)
{

}

OsmFilterRenumber::~OsmFilterRenumber()
{

}

bool OsmFilterRenumber::Finish()
{
	//Sort nodes
	std::vector<class OsmObject *> ptrs;
	for(size_t i=0; i<nodes.size(); i++)
		ptrs.push_back(&nodes[i]);
	std::sort(ptrs.begin(), ptrs.end(), CompareObjsById);
	std::vector<class OsmNode> nodesCpy;
	for(size_t i=0; i<ptrs.size(); i++)
		nodesCpy.push_back(*static_cast<class OsmNode *>(ptrs[i]));
	nodes = nodesCpy;

	//Sort ways
	ptrs.clear();
	for(size_t i=0; i<ways.size(); i++)
		ptrs.push_back(&ways[i]);
	std::sort(ptrs.begin(), ptrs.end(), CompareObjsById);
	std::vector<class OsmWay> waysCpy;
	for(size_t i=0; i<ptrs.size(); i++)
		waysCpy.push_back(*static_cast<class OsmWay *>(ptrs[i]));
	ways = waysCpy;

	//Sort relation
	ptrs.clear();
	for(size_t i=0; i<relations.size(); i++)
		ptrs.push_back(&relations[i]);
	std::sort(ptrs.begin(), ptrs.end(), CompareObjsById);
	std::vector<class OsmRelation> relationsCpy;
	for(size_t i=0; i<ptrs.size(); i++)
		relationsCpy.push_back(*static_cast<class OsmRelation *>(ptrs[i]));
	relations = relationsCpy;	

	this->StreamTo(*out);
	return false;
}

