#ifndef _OSMXML_H
#define _OSMXML_H

#include "o5m.h"
#ifdef PYTHON_AWARE
#include <Python.h>
#endif

class OsmXmlEncodeBase : public IDataStreamHandler
{
protected:
	void WriteStart();
	void EncodeMetaData(const class MetaData &metaData, std::stringstream &ss);

public:
	OsmXmlEncodeBase();
	virtual ~OsmXmlEncodeBase();

	void Sync();
	void Reset();
	void Finish();

	void StoreIsDiff(bool);
	void StoreBounds(double x1, double y1, double x2, double y2);
	void StoreNode(int64_t objId, const class MetaData &metaData, 
		const TagMap &tags, double lat, double lon);
	void StoreWay(int64_t objId, const class MetaData &metaData, 
		const TagMap &tags, const std::vector<int64_t> &refs);
	void StoreRelation(int64_t objId, const class MetaData &metaData, const TagMap &tags, 
		const std::vector<std::string> &refTypeStrs, const std::vector<int64_t> &refIds, 
		const std::vector<std::string> &refRoles);

	virtual void write (const char* s, streamsize n)=0;
	virtual void operator<< (const string &val)=0;
};

class OsmXmlEncode : public OsmXmlEncodeBase
{
private:
	std::ostream handle;
	
	virtual void write (const char* s, streamsize n)
	{
		this->handle.write(s, n);
	}

	virtual void operator<< (const string &val)
	{
		this->handle << val;
	}

public:
	OsmXmlEncode(std::streambuf &handle);
	virtual ~OsmXmlEncode();
};

#endif //_OSMXML_H

