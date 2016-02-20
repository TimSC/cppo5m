
#ifndef _O5M_H
#define _O5M_H

#include <string>
#include <stdint.h>
#include <vector>

void TestDecodeNumber();
void TestEncodeNumber();

class O5mDecode
{
protected:
	std::istream &handle;

	int64_t lastObjId;
	int64_t lastTimeStamp;
	int64_t lastChangeSet;
	std::vector<std::pair<std::string, std::string> > stringPairs;
	double lastLat;
	double lastLon;
	int64_t lastRefNode;
	int64_t lastRefWay;
	int64_t lastRefRelation;

	unsigned refTableLengthThreshold;
	unsigned refTableMaxSize;

	std::string tmpBuff;

	void DecodeBoundingBox();

public:
	O5mDecode(std::istream &handleIn);
	virtual ~O5mDecode();

	void ResetDeltaCoding();
	bool DecodeNext();
	void DecodeHeader();

	void (*funcStoreNode)();
	void (*funcStoreWay)();
	void (*funcStoreRelation)();
	void (*funcStoreBounds)(double, double, double, double);
	void (*funcStoreIsDiff)(bool);
};

#endif //_O5M_H

