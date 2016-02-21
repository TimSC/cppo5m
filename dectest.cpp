
#include <iostream>
#include <fstream>
#include "o5m.h"
using namespace std;

class ResultHandler : public IDataStreamHandler
{
public:
	void StoreIsDiff(bool isDiff)
	{
		cout << "StoreIsDiff " << isDiff << endl;
	}

	void StoreBounds(double x1, double y1, double x2, double y2)
	{
		cout << x1 << "," << y1 << "," << x2 << "," << y2 << endl;
	}

	void StoreNode(int64_t objId, const class MetaData &metaData, const TagMap &tags, double lat, double lon)
	{
		cout << "node " << objId << endl;
		for(TagMap::const_iterator it=tags.begin(); it != tags.end(); it ++)
			cout << "tag: " << it->first << "\t" << it->second << endl;
	}

	void StoreWay(int64_t objId, const class MetaData &metaData, const TagMap &tags, std::vector<int64_t> &refs)
	{
		cout << "way " << objId << endl;
	}

	void StoreRelation(int64_t objId, const MetaData &metaData, const TagMap &tags, 
		std::vector<std::string> refTypeStrs, std::vector<int64_t> refIds, std::vector<std::string> refRoles)
	{
		cout << "relation " << objId << endl;
	}
};

int main()
{
	std::ifstream infi("o5mtest.o5m");

	class O5mDecode dec(infi);
	class ResultHandler resultHandler;
	dec.output = &resultHandler;

	dec.DecodeHeader();

	while (!infi.eof())
		dec.DecodeNext();
}

