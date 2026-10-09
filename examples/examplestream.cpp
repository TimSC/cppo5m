// Handle a map file as a stream, without holding it in memory: count the
// objects and print the tags of each named node.
#include <fstream>
#include <iostream>
#include "../cppo5m.h"
using namespace std;

class NamedNodePrinter : public IDataStreamHandler
{
public:
	size_t nodes = 0, ways = 0, relations = 0;

	void StoreBounds(const Bounds &bounds) override
	{
		cout << "bounds " << bounds.minLon << "," << bounds.minLat << ","
			<< bounds.maxLon << "," << bounds.maxLat << endl;
	}

	void StoreNode(const OsmNode &node) override
	{
		nodes ++;
		TagMap::const_iterator it = node.tags.find("name");
		if(it != node.tags.end())
			cout << "node " << node.objId << " at " << node.lat << "," << node.lon
				<< " is named " << it->second << endl;
	}

	void StoreWay(const OsmWay &) override { ways ++; }
	void StoreRelation(const OsmRelation &) override { relations ++; }
};

int main(int argc, char **argv)
{
	string inFilename = "tests/data/o5mtest.osm";
	if(argc > 1)
		inFilename = argv[1];

	std::filebuf infi;
	if(!infi.open(inFilename, std::ios::in | std::ios::binary))
	{
		cerr << "Cannot open " << inFilename << endl;
		return 1;
	}

	NamedNodePrinter printer;
	std::unique_ptr<OsmDecoder> decoder = MakeDecoder(FormatFromFilename(inFilename), infi, printer);
	try
	{
		decoder->Decode();
	}
	catch(const OsmDecodeError &err)
	{
		cerr << "Could not read " << inFilename << ": " << err.what() << endl;
		return 1;
	}

	cout << "nodes " << printer.nodes << endl;
	cout << "ways " << printer.ways << endl;
	cout << "relations " << printer.relations << endl;
	return 0;
}
