
#include <iostream>
#include <fstream>
#include "o5m.h"
using namespace std;

void BoundingBox(double x1, double y1, double x2, double y2)
{
	cout << x1 << "," << y1 << "," << x2 << "," << y2 << endl;
}

void FuncStoreNode(int64_t objId, const class MetaData &metaData, TagMap &tags, double lat, double lon)
{
	cout << "node " << objId << endl;
	for(TagMap::iterator it=tags.begin(); it != tags.end(); it ++)
		cout << "tag: " << it->first << "\t" << it->second << endl;
}

void FuncStoreWay(int64_t objId, const class MetaData &metaData, TagMap &tags, std::vector<int64_t> &refs)
{
	cout << "way " << objId << endl;
}

int main()
{
	std::ifstream infi("o5mtest.o5m");

	class O5mDecode dec(infi);
	dec.funcStoreBounds = BoundingBox;
	dec.funcStoreNode = FuncStoreNode;
	dec.funcStoreWay = FuncStoreWay;
	dec.DecodeHeader();

	while (!infi.eof())
		dec.DecodeNext();
}

