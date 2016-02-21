#include "OsmData.h"
#include <fstream>
#include <iostream>
using namespace std;

int main()
{
	class OsmData osmData;
	std::ifstream infi("o5mtest.o5m");
	osmData.LoadFromO5m(infi);
	cout << "nodes " << osmData.nodes.size() << endl;
	cout << "ways " << osmData.ways.size() << endl;
	cout << "relations " << osmData.relations.size() << endl;
	std::ofstream outfi("o5mtest2.o5m");
	osmData.SaveToO5m(outfi);
	outfi.close();
}
