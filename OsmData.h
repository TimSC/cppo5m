#ifndef _OSMDATA_H
#define _OSMDATA_H

#include <memory>
#include <set>
#include <map>
#include <vector>
#include <string>

typedef std::map<std::string, std::string> TagMap;
void PrintTagMap(const TagMap &tagMap);

///Simply stores meta data files for a map object
class MetaData
{
public:
	uint64_t version;
	int64_t timestamp, changeset;
	uint64_t uid;
	std::string username;
	bool visible, current;

	MetaData();
	virtual ~MetaData();
	MetaData( const MetaData &obj);
	MetaData& operator=(const MetaData &arg);
};

///Defines an interface to handle a stream of map objects. Derive from this to make a result handler.
///If any functions return true, that indicates the receiver wants to halt processing.
class IDataStreamHandler
{
public:
	virtual ~IDataStreamHandler() {};

	///Add syncronize code to output (if supported). Always call Reset immediately after a Sync. This
	///is only used by o5m and generally only when changing between object types in the stream.
	virtual bool Sync() {return false;};

	///Tells encoder to reset its counters (if supported)
	virtual bool Reset() {return false;};
	virtual bool Finish() {return false;};

	virtual bool StoreIsDiff(bool) {return false;};
	virtual bool StoreBounds(double x1, double y1, double x2, double y2) {return false;};

	virtual bool StoreBbox(const std::vector<double> &bbox) {return false;};

	virtual bool StoreNode(int64_t objId, const class MetaData &metaData, 
		const TagMap &tags, double lat, double lon) {return false;};
	virtual bool StoreWay(int64_t objId, const class MetaData &metaData, 
		const TagMap &tags, const std::vector<int64_t> &refs) {return false;};
	virtual bool StoreRelation(int64_t objId, const class MetaData &metaData, const TagMap &tags, 
		const std::vector<std::string> &refTypeStrs, const std::vector<int64_t> &refIds, 
		const std::vector<std::string> &refRoles) {return false;};
};

class IOsmChangeBlock
{
public:
	virtual ~IOsmChangeBlock() {};

	virtual void StoreOsmData(const std::string &action, const class OsmData &osmData, bool ifunused) {};
	virtual void StoreOsmData(const class OsmObject *obj, bool ifunused) {};
};

class OsmDecoder
{
public:
	OsmDecoder();
	virtual ~OsmDecoder();

	virtual bool DecodeNext()=0;
	virtual void DecodeHeader() {};
	virtual void DecodeFinish() {};

	class IDataStreamHandler *output;
	std::string errString;
};

class OsmObject
{
public:
	int64_t objId;
	class MetaData metaData;
	TagMap tags; 

	OsmObject();
	virtual ~OsmObject();
	OsmObject( const OsmObject &obj);
	OsmObject& operator=(const OsmObject &arg);
	virtual void StreamTo(class IDataStreamHandler &enc) const;
};

class OsmNode : public OsmObject
{
public:
	double lat;
	double lon;

	OsmNode();
	virtual ~OsmNode();
	OsmNode( const OsmNode &obj);
	OsmNode& operator=(const OsmNode &arg);
	void StreamTo(class IDataStreamHandler &enc) const;
};

class OsmWay : public OsmObject
{
public:
	std::vector<int64_t> refs;

	OsmWay();
	virtual ~OsmWay();
	OsmWay( const OsmWay &obj);
	OsmWay& operator=(const OsmWay &arg);
	void StreamTo(class IDataStreamHandler &enc) const;
	std::set<int64_t> GetRefIds() const;
};

class OsmRelation : public OsmObject
{
public:
	std::vector<std::string> refTypeStrs;
	std::vector<int64_t> refIds;
	std::vector<std::string> refRoles;

	OsmRelation();
	virtual ~OsmRelation();
	OsmRelation( const OsmRelation &obj);
	OsmRelation& operator=(const OsmRelation &arg);
	void StreamTo(class IDataStreamHandler &enc) const;
};

// ****** generic osm data store ******

class OsmData : public IDataStreamHandler
{
public:
	std::vector<class OsmNode> nodes;
	std::vector<class OsmWay> ways;
	std::vector<class OsmRelation> relations;
	std::vector<std::vector<double> > bounds;
	bool isDiff;

	OsmData();
	OsmData( const OsmData &obj);
	OsmData& operator=(const OsmData &arg);
	virtual ~OsmData();
	void StreamTo(class IDataStreamHandler &out, bool finishStream = true) const;
	void Clear();
	bool IsEmpty();

	bool StoreIsDiff(bool);
	bool StoreBounds(double x1, double y1, double x2, double y2);
	bool StoreNode(int64_t objId, const class MetaData &metaData, 
		const TagMap &tags, double lat, double lon);
	bool StoreWay(int64_t objId, const class MetaData &metaDta, 
		const TagMap &tags, const std::vector<int64_t> &refs);
	bool StoreRelation(int64_t objId, const class MetaData &metaData, const TagMap &tags, 
		const std::vector<std::string> &refTypeStrs, const std::vector<int64_t> &refIds, 
		const std::vector<std::string> &refRoles);
	void StoreObject(const class OsmObject *obj);

	std::set<int64_t> GetNodeIds() const;
	std::set<int64_t> GetWayIds() const;
	std::set<int64_t> GetRelationIds() const;
};

class OsmChange : public IOsmChangeBlock
{
public:
	std::vector<class OsmData> blocks;
	std::vector<std::string> actions; 
	std::vector<bool> ifunused;

	OsmChange();
	OsmChange( const OsmChange &obj);
	OsmChange& operator=(const OsmChange &arg);
	virtual ~OsmChange();

	virtual void StoreOsmData(const std::string &action, const class OsmData &osmData, bool ifunused);
	virtual void StoreOsmData(const class OsmObject *obj, bool ifunused);
};

#endif //_OSMDATA_H

