#include "OsmData.h"
#include "utils.h"
#include <fstream>
#include <iostream>
#include "pbf/osmformat.pb.h"
using namespace std;

int main(int argc, char **argv)
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	string inFilename = "o5mtest.o5m";
	if (argc>1)
		inFilename = argv[1];
	class OsmData osmData;

	std::filebuf infi;
	infi.open(inFilename, std::ios::in);
	std::shared_ptr<class OsmDecoder> decoder = DecoderOsmFactory(infi, inFilename);

	LoadFromDecoder(infi, decoder.get(), &osmData);
	cout << "nodes " << osmData.nodes.size() << endl;
	cout << "ways " << osmData.ways.size() << endl;
	cout << "relations " << osmData.relations.size() << endl;
	std::filebuf outfi;
	outfi.open("o5mtest2.o5m", std::ios::out | std::ios::binary);
	SaveToO5m(osmData, outfi);
	outfi.close();
	cout << "Written o5mtest2.o5m" << endl;

	std::filebuf outfi2;
	outfi2.open("o5mtest2.osm", std::ios::out);
	SaveToOsmXml(osmData, outfi2);
	outfi2.close();
	cout << "Written o5mtest2.osm" << endl;
}
