
#include <iostream>
#include <fstream>
#include "o5m.h"
using namespace std;

void BoundingBox(double x1, double y1, double x2, double y2, void *userData)
{
	cout << x1 << "," << y1 << "," << x2 << "," << y2 << endl;
}

void FuncStoreNode(int64_t objId, const class MetaData &metaData, const TagMap &tags, double lat, double lon, void *userData)
{
	cout << "node " << objId << endl;
	for(TagMap::const_iterator it=tags.begin(); it != tags.end(); it ++)
		cout << "tag: " << it->first << "\t" << it->second << endl;
}

void FuncStoreWay(int64_t objId, const class MetaData &metaData, const TagMap &tags, std::vector<int64_t> &refs, void *userData)
{
	cout << "way " << objId << endl;
}

void FuncStoreRelation(int64_t objId, const MetaData &metaData, const TagMap &tags, std::vector<std::string> refTypeStrs, std::vector<int64_t> refIds, std::vector<std::string> refRoles, void *userData)
{
	cout << "relation " << objId << endl;
}

int main()
{
	std::ifstream infi("o5mtest.o5m");

	class O5mDecode dec(infi);
	dec.funcStoreBounds = BoundingBox;
	dec.funcStoreNode = FuncStoreNode;
	dec.funcStoreWay = FuncStoreWay;
	dec.funcStoreRelation = FuncStoreRelation;
	dec.DecodeHeader();

	while (!infi.eof())
		dec.DecodeNext();
}

