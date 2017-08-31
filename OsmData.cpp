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
	return *this;
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
	return *this;
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
	return *this;
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

void OsmData::LoadFromO5m(std::streambuf &fi)
{
	class O5mDecode dec(fi);
	dec.output = this;
	dec.DecodeHeader();

	while (fi.in_avail()>0)
	{
		dec.DecodeNext();
	}
}

void OsmData::SaveToO5m(std::streambuf &fi)
{
	class O5mEncode enc(fi);
	this->StreamTo(enc);
}

void OsmData::StreamTo(class IDataStreamHandler &enc)
{
	enc.StoreIsDiff(this->isDiff);
	for(size_t i=0;i< this->bounds.size(); i++) {
		std::vector<double> &bbox = this->bounds[i];
		enc.StoreBounds(bbox[0], bbox[1], bbox[2], bbox[3]);
	}
	for(size_t i=0; i < this->nodes.size(); i++)
	{
		class OsmNode &node = this->nodes[i];
		enc.StoreNode(node.objId, node.metaData, 
			node.tags, node.lat, node.lon);
	}
	enc.Reset();
	for(size_t i=0; i < this->ways.size(); i++)
	{
		class OsmWay &way = this->ways[i];
		enc.StoreWay(way.objId, way.metaData, 
			way.tags, way.refs);
	}
	enc.Reset();
	for(size_t i=0; i < this->relations.size(); i++)
	{
		class OsmRelation &relation = this->relations[i];
		enc.StoreRelation(relation.objId, relation.metaData, relation.tags, 
			relation.refTypeStrs, relation.refIds, 
			relation.refRoles);
	}
	enc.Finish();
}

void OsmData::StoreIsDiff(bool d)
{
	this->isDiff = d;
}

void OsmData::StoreBounds(double x1, double y1, double x2, double y2)
{
	std::vector<double> b;
	b.resize(4);
	b[0] = x1;
	b[1] = y1;
	b[2] = x2;
	b[3] = y2;
	this->bounds.push_back(b);
}

void OsmData::StoreNode(int64_t objId, const class MetaData &metaData, 
	const TagMap &tags, double lat, double lon)
{
	class OsmNode osmNode;
	osmNode.objId = objId;
	osmNode.metaData = metaData;
	osmNode.tags = tags; 
	osmNode.lat = lat;
	osmNode.lon = lon;

	this->nodes.push_back(osmNode);
}

void OsmData::StoreWay(int64_t objId, const class MetaData &metaData, 
		const TagMap &tags, const std::vector<int64_t> &refs)
{
	class OsmWay osmWay;
	osmWay.objId = objId;
	osmWay.metaData = metaData;
	osmWay.tags = tags; 
	osmWay.refs = refs;

	this->ways.push_back(osmWay);

}

void OsmData::StoreRelation(int64_t objId, const class MetaData &metaData, const TagMap &tags, 
		const std::vector<std::string> &refTypeStrs, const std::vector<int64_t> &refIds, 
		const std::vector<std::string> &refRoles)
{
	class OsmRelation osmRelation;
	osmRelation.objId = objId;
	osmRelation.metaData = metaData;
	osmRelation.tags = tags; 
	osmRelation.refTypeStrs = refTypeStrs;
	osmRelation.refIds = refIds;
	osmRelation.refRoles = refRoles;

	this->relations.push_back(osmRelation);

}

