# cppo5m
Encoding and decoding o5m/xml OSM map format in C++. pbf is support read only.

	sudo apt install libboost-program-options-dev libprotobuf-dev zlib1g-dev libboost-iostreams-dev

	git clone https://github.com/TimSC/cppo5m.git --recursive

Example:

	#include "OsmData.h"
	#include <fstream>
	#include <iostream>
	using namespace std;

	int main()
	{
		class OsmData osmData;
		std::filebuf infi;
		infi.open("o5mtest.o5m", std::ios::in);
		osmData.LoadFromO5m(infi);
		cout << "nodes " << osmData.nodes.size() << endl;
		cout << "ways " << osmData.ways.size() << endl;
		cout << "relations " << osmData.relations.size() << endl;
		std::filebuf outfi;
		outfi.open("o5mtest2.o5m", std::ios::out);
		osmData.SaveToO5m(outfi);
		outfi.close();
	}

o5mconvert is a conversion tool modelled after osmconvert. It only supports o5m and osm so far. It is mainly useful for testing. Compressed input is not supported but streaming from the console is:

	gunzip -c data.o5m.gz | ./o5mconvert - --in-o5m

To regenerate sources in the pbf folder (required if your protobuf library version does not match):

* protoc -I=proto proto/osmformat.proto --cpp_out=pbf

* protoc -I=proto proto/fileformat.proto --cpp_out=pbf

