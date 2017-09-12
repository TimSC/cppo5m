#include "OsmData.h"
#include "osmxml.h"
#include <fstream>
#include <iostream>
using namespace std;

int main()
{
	class OsmData osmData;
	std::filebuf infi;
	infi.open("example.osm", std::ios::in);
	osmData.LoadFromOsmXml(infi);
	cout << "nodes " << osmData.nodes.size() << endl;
	cout << "ways " << osmData.ways.size() << endl;
	cout << "relations " << osmData.relations.size() << endl;
	std::filebuf outfi;
	outfi.open("example2.osm", std::ios::out);
	osmData.SaveToOsmXml(outfi);
	outfi.close();
}
