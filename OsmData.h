#ifndef _OSMDATA_H
#define _OSMDATA_H

#include <memory>
#include "o5m.h"
#include "osmxml.h"

// ****** generic osm data store ******

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
};

class OsmWay : public OsmObject
{
public:
	std::vector<int64_t> refs;

	OsmWay();
	virtual ~OsmWay();
	OsmWay( const OsmWay &obj);
	OsmWay& operator=(const OsmWay &arg);
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
};

///Provides in memory decoding and encoding of o5m binary streams
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

	void StoreIsDiff(bool);
	void StoreBounds(double x1, double y1, double x2, double y2);
	void StoreNode(int64_t objId, const class MetaData &metaData, 
		const TagMap &tags, double lat, double lon);
	void StoreWay(int64_t objId, const class MetaData &metaDta, 
		const TagMap &tags, const std::vector<int64_t> &refs);
	void StoreRelation(int64_t objId, const class MetaData &metaData, const TagMap &tags, 
		const std::vector<std::string> &refTypeStrs, const std::vector<int64_t> &refIds, 
		const std::vector<std::string> &refRoles);
	void StoreObject(const class OsmObject *obj);
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

// Convenience functions: load and save from std::streambuf

void LoadFromO5m(std::streambuf &fi, std::shared_ptr<class IDataStreamHandler> output);
void LoadFromOsmXml(std::streambuf &fi, std::shared_ptr<class IDataStreamHandler> output);
void LoadFromOsmChangeXml(std::streambuf &fi, std::shared_ptr<class IOsmChangeBlock> output);
void SaveToO5m(const class OsmData &osmData, std::streambuf &fi);
void SaveToOsmXml(const class OsmData &osmData, std::streambuf &fi);
void SaveToOsmChangeXml(const class OsmChange &osmChange, std::streambuf &fi);

// Convenience functions: load from std::string

void LoadFromO5m(const std::string &fi, std::shared_ptr<class IDataStreamHandler> output);
void LoadFromOsmXml(const std::string &fi, std::shared_ptr<class IDataStreamHandler> output);
void LoadFromOsmChangeXml(const std::string &fi, std::shared_ptr<class IOsmChangeBlock> output);

#endif //_OSMDATA_H

