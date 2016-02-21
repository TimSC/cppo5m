#ifndef _OSMDATA_H
#define _OSMDATA_H

#include "o5m.h"

// ****** generic osm data store ******

class OsmNode
{
public:
	int64_t objId;
	class MetaData metaData;
	TagMap tags; 
	double lat;
	double lon;

	OsmNode();
	virtual ~OsmNode();
	OsmNode( const OsmNode &obj);
	OsmNode& operator=(const OsmNode &arg);
};

class OsmWay
{
public:
	int64_t objId;
	class MetaData metaData;
	TagMap tags; 
	std::vector<int64_t> refs;

	OsmWay();
	virtual ~OsmWay();
	OsmWay( const OsmWay &obj);
	OsmWay& operator=(const OsmWay &arg);
};

class OsmRelation
{
public:
	int64_t objId;
	class MetaData metaData;
	TagMap tags; 
	std::vector<std::string> refTypeStrs;
	std::vector<int64_t> refIds;
	std::vector<std::string> refRoles;

	OsmRelation();
	virtual ~OsmRelation();
	OsmRelation( const OsmRelation &obj);
	OsmRelation& operator=(const OsmRelation &arg);
};

class OsmData
{
public:
	std::vector<class OsmNode> nodes;
	std::vector<class OsmWay> ways;
	std::vector<class OsmRelation> relations;
	std::vector<std::vector<double> > bounds;
	bool isDiff;

	OsmData();
	virtual ~OsmData();
	void LoadFromO5m(std::istream &fi);
	void SaveToO5m(std::ostream &fi);

	static void FuncStoreIsDiff(bool, void *userData);
	static void FuncStoreBounds(double x1, double y1, double x2, double y2, void *userData);
	static void FuncStoreNode(int64_t objId, const class MetaData &metaData, 
		const TagMap &tags, double lat, double lon, void *userData);
	static void FuncStoreWay(int64_t objId, const class MetaData &metaDta, 
		const TagMap &tags, std::vector<int64_t> &refs, void *userData);
	static void FuncStoreRelation(int64_t objId, const MetaData &metaData, const TagMap &tags, 
		std::vector<std::string> refTypeStrs, std::vector<int64_t> refIds, 
		std::vector<std::string> refRoles, void *userData);
};

#endif //_OSMDATA_H

