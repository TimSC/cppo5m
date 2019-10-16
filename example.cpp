#include "OsmData.h"
#include "utils.h"
#include <fstream>
#include <iostream>
using namespace std;

int main()
{
	shared_ptr<class OsmData> osmData(new class OsmData());
	std::filebuf infi;
	infi.open("o5mtest.o5m", std::ios::in);
	LoadFromO5m(infi, osmData);
	cout << "nodes " << osmData->nodes.size() << endl;
	cout << "ways " << osmData->ways.size() << endl;
	cout << "relations " << osmData->relations.size() << endl;
	std::filebuf outfi;
	outfi.open("o5mtest2.o5m", std::ios::out | std::ios::binary);
	SaveToO5m(*osmData, outfi);
	outfi.close();
	cout << "Written o5mtest2.o5m" << endl;

	std::filebuf outfi2;
	outfi2.open("o5mtest2.osm", std::ios::out);
	SaveToOsmXml(*osmData, outfi2);
	outfi2.close();
	cout << "Written o5mtest2.osm" << endl;
}
