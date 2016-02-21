#include "OsmData.h"
#include <fstream>

int main()
{
	class OsmData osmData;
	std::ifstream infi("o5mtest.o5m");
	osmData.LoadFromO5m(infi);
/*	fi = open("o5mtest.o5m", "rb")
	osmData = OsmData.OsmData()
	osmData.LoadFromO5m(fi)
	print ("nodes", len(osmData.nodes))
	print ("ways", len(osmData.ways))
	print ("relations", len(osmData.relations))

	fi2 = open("o5mtest2.o5m", "wb")
	osmData.SaveToO5m(fi2)
	fi2.close()

	fi3 = open("o5mtest2.osm", "wb")
	osmData.SaveToOsmXml(fi3)
	fi3.close()

	print ("Read data back")
	fi4 = open("o5mtest2.o5m", "rb")
	osmData2 = OsmData.OsmData()
	osmData2.LoadFromO5m(fi4)
	print ("nodes", len(osmData2.nodes))
	print ("ways", len(osmData2.ways))
	print ("relations", len(osmData2.relations))*/
}
