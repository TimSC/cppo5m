#include "model.h"
#include <stdexcept>
using namespace std;

const char *ObjectTypeName(ObjectType type)
{
	switch(type)
	{
	case ObjectType::Node: return "node";
	case ObjectType::Way: return "way";
	case ObjectType::Relation: return "relation";
	}
	throw invalid_argument("Invalid object type");
}

bool ObjectTypeFromName(const std::string &name, ObjectType &out)
{
	if(name == "node") out = ObjectType::Node;
	else if(name == "way") out = ObjectType::Way;
	else if(name == "relation") out = ObjectType::Relation;
	else return false;
	return true;
}

ObjectType ObjectTypeFromName(const std::string &name)
{
	ObjectType out = ObjectType::Node;
	if(!ObjectTypeFromName(name, out))
		throw invalid_argument("Unknown object type: " + name);
	return out;
}

// **************************************************

bool MetaData::operator==(const MetaData &other) const
{
	return version == other.version && timestamp == other.timestamp &&
		changeset == other.changeset && uid == other.uid &&
		username == other.username && visible == other.visible &&
		current == other.current;
}

bool Bounds::operator==(const Bounds &other) const
{
	return minLon == other.minLon && minLat == other.minLat &&
		maxLon == other.maxLon && maxLat == other.maxLat;
}

static bool SameBase(const OsmObject &a, const OsmObject &b)
{
	return a.objId == b.objId && a.metaData == b.metaData && a.tags == b.tags;
}

void OsmNode::StreamTo(IDataStreamHandler &out) const
{
	out.StoreNode(*this);
}

bool OsmNode::operator==(const OsmNode &other) const
{
	return SameBase(*this, other) && lat == other.lat && lon == other.lon;
}

void OsmWay::StreamTo(IDataStreamHandler &out) const
{
	out.StoreWay(*this);
}

bool OsmWay::operator==(const OsmWay &other) const
{
	return SameBase(*this, other) && refs == other.refs;
}

bool RelationMember::operator==(const RelationMember &other) const
{
	return type == other.type && ref == other.ref && role == other.role;
}

void OsmRelation::StreamTo(IDataStreamHandler &out) const
{
	out.StoreRelation(*this);
}

bool OsmRelation::operator==(const OsmRelation &other) const
{
	return SameBase(*this, other) && members == other.members;
}

// ****** generic osm data store ******

void OsmData::Clear()
{
	nodes.clear();
	ways.clear();
	relations.clear();
	bounds.clear();
	isDiff = false;
}

bool OsmData::IsEmpty() const
{
	return nodes.empty() && ways.empty() && relations.empty() && bounds.empty();
}

void OsmData::StreamTo(IDataStreamHandler &out, bool finishStream) const
{
	out.StoreIsDiff(this->isDiff);
	for(const Bounds &b : this->bounds)
		out.StoreBounds(b);
	for(const OsmNode &node : this->nodes)
		out.StoreNode(node);
	out.Reset();
	for(const OsmWay &way : this->ways)
		out.StoreWay(way);
	out.Reset();
	for(const OsmRelation &relation : this->relations)
		out.StoreRelation(relation);
	if(finishStream)
		out.Finish();
}

void OsmData::StoreObject(const OsmObject &obj)
{
	// The dynamic type decides, so a subclass of a node is still stored as a node.
	obj.StreamTo(*this);
}

void OsmData::StoreIsDiff(bool d)
{
	this->isDiff = d;
}

void OsmData::StoreBounds(const Bounds &b)
{
	this->bounds.push_back(b);
}

void OsmData::StoreNode(const OsmNode &node)
{
	this->nodes.push_back(node);
}

void OsmData::StoreWay(const OsmWay &way)
{
	this->ways.push_back(way);
}

void OsmData::StoreRelation(const OsmRelation &relation)
{
	this->relations.push_back(relation);
}

std::set<int64_t> OsmData::GetNodeIds() const
{
	std::set<int64_t> out;
	for(const OsmNode &node : nodes)
		out.insert(node.objId);
	return out;
}

std::set<int64_t> OsmData::GetWayIds() const
{
	std::set<int64_t> out;
	for(const OsmWay &way : ways)
		out.insert(way.objId);
	return out;
}

std::set<int64_t> OsmData::GetRelationIds() const
{
	std::set<int64_t> out;
	for(const OsmRelation &relation : relations)
		out.insert(relation.objId);
	return out;
}

bool OsmData::operator==(const OsmData &other) const
{
	return isDiff == other.isDiff && bounds == other.bounds && nodes == other.nodes &&
		ways == other.ways && relations == other.relations;
}

// *********************************

bool OsmChangeBlock::operator==(const OsmChangeBlock &other) const
{
	return action == other.action && ifUnused == other.ifUnused && data == other.data;
}

void OsmChange::Clear()
{
	blocks.clear();
}

void OsmChange::StoreChangeBlock(const OsmChangeBlock &block)
{
	this->blocks.push_back(block);
}

void OsmChange::StoreObject(const OsmObject &obj, bool ifUnused)
{
	std::string action = "delete";
	if(obj.metaData.version <= 1)
		action = "create";
	else if(obj.metaData.visible)
		action = "modify";
	if(action != "delete")
		ifUnused = false;

	if(this->blocks.empty() || this->blocks.back().action != action ||
		this->blocks.back().ifUnused != ifUnused)
		this->blocks.push_back(OsmChangeBlock(action, ifUnused));
	this->blocks.back().data.StoreObject(obj);
}

bool OsmChange::operator==(const OsmChange &other) const
{
	return blocks == other.blocks;
}
