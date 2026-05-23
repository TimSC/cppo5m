#include "o5m.h"
#include "OsmData.h"
#include <assert.h>
#include <iostream>
#include <sstream>
using namespace std;

void TestRelationEmptyRole()
{
	MetaData metaData;
	metaData.version = 0;

	TagMap tags;
	vector<string> refTypeStrs;
	vector<int64_t> refIds;
	vector<string> refRoles;
	refTypeStrs.push_back("node");
	refIds.push_back(123);
	refRoles.push_back("");

	stringstream encoded;
	O5mEncode enc(*encoded.rdbuf());
	enc.StoreIsDiff(false);
	enc.StoreRelation(1, metaData, tags, refTypeStrs, refIds, refRoles);
	enc.Finish();

	OsmData decoded;
	O5mDecode dec(*encoded.rdbuf());
	dec.output = &decoded;
	dec.DecodeHeader();
	while (encoded.rdbuf()->in_avail() > 0)
	{
		dec.DecodeNext();
	}

	assert(decoded.relations.size() == 1);
	assert(decoded.relations[0].refTypeStrs.size() == 1);
	assert(decoded.relations[0].refTypeStrs[0] == "node");
	assert(decoded.relations[0].refIds[0] == 123);
	assert(decoded.relations[0].refRoles[0] == "");
}

int main()
{
	TestDecodeNumber();
	TestEncodeNumber();
	TestRelationEmptyRole();
	cout << "ok" << endl;
}
