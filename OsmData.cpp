#include "OsmData.h"
#include <iostream>

OsmObject::OsmObject()
{
	objId = 0;
}

OsmObject::~OsmObject()
{

}

OsmObject::OsmObject( const OsmObject &obj) : OsmObject()
{
	*this = obj;
}

OsmObject& OsmObject::operator=(const OsmObject &arg)
{
	objId = arg.objId;
	metaData = arg.metaData;
	tags = arg.tags; 
	return *this;
}

// ***********************************

OsmNode::OsmNode() : OsmObject()
{
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
	OsmObject::operator=(arg);
	lat = arg.lat;
	lon = arg.lon;
	return *this;
}

OsmWay::OsmWay() : OsmObject()
{

}

OsmWay::~OsmWay()
{

}

OsmWay::OsmWay( const OsmWay &obj) : OsmObject()
{
	*this = obj;
}

OsmWay& OsmWay::operator=(const OsmWay &arg)
{
	OsmObject::operator=(arg);
	refs = arg.refs;
	return *this;
}

OsmRelation::OsmRelation() : OsmObject()
{

}

OsmRelation::~OsmRelation()
{

}

OsmRelation::OsmRelation( const OsmRelation &obj) : OsmObject()
{
	*this = obj;
}

OsmRelation& OsmRelation::operator=(const OsmRelation &arg)
{
	if(arg.refTypeStrs.size() != arg.refIds.size() || arg.refTypeStrs.size() != arg.refRoles.size())
		throw std::invalid_argument("Length of ref vectors must be equal for RHS relation");

	OsmObject::operator=(arg);
	refTypeStrs = arg.refTypeStrs;
	refIds = arg.refIds;
	refRoles = arg.refRoles;
	return *this;
}

// ****** generic osm data store ******

OsmData::OsmData()
{
	this->Clear();
}

OsmData::~OsmData()
{

}

OsmData::OsmData( const OsmData &obj)
{
	*this = obj;
}

OsmData& OsmData::operator=(const OsmData &obj)
{
	nodes = obj.nodes;
	ways = obj.ways;
	relations = obj.relations;
	bounds = obj.bounds;
	isDiff = obj.isDiff;
	return *this;
}

void OsmData::StreamTo(class IDataStreamHandler &enc, bool finishStream) const
{
	enc.StoreIsDiff(this->isDiff);
	for(size_t i=0;i< this->bounds.size(); i++) {
		const std::vector<double> &bbox = this->bounds[i];
		enc.StoreBounds(bbox[0], bbox[1], bbox[2], bbox[3]);
	}
	for(size_t i=0; i < this->nodes.size(); i++)
	{
		const class OsmNode &node = this->nodes[i];
		enc.StoreNode(node.objId, node.metaData, 
			node.tags, node.lat, node.lon);
	}
	enc.Reset();
	for(size_t i=0; i < this->ways.size(); i++)
	{
		const class OsmWay &way = this->ways[i];
		enc.StoreWay(way.objId, way.metaData, 
			way.tags, way.refs);
	}
	enc.Reset();
	for(size_t i=0; i < this->relations.size(); i++)
	{
		const class OsmRelation &relation = this->relations[i];
		enc.StoreRelation(relation.objId, relation.metaData, relation.tags, 
			relation.refTypeStrs, relation.refIds, 
			relation.refRoles);
	}
	if(finishStream)
		enc.Finish();
}

void OsmData::Clear()
{
	nodes.clear();
	ways.clear();
	relations.clear();
	bounds.clear();
	isDiff = false;
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

// *********************************

OsmChange::OsmChange() : IOsmChangeBlock()
{
	
}

OsmChange::OsmChange( const OsmChange &obj) : IOsmChangeBlock()
{
	*this = obj;
}

OsmChange& OsmChange::operator=(const OsmChange &arg)
{
	this->blocks = arg.blocks;
	this->actions = arg.actions;
	return *this;
}

OsmChange::~OsmChange()
{

}

void OsmChange::StoreOsmData(const std::string &action, const class OsmData &osmData)
{
	this->actions.push_back(action);
	this->blocks.push_back(osmData);
}

// ******* Utility funcs **********

void LoadFromO5m(std::streambuf &fi, std::shared_ptr<class IDataStreamHandler> output)
{
	class O5mDecode dec(fi);
	dec.output = output;
	dec.DecodeHeader();

	while (fi.in_avail()>0)
	{
		bool ok = dec.DecodeNext();
		if(!ok) break;
	}
}

void LoadFromOsmXml(std::streambuf &fi, std::shared_ptr<class IDataStreamHandler> output)
{
	class OsmXmlDecode dec(fi);
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
}

void LoadFromOsmChangeXml(std::streambuf &fi, std::shared_ptr<class IOsmChangeBlock> output)
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
}

void SaveToO5m(const class OsmData &osmData, std::streambuf &fi)
{
	class O5mEncode enc(fi);
	osmData.StreamTo(enc);
}

void SaveToOsmXml(const class OsmData &osmData, std::streambuf &fi)
{
	class OsmXmlEncode enc(fi);
	osmData.StreamTo(enc);
}

