#include "OsmData.h"
#include <fstream>
#include <iostream>
using namespace std;

int main()
{
	shared_ptr<class OsmData> osmData(new class OsmData());
	std::filebuf infi;
	infi.open("o5mtest.o5m", std::ios::in);
	LoadFromO5m(infi, osmData);
	cout << "nodes " << osmData->nodes.size() << endl;
	cout << "ways " << osmData->ways.size() << endl;
	cout << "relations " << osmData->relations.size() << endl;
	std::filebuf outfi;
	outfi.open("o5mtest2.o5m", std::ios::out);
	SaveToO5m(*osmData, outfi);
	outfi.close();
}
