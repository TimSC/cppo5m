#ifndef _UTILS_H
#define _UTILS_H

#include "OsmData.h"

// Convenience functions: load and save from std::streambuf

void LoadFromO5m(std::streambuf &fi, class IDataStreamHandler *output);
void LoadFromOsmXml(std::streambuf &fi, class IDataStreamHandler *output);
void LoadFromPbf(std::streambuf &fi, class IDataStreamHandler *output);
void LoadFromDecoder(std::streambuf &fi, class OsmDecoder *osmDecoder, class IDataStreamHandler *output);

void LoadFromOsmChangeXml(std::streambuf &fi, class IOsmChangeBlock *output);

void SaveToO5m(const class OsmData &osmData, std::streambuf &fi);
void SaveToOsmXml(const class OsmData &osmData, std::streambuf &fi);
void SaveToOsmChangeXml(const class OsmChange &osmChange, bool separateActions, std::streambuf &fi);

std::shared_ptr<class OsmDecoder> DecoderOsmFactory(std::streambuf &handleIn, const std::string &filename);

// Convenience functions: load from std::string

void LoadFromO5m(const std::string &fi, class IDataStreamHandler *output);
void LoadFromOsmXml(const std::string &fi, class IDataStreamHandler *output);

void LoadFromOsmChangeXml(const std::string &fi, class IOsmChangeBlock *output);

// Filters

class FindBbox : public IDataStreamHandler
{
public:
	FindBbox();
	virtual ~FindBbox();

	virtual bool StoreIsDiff(bool);
	virtual bool StoreBounds(double x1, double y1, double x2, double y2);
	virtual bool StoreNode(int64_t objId, const class MetaData &metaData, 
		const TagMap &tags, double lat, double lon);
	virtual bool StoreWay(int64_t objId, const class MetaData &metaData, 
		const TagMap &tags, const std::vector<int64_t> &refs);
	virtual bool StoreRelation(int64_t objId, const class MetaData &metaData, const TagMap &tags, 
		const std::vector<std::string> &refTypeStrs, const std::vector<int64_t> &refIds, 
		const std::vector<std::string> &refRoles);

	bool bboxFound;
	double x1, y1, x2, y2;
};

class DeduplicateOsm : public IDataStreamHandler
{
public:
	DeduplicateOsm(class IDataStreamHandler &out);
	virtual ~DeduplicateOsm();

	virtual bool StoreIsDiff(bool);
	virtual bool StoreBounds(double x1, double y1, double x2, double y2);
	virtual bool StoreNode(int64_t objId, const class MetaData &metaData, 
		const TagMap &tags, double lat, double lon);
	virtual bool StoreWay(int64_t objId, const class MetaData &metaData, 
		const TagMap &tags, const std::vector<int64_t> &refs);
	virtual bool StoreRelation(int64_t objId, const class MetaData &metaData, const TagMap &tags, 
		const std::vector<std::string> &refTypeStrs, const std::vector<int64_t> &refIds, 
		const std::vector<std::string> &refRoles);

	virtual void ResetExisting();

	std::set<int64_t> nodeIds, wayIds, relationIds;
	class IDataStreamHandler &out;
};

#endif //_UTILS_H

