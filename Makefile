
all: example selftest dectest examplexml exampleosmchange
selftest: o5m.cpp varint.cpp selftest.cpp
	g++ $^ -Wall -std=c++11 -o $@
dectest: o5m.cpp varint.cpp dectest.cpp
	g++ $^ -Wall -std=c++11 -o $@
example: o5m.cpp varint.cpp OsmData.cpp osmxml.cpp example.cpp iso8601lib/iso8601.c
	g++ $^ -I/usr/include/libxml2 -lexpat -Wall -std=c++11 -o $@
examplexml: o5m.cpp varint.cpp OsmData.cpp osmxml.cpp examplexml.cpp iso8601lib/iso8601.c
	g++ $^ -I/usr/include/libxml2 -lexpat -Wall -std=c++11 -o $@
exampleosmchange: o5m.cpp varint.cpp OsmData.cpp osmxml.cpp exampleosmchange.cpp iso8601lib/iso8601.c
	g++ $^ -g -I/usr/include/libxml2 -lexpat -Wall -std=c++11 -o $@

