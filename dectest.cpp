
#include <iostream>
#include <fstream>
#include "o5m.h"
using namespace std;

class ResultHandler : public IDataStreamHandler
{
public:
	bool StoreIsDiff(bool isDiff)
	{
		cout << "StoreIsDiff " << isDiff << endl;
		return false;
	}

	bool StoreBounds(double x1, double y1, double x2, double y2)
	{
		cout << x1 << "," << y1 << "," << x2 << "," << y2 << endl;
		return false;
	}

	bool StoreNode(int64_t objId, const class MetaData &metaData, const TagMap &tags, double lat, double lon)
	{
		cout << "node " << objId << endl;
		for(TagMap::const_iterator it=tags.begin(); it != tags.end(); it ++)
			cout << "tag: " << it->first << "\t" << it->second << endl;
		return false;
	}

	bool StoreWay(int64_t objId, const class MetaData &metaData, const TagMap &tags, std::vector<int64_t> &refs)
	{
		cout << "way " << objId << endl;
		return false;
	}

	bool StoreRelation(int64_t objId, const MetaData &metaData, const TagMap &tags, 
		std::vector<std::string> refTypeStrs, std::vector<int64_t> refIds, std::vector<std::string> refRoles)
	{
		cout << "relation " << objId << endl;
		return false;
	}
};

int main()
{
	std::filebuf infi;
	infi.open("o5mtest.o5m", std::ios::in);

	class O5mDecode dec(infi);
	dec.output.reset(new class ResultHandler);

	dec.DecodeHeader();
	while (infi.in_avail()>0)
	{
		dec.DecodeNext();
	}
}

