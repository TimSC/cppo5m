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

	std::filebuf infi;
	infi.open("examplechange.osm", std::ios::in);

	class OsmChange osmChange;
	LoadFromOsmChangeXml(infi, &osmChange);

	cout << osmChange.blocks.size() << endl;
	for(size_t i=0;i<osmChange.blocks.size();i++)
	{
		cout << osmChange.actions[i] << endl;
		const class OsmData &block = osmChange.blocks[i];
		cout << "nodes " << block.nodes.size() << endl;
		cout << "ways " << block.ways.size() << endl;
		cout << "relations " << block.relations.size() << endl;
	}
}
