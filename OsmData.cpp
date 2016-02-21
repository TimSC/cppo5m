#include "OsmData.h"
#include <iostream>
using namespace std;

// ****** generic osm data store ******

OsmData::OsmData()
{
	nodes.clear();
	ways.clear();
	relations.clear();
	bounds.clear();
	isDiff = false;
}

OsmData::~OsmData()
{

}

void OsmData::LoadFromO5m(std::istream &fi)
{
	class O5mDecode dec(fi);
	dec.funcStoreNode = this->FuncStoreNode;
	dec.funcStoreWay = this->FuncStoreWay;
	dec.funcStoreRelation = this->FuncStoreRelation;
	dec.funcStoreBounds = this->FuncStoreBounds;
	dec.funcStoreIsDiff = this->FuncStoreIsDiff;
	dec.DecodeHeader();

	while (!fi.eof())
		dec.DecodeNext();
}
/*
	def SaveToO5m(self, fi):
		enc = o5m.O5mEncode(fi)
		enc.StoreIsDiff(self.isDiff)
		for bbox in self.bounds:
			enc.StoreBounds(bbox)
		for nodeData in self.nodes:
			enc.StoreNode(*nodeData)
		enc.Reset()
		for wayData in self.ways:
			enc.StoreWay(*wayData)
		enc.Reset()
		for relationData in self.relations:
			enc.StoreRelation(*relationData)
		enc.Finish()

	def LoadFromOsmXml(self, fi):
		dec = osmxml.OsmXmlDecode(fi)
		dec.funcStoreNode = self.StoreNode
		dec.funcStoreWay = self.StoreWay
		dec.funcStoreRelation = self.StoreRelation
		dec.funcStoreBounds = self.StoreBounds
		dec.funcStoreIsDiff = self.StoreIsDiff

		eof = False
		while not eof:
			eof = dec.DecodeNext()
	
	def SaveToOsmXml(self, fi):
		enc = osmxml.OsmXmlEncode(fi)
		enc.StoreIsDiff(self.isDiff)
		for bbox in self.bounds:
			enc.StoreBounds(bbox)
		for nodeData in self.nodes:
			enc.StoreNode(*nodeData)
		for wayData in self.ways:
			enc.StoreWay(*wayData)
		for relationData in self.relations:
			enc.StoreRelation(*relationData)
		enc.Finish()
*/

void OsmData::FuncStoreIsDiff(bool)
{

}

void OsmData::FuncStoreBounds(double x1, double y1, double x2, double y2)
{
	cout << x1 << "," << y1 << "," << x2 << "," << y2 << endl;
}

void OsmData::FuncStoreNode(int64_t objId, const class MetaData &metaData, 
	const TagMap &tags, double lat, double lon)
{
	cout << "node " << objId << endl;
	for(TagMap::const_iterator it=tags.begin(); it != tags.end(); it ++)
		cout << "tag: " << it->first << "\t" << it->second << endl;
}

void OsmData::FuncStoreWay(int64_t objId, const class MetaData &metaData, 
	const TagMap &tags, std::vector<int64_t> &refs)
{
	cout << "way " << objId << endl;
}

void OsmData::FuncStoreRelation(int64_t objId, const MetaData &metaData, const TagMap &tags, 
	std::vector<std::string> refTypeStrs, std::vector<int64_t> refIds, 
	std::vector<std::string> refRoles)
{

}

