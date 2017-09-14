#include "OsmData.h"
#include "osmxml.h"
#include <fstream>
#include <iostream>
using namespace std;

int main()
{
	shared_ptr<class OsmData> osmData(new class OsmData());
	std::filebuf infi;
	infi.open("example.osm", std::ios::in);
	LoadFromOsmXml(infi, osmData);
	cout << "nodes " << osmData->nodes.size() << endl;
	cout << "ways " << osmData->ways.size() << endl;
	cout << "relations " << osmData->relations.size() << endl;
	std::filebuf outfi;
	outfi.open("example2.osm", std::ios::out);
	SaveToOsmXml(*osmData, outfi);
	outfi.close();
}
