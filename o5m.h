
#ifndef _O5M_H
#define _O5M_H

#include <string>
#include <stdint.h>
#include <vector>
#include <deque>
#include <map>

void TestDecodeNumber();
void TestEncodeNumber();

typedef std::map<std::string, std::string> TagMap;

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

class O5mDecode
{
protected:
	std::istream &handle;

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

	std::string tmpBuff;
	class MetaData tmpMetaData;

	void DecodeBoundingBox();
	void DecodeSingleString(std::istream &stream, std::string &out);
	void ConsiderAddToStringRefTable(const std::string &firstStr, const std::string &secondStr);
	void AddBuffToStringRefTable(const std::string &buff);
	void DecodeMetaData(std::istream &nodeDataStream, class MetaData &out);
	void ReadStringPair(std::istream &stream, std::string &firstStr, std::string &secondStr);
	void DecodeNode();

public:
	O5mDecode(std::istream &handleIn);
	virtual ~O5mDecode();

	void ResetDeltaCoding();
	bool DecodeNext();
	void DecodeHeader();

	void (*funcStoreNode)(int64_t, const class MetaData &, TagMap &, double, double);
	void (*funcStoreWay)();
	void (*funcStoreRelation)();
	void (*funcStoreBounds)(double, double, double, double);
	void (*funcStoreIsDiff)(bool);
};

#endif //_O5M_H
