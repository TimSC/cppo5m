#include "OsmData.h"
#include "osmxml.h"
#include "utils.h"
#include <fstream>
#include <iostream>
#include "pbf/osmformat.pb.h"
using namespace std;

int main()
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	class OsmData osmData;
	std::filebuf infi;
	infi.open("example.osm", std::ios::in);
	LoadFromOsmXml(infi, &osmData);
	cout << "nodes " << osmData.nodes.size() << endl;
	cout << "ways " << osmData.ways.size() << endl;
	cout << "relations " << osmData.relations.size() << endl;
	std::filebuf outfi;
	outfi.open("example2.osm", std::ios::out);
	SaveToOsmXml(osmData, outfi);
	outfi.close();
}
