// Read an osmChange document and list its action blocks.
#include <fstream>
#include <iostream>
#include "../cppo5m.h"
using namespace std;

int main(int argc, char **argv)
{
	string inFilename = "tests/data/examplechange.osm";
	if(argc > 1)
		inFilename = argv[1];

	std::filebuf infi;
	if(!infi.open(inFilename, std::ios::in | std::ios::binary))
	{
		cerr << "Cannot open " << inFilename << endl;
		return 1;
	}

	OsmChange osmChange;
	LoadFromOsmChangeXml(infi, osmChange);

	cout << osmChange.blocks.size() << " blocks" << endl;
	for(const OsmChangeBlock &block : osmChange.blocks)
	{
		cout << block.action << (block.ifUnused ? " (if unused)" : "") << endl;
		cout << "nodes " << block.data.nodes.size() << endl;
		cout << "ways " << block.data.ways.size() << endl;
		cout << "relations " << block.data.relations.size() << endl;
	}
	return 0;
}
