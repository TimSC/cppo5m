
#ifndef _O5M_H
#define _O5M_H

#include <string>
#include <stdint.h>
#include <vector>
#include "fixeddeque.h"
#include <map>
#include <iostream>
#include <memory>
#ifdef PYTHON_AWARE
#include <Python.h>
#endif

void TestDecodeNumber();
void TestEncodeNumber();

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

///Decodes a binary o5m stream and fires a series of events to the output object derived from IDataStreamHandler
class O5mDecode
{
protected:
	std::istream handle;

	int64_t lastObjId;
	int64_t lastTimeStamp;
	int64_t lastChangeSet;
	FixedDeque<std::string> stringPairs;
	std::map<std::string, int> stringPairsDict;  
	double lastLat;
	double lastLon;
	int64_t lastRefNode;
	int64_t lastRefWay;
	int64_t lastRefRelation;
	bool finished, stopProcessing;

	unsigned refTableLengthThreshold;
	unsigned refTableMaxSize;

	//Various buffers to avoid continuously reallocating memory
	std::string tmpBuff;
	class MetaData tmpMetaData;
	std::string combinedRawTmpBuff;
	std::vector<int64_t> tmpRefsBuff;
	TagMap tmpTagsBuff;
	std::vector<std::string> tmpRefRolesBuff, tmpRefTypeStrBuff;

	void DecodeBoundingBox();
	void DecodeSingleString(std::istream &stream, std::string &out);
	void ConsiderAddToStringRefTable(const std::string &firstStr, const std::string &secondStr);
	void AddBuffToStringRefTable(const std::string &buff);
	void DecodeMetaData(std::istream &nodeDataStream, class MetaData &out);
	bool ReadStringPair(std::istream &stream, std::string &firstStr, std::string &secondStr);
	void DecodeNode();
	void DecodeWay();
	void DecodeRelation();

public:
	O5mDecode(std::streambuf &handleIn);
	virtual ~O5mDecode();

	void ResetDeltaCoding();
	bool DecodeNext();
	void DecodeHeader();
	void DecodeFinish();

	std::shared_ptr<class IDataStreamHandler> output;
};

///Defines an interface to handle a stream of map objects. Derive from this to make a result handler.
class IDataStreamHandler
{
public:
	virtual ~IDataStreamHandler() {};

	virtual bool Sync() {return false;};
	virtual bool Reset() {return false;};
	virtual bool Finish() {return false;};

	virtual bool StoreIsDiff(bool) {return false;};
	virtual bool StoreBounds(double x1, double y1, double x2, double y2) {return false;};
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

///Encodes a stream of map objects into an o5m output binary stream
class O5mEncodeBase : public IDataStreamHandler
{
protected:

	int64_t lastObjId;
	int64_t lastTimeStamp;
	int64_t lastChangeSet;
	FixedDeque<std::string> stringPairs;
	std::map<std::string, int> stringPairsDict; 
	double lastLat;
	double lastLon;
	int64_t lastRefNode;
	int64_t lastRefWay;
	int64_t lastRefRelation;

	unsigned refTableLengthThreshold;
	unsigned refTableMaxSize;
	int64_t runningRefOffset;

	void WriteStart();
	void EncodeMetaData(const class MetaData &metaData, std::ostream &outStream);
	void WriteStringPair(const std::string &firstString, const std::string &secondString, 
			std::ostream &tmpStream);
	void AddToRefTable(const std::string &encodedStrings);
	size_t FindStringPairsIndex(std::string needle, bool &indexFound);

	virtual void write (const char* s, std::streamsize n);
	virtual void operator<< (const std::string &val);

public:
	O5mEncodeBase();
	virtual ~O5mEncodeBase();

	void ResetDeltaCoding();

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

};

class O5mEncode : public O5mEncodeBase
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
	O5mEncode(std::streambuf &handle);
	virtual ~O5mEncode();
};

#ifdef PYTHON_AWARE
class PyO5mEncode : public O5mEncodeBase
{
private:
	PyObject* m_PyObj;
	PyObject* m_Write;

protected:
	virtual void write (const char* s, std::streamsize n);
	virtual void operator<< (const std::string &val);

public:
	PyO5mEncode(PyObject* obj);
	virtual ~PyO5mEncode();	

};
#endif //PYTHON_AWARE

#endif //_O5M_H

