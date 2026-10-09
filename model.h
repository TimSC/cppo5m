#ifndef CPPO5M_MODEL_H
#define CPPO5M_MODEL_H

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "handler.h"

typedef std::map<std::string, std::string> TagMap;

enum class ObjectType : uint8_t
{
	Node = 0,
	Way = 1,
	Relation = 2
};

///Returns "node", "way" or "relation".
const char *ObjectTypeName(ObjectType type);
///Parses "node", "way" or "relation". Returns false, leaving out unchanged, for anything else.
bool ObjectTypeFromName(const std::string &name, ObjectType &out);
///As above, but throws std::invalid_argument for an unknown name.
ObjectType ObjectTypeFromName(const std::string &name);

struct MetaData
{
	uint64_t version = 0;
	int64_t timestamp = 0; ///<Seconds since the Unix epoch, or zero if unknown
	int64_t changeset = 0;
	uint64_t uid = 0;
	std::string username;
	bool visible = true;
	bool current = true;

	bool operator==(const MetaData &other) const;
	bool operator!=(const MetaData &other) const { return !(*this == other); }
};

///West, south, east and north edges in degrees.
struct Bounds
{
	double minLon = 0.0;
	double minLat = 0.0;
	double maxLon = 0.0;
	double maxLat = 0.0;

	Bounds() {}
	Bounds(double minLon, double minLat, double maxLon, double maxLat) :
		minLon(minLon), minLat(minLat), maxLon(maxLon), maxLat(maxLat) {}

	bool operator==(const Bounds &other) const;
	bool operator!=(const Bounds &other) const { return !(*this == other); }
};

///Fields shared by nodes, ways and relations.
class OsmObject
{
public:
	int64_t objId = 0;
	MetaData metaData;
	TagMap tags;

	OsmObject() = default;
	OsmObject(const OsmObject &) = default;
	OsmObject(OsmObject &&) = default;
	OsmObject &operator=(const OsmObject &) = default;
	OsmObject &operator=(OsmObject &&) = default;
	virtual ~OsmObject() = default;

	virtual ObjectType Type() const = 0;
	///Sends this object to the handler method matching its type.
	virtual void StreamTo(IDataStreamHandler &out) const = 0;
};

class OsmNode : public OsmObject
{
public:
	double lat = 0.0;
	double lon = 0.0;

	ObjectType Type() const override { return ObjectType::Node; }
	void StreamTo(IDataStreamHandler &out) const override;
	bool operator==(const OsmNode &other) const;
	bool operator!=(const OsmNode &other) const { return !(*this == other); }
};

class OsmWay : public OsmObject
{
public:
	std::vector<int64_t> refs;

	ObjectType Type() const override { return ObjectType::Way; }
	void StreamTo(IDataStreamHandler &out) const override;
	bool operator==(const OsmWay &other) const;
	bool operator!=(const OsmWay &other) const { return !(*this == other); }
};

struct RelationMember
{
	ObjectType type = ObjectType::Node;
	int64_t ref = 0;
	std::string role;

	RelationMember() {}
	RelationMember(ObjectType type, int64_t ref, const std::string &role = std::string()) :
		type(type), ref(ref), role(role) {}

	bool operator==(const RelationMember &other) const;
	bool operator!=(const RelationMember &other) const { return !(*this == other); }
};

class OsmRelation : public OsmObject
{
public:
	std::vector<RelationMember> members;

	ObjectType Type() const override { return ObjectType::Relation; }
	void StreamTo(IDataStreamHandler &out) const override;
	bool operator==(const OsmRelation &other) const;
	bool operator!=(const OsmRelation &other) const { return !(*this == other); }
};

///An in-memory collection of map objects. As a handler it keeps everything it
///is sent; StreamTo replays the contents into another handler.
class OsmData : public IDataStreamHandler
{
public:
	std::vector<OsmNode> nodes;
	std::vector<OsmWay> ways;
	std::vector<OsmRelation> relations;
	std::vector<Bounds> bounds;
	bool isDiff = false;

	void Clear();
	bool IsEmpty() const;
	///Sends bounds, then nodes, ways and relations with a Reset between the
	///types, then Finish unless finishStream is false.
	void StreamTo(IDataStreamHandler &out, bool finishStream = true) const;
	void StoreObject(const OsmObject &obj);

	void StoreIsDiff(bool isDiff) override;
	void StoreBounds(const Bounds &bounds) override;
	void StoreNode(const OsmNode &node) override;
	void StoreWay(const OsmWay &way) override;
	void StoreRelation(const OsmRelation &relation) override;

	std::set<int64_t> GetNodeIds() const;
	std::set<int64_t> GetWayIds() const;
	std::set<int64_t> GetRelationIds() const;

	bool operator==(const OsmData &other) const;
	bool operator!=(const OsmData &other) const { return !(*this == other); }
};

///One action element of an osmChange document.
struct OsmChangeBlock
{
	std::string action; ///<"create", "modify" or "delete"
	bool ifUnused = false; ///<The if-unused attribute, meaningful for delete
	OsmData data;

	OsmChangeBlock() {}
	OsmChangeBlock(const std::string &action, bool ifUnused = false) :
		action(action), ifUnused(ifUnused) {}

	bool operator==(const OsmChangeBlock &other) const;
	bool operator!=(const OsmChangeBlock &other) const { return !(*this == other); }
};

///An in-memory osmChange document.
class OsmChange : public IOsmChangeHandler
{
public:
	std::vector<OsmChangeBlock> blocks;

	void Clear();
	void StoreChangeBlock(const OsmChangeBlock &block) override;

	///Appends an object, choosing the action from its metadata: version 1 or
	///less is a create, a later visible version a modify, otherwise a delete.
	///Consecutive objects with the same action share a block. ifUnused applies
	///to deletes and starts a new block when it differs from the current one.
	void StoreObject(const OsmObject &obj, bool ifUnused = false);

	bool operator==(const OsmChange &other) const;
	bool operator!=(const OsmChange &other) const { return !(*this == other); }
};

#endif //CPPO5M_MODEL_H
