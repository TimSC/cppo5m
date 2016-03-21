
#ifndef _O5M_H
#define _O5M_H

#include <string>
#include <stdint.h>
#include <vector>
#include <deque>
#include <map>
#include <iostream>

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
	std::deque<std::string> stringPairs;
	double lastLat;
	double lastLon;
	int64_t lastRefNode;
	int64_t lastRefWay;
	int64_t lastRefRelation;

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

	class IDataStreamHandler *output;
};

///Defines an interface to handle a stream of map objects. Derive from this to make a result handler.
class IDataStreamHandler
{
public:
	virtual void StoreIsDiff(bool) {};
	virtual void StoreBounds(double x1, double y1, double x2, double y2) {};
	virtual void StoreNode(int64_t objId, const class MetaData &metaData, 
		const TagMap &tags, double lat, double lon) {};
	virtual void StoreWay(int64_t objId, const class MetaData &metaData, 
		const TagMap &tags, const std::vector<int64_t> &refs) {};
	virtual void StoreRelation(int64_t objId, const class MetaData &metaData, const TagMap &tags, 
		const std::vector<std::string> &refTypeStrs, const std::vector<int64_t> &refIds, 
		const std::vector<std::string> &refRoles) {};
};

///Encodes a stream of map objects into an o5m output binary stream
class O5mEncode : public IDataStreamHandler
{
protected:
	std::ostream handle;

	int64_t lastObjId;
	int64_t lastTimeStamp;
	int64_t lastChangeSet;
	std::deque<std::string> stringPairs;
	double lastLat;
	double lastLon;
	int64_t lastRefNode;
	int64_t lastRefWay;
	int64_t lastRefRelation;

	unsigned refTableLengthThreshold;
	unsigned refTableMaxSize;

	void EncodeMetaData(const class MetaData &metaData, std::ostream &outStream);
	void WriteStringPair(const std::string &firstString, const std::string &secondString, 
			std::ostream &tmpStream);
	void AddToRefTable(const std::string &encodedStrings);
	size_t FindStringPairsIndex(std::string needle, bool &indexFound);

public:
	O5mEncode(std::streambuf &handle);
	virtual ~O5mEncode();

	void ResetDeltaCoding();

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

};

#endif //_O5M_H

