// Load a map file in any supported format into memory, then save it as o5m and XML.
#include <fstream>
#include <iostream>
#include "../cppo5m.h"
using namespace std;

int main(int argc, char **argv)
{
	string inFilename = "tests/data/o5mtest.o5m";
	if(argc > 1)
		inFilename = argv[1];

	std::filebuf infi;
	if(!infi.open(inFilename, std::ios::in | std::ios::binary))
	{
		cerr << "Cannot open " << inFilename << endl;
		return 1;
	}

	OsmData osmData;
	std::unique_ptr<OsmDecoder> decoder = MakeDecoder(FormatFromFilename(inFilename), infi, osmData);
	decoder->Decode();

	cout << "nodes " << osmData.nodes.size() << endl;
	cout << "ways " << osmData.ways.size() << endl;
	cout << "relations " << osmData.relations.size() << endl;

	std::filebuf outfi;
	outfi.open("example-out.o5m", std::ios::out | std::ios::binary);
	SaveToO5m(osmData, outfi);
	outfi.close();
	cout << "Written example-out.o5m" << endl;

	std::filebuf outfi2;
	outfi2.open("example-out.osm", std::ios::out | std::ios::binary);
	SaveToOsmXml(osmData, outfi2);
	outfi2.close();
	cout << "Written example-out.osm" << endl;
	return 0;
}
