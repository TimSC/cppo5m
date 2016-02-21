
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
	O5mDecode(std::istream &handleIn);
	virtual ~O5mDecode();

	void ResetDeltaCoding();
	bool DecodeNext();
	void DecodeHeader();

	void (*funcStoreNode)(int64_t, const class MetaData &, const TagMap &, double, double);
	void (*funcStoreWay)(int64_t, const class MetaData &, const TagMap &, std::vector<int64_t> &);
	void (*funcStoreRelation)(int64_t, const MetaData &, const TagMap &, std::vector<std::string>, std::vector<int64_t>, std::vector<std::string>);
	void (*funcStoreBounds)(double, double, double, double);
	void (*funcStoreIsDiff)(bool);
};

#endif //_O5M_H

