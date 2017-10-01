#include "OsmData.h"
#include "osmxml.h"
#include <fstream>
#include <iostream>
using namespace std;

int main()
{
	shared_ptr<class OsmData> osmData(new class OsmData());
	std::filebuf infi;
	infi.open("examplechange.osm", std::ios::in);

	std::shared_ptr<class OsmChange> osmChange(new class OsmChange());
	LoadFromOsmChangeXml(infi, osmChange);

	cout << osmChange->blocks.size() << endl;
	for(size_t i=0;i<osmChange->blocks.size();i++)
	{
		cout << osmChange->actions[i] << endl;
		const class OsmData &block = osmChange->blocks[i];
		cout << "nodes " << block.nodes.size() << endl;
		cout << "ways " << block.ways.size() << endl;
		cout << "relations " << block.relations.size() << endl;
	}
}
