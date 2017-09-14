#include "OsmData.h"
#include "osmxml.h"
#include <fstream>
#include <iostream>
using namespace std;

int main()
{
	shared_ptr<class OsmData> osmData(new class OsmData());
	shared_ptr<class IDataStreamHandler> parentType(osmData);
	std::filebuf infi;
	infi.open("example.osm", std::ios::in);
	LoadFromOsmXml(infi, parentType);
	cout << "nodes " << osmData->nodes.size() << endl;
	cout << "ways " << osmData->ways.size() << endl;
	cout << "relations " << osmData->relations.size() << endl;
	std::filebuf outfi;
	outfi.open("example2.osm", std::ios::out);
	SaveToOsmXml(*osmData, outfi);
	outfi.close();
}
