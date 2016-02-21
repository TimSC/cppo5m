#include "OsmData.h"
#include <iostream>

OsmNode::OsmNode()
{
	objId = 0;
	lat = 0.0;
	lon = 0.0;
}

OsmNode::~OsmNode()
{

}

OsmNode::OsmNode( const OsmNode &obj)
{
	*this = obj;
}

OsmNode& OsmNode::operator=(const OsmNode &arg)
{
	objId = arg.objId;
	metaData = arg.metaData;
	tags = arg.tags; 
	lat = arg.lat;
	lon = arg.lon;
}

OsmWay::OsmWay()
{
	objId = 0;
}

OsmWay::~OsmWay()
{

}

OsmWay::OsmWay( const OsmWay &obj)
{
	*this = obj;
}

OsmWay& OsmWay::operator=(const OsmWay &arg)
{
	objId = arg.objId;
	metaData = arg.metaData;
	tags = arg.tags; 
	refs = arg.refs;
}

OsmRelation::OsmRelation()
{
	objId = 0;
}

OsmRelation::~OsmRelation()
{

}

OsmRelation::OsmRelation( const OsmRelation &obj)
{
	*this = obj;
}

OsmRelation& OsmRelation::operator=(const OsmRelation &arg)
{
	objId = arg.objId;
	metaData = arg.metaData;
	tags = arg.tags; 
	refTypeStrs = arg.refTypeStrs;
	refIds = arg.refIds;
	refRoles = arg.refRoles;
}

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
	dec.userData = this;
	dec.DecodeHeader();

	while (!fi.eof())
		dec.DecodeNext();
}

void OsmData::SaveToO5m(std::ostream &fi)
{
	class O5mEncode enc(fi);
/*	enc.StoreIsDiff(this->isDiff)
	foreach(bbox in this->bounds:
		enc.StoreBounds(bbox)
	for nodeData in self.nodes:
		enc.StoreNode(*nodeData)
	enc.Reset()
	for wayData in self.ways:
		enc.StoreWay(*wayData)
	enc.Reset()
	for relationData in self.relations:
		enc.StoreRelation(*relationData)*/
	enc.Finish();
}

void OsmData::FuncStoreIsDiff(bool d, void *userData)
{
	class OsmData *self = (class OsmData *)userData;
	self->isDiff = d;
}

void OsmData::FuncStoreBounds(double x1, double y1, double x2, double y2, void *userData)
{
	std::vector<double> b;
	b.resize(4);
	b[0] = x1;
	b[1] = y1;
	b[2] = x2;
	b[3] = y2;
	class OsmData *self = (class OsmData *)userData;
	self->bounds.push_back(b);
}

void OsmData::FuncStoreNode(int64_t objId, const class MetaData &metaData, 
	const TagMap &tags, double lat, double lon, void *userData)
{
	class OsmNode osmNode;
	osmNode.objId = objId;
	osmNode.metaData = metaData;
	osmNode.tags = tags; 
	osmNode.lat = lat;
	osmNode.lon = lon;

	class OsmData *self = (class OsmData *)userData;
	self->nodes.push_back(osmNode);
}

void OsmData::FuncStoreWay(int64_t objId, const class MetaData &metaData, 
	const TagMap &tags, std::vector<int64_t> &refs, void *userData)
{
	class OsmWay osmWay;
	osmWay.objId = objId;
	osmWay.metaData = metaData;
	osmWay.tags = tags; 
	osmWay.refs = refs;

	class OsmData *self = (class OsmData *)userData;
	self->ways.push_back(osmWay);

}

void OsmData::FuncStoreRelation(int64_t objId, const MetaData &metaData, const TagMap &tags, 
	std::vector<std::string> refTypeStrs, std::vector<int64_t> refIds, 
	std::vector<std::string> refRoles, void *userData)
{
	class OsmRelation osmRelation;
	osmRelation.objId = objId;
	osmRelation.metaData = metaData;
	osmRelation.tags = tags; 
	osmRelation.refTypeStrs = refTypeStrs;
	osmRelation.refIds = refIds;
	osmRelation.refRoles = refRoles;

	class OsmData *self = (class OsmData *)userData;
	self->relations.push_back(osmRelation);

}

