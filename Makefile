
all: example selftest dectest examplexml
selftest: o5m.cpp varint.cpp selftest.cpp
	g++ $^ -Wall -o $@
dectest: o5m.cpp varint.cpp dectest.cpp
	g++ $^ -Wall -o $@
example: o5m.cpp varint.cpp OsmData.cpp osmxml.cpp example.cpp iso8601.cpp
	g++ $^ -I/usr/include/libxml2 -lexpat -Wall -std=c++11 -o $@
examplexml: o5m.cpp varint.cpp OsmData.cpp osmxml.cpp examplexml.cpp iso8601.cpp
	g++ $^ -I/usr/include/libxml2 -lexpat -Wall -std=c++11 -o $@

