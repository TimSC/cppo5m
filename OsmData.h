#ifndef _OSMDATA_H
#define _OSMDATA_H

#include "o5m.h"

// ****** generic osm data store ******

class OsmData
{
public:
	std::vector<int> nodes;
	std::vector<int> ways;
	std::vector<int> relations;
	std::vector<int> bounds;
	bool isDiff;

	OsmData();
	virtual ~OsmData();
	void LoadFromO5m(std::istream &fi);

	static void FuncStoreIsDiff(bool);
	static void FuncStoreBounds(double x1, double y1, double x2, double y2);
	static void FuncStoreNode(int64_t objId, const class MetaData &metaData, 
		const TagMap &tags, double lat, double lon);
	static void FuncStoreWay(int64_t objId, const class MetaData &metaDta, 
		const TagMap &tags, std::vector<int64_t> &refs);
	static void FuncStoreRelation(int64_t objId, const MetaData &metaData, const TagMap &tags, 
		std::vector<std::string> refTypeStrs, std::vector<int64_t> refIds, 
		std::vector<std::string> refRoles);
};

#endif //_OSMDATA_H

