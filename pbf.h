#ifndef _PBF_H
#define _PBF_H

#include "OsmData.h"
#include <iostream>
#ifdef PYTHON_AWARE
#include <Python.h>
#endif

///Decodes a binary PBF stream and fires a series of events to the output object derived from IDataStreamHandler
class PbfDecode : public OsmDecoder
{
protected:
	std::istream handle;
	std::string prevObjType;

	bool DecodeOsmData(std::string &decBlob);
	bool CheckOutputType(const char *objType);

public:
	PbfDecode(std::streambuf &handleIn);
	virtual ~PbfDecode();

	bool DecodeNext();
	void DecodeFinish();
};


///Encodes a stream of map objects into an Pbf output binary stream
class PbfEncodeBase : public IDataStreamHandler
{
protected:

	virtual void write (const char* s, std::streamsize n);
	virtual void operator<< (const std::string &val);
	class OsmData buffer;
	std::string prevObjType;
	bool headerWritten;
	uint32_t maxPayloadSize, maxHeaderSize, optimalDenseNodes, optimalWays, optimalRelations;

	void EncodeBuffer();
	void EncodeHeaderBlock(std::string &out);
	void EncodePbfDenseNodes(const std::vector<class OsmNode> &nodes, size_t &nodec, size_t maxNodesToProcess, 
		std::string &out);
	void EncodePbfWays(const std::vector<class OsmWay> &ways, size_t &wayc, size_t maxWaysToProcess, 
		std::string &out);
	void EncodePbfRelations(const std::vector<class OsmRelation> &relations, size_t &relationc, size_t maxRelsToProcess, 
		std::string &out);

	void EncodePbfDenseNodesSizeLimited(const std::vector<class OsmNode> &nodes, size_t &nodec, std::string &out);
	void EncodePbfWaysSizeLimited(const std::vector<class OsmWay> &ways, size_t &wayc, std::string &out);
	void EncodePbfRelationsSizeLimited(const std::vector<class OsmRelation> &relations, size_t &relc, std::string &out);

	void WriteBlobPayload(const std::string &blobPayload, const char *type);

public:
	PbfEncodeBase();
	virtual ~PbfEncodeBase();

	bool Sync();
	bool Reset();
	bool Finish();

	bool StoreIsDiff(bool);
	bool StoreBounds(double x1, double y1, double x2, double y2);
	bool StoreNode(int64_t objId, const class MetaData &metaData, 
		const TagMap &tags, double lat, double lon);
	bool StoreWay(int64_t objId, const class MetaData &metaData, 
		const TagMap &tags, const std::vector<int64_t> &refs);
	bool StoreRelation(int64_t objId, const class MetaData &metaData, const TagMap &tags, 
		const std::vector<std::string> &refTypeStrs, const std::vector<int64_t> &refIds, 
		const std::vector<std::string> &refRoles);

	bool encodeMetaData, encodeHistorical, compressUsingZLib;
	size_t maxGroupObjects;
	std::string writingProgram;
	int32_t granularity, date_granularity;
	int64_t lat_offset, lon_offset;
};

class PbfEncode : public PbfEncodeBase
{
private:
	std::ostream handle;

protected:
	virtual void write (const char* s, std::streamsize n)
	{
		this->handle.write(s, n);
	}

	virtual void operator<< (const std::string &val)
	{
		this->handle << val;
	}

public:
	PbfEncode(std::streambuf &handle);
	virtual ~PbfEncode();
};

#ifdef PYTHON_AWARE
class PyPbfEncode : public PbfEncodeBase
{
private:
	PyObject* m_PyObj;
	PyObject* m_Write;

protected:
	virtual void write (const char* s, std::streamsize n);
	virtual void operator<< (const std::string &val);

public:
	PyPbfEncode(PyObject* obj);
	virtual ~PyPbfEncode();	

};
#endif //PYTHON_AWARE

#endif //_PBF_H
