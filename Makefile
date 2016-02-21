
all: example
	g++ o5m.cpp varint.cpp selftest.cpp -o selftest
	g++ o5m.cpp varint.cpp test.cpp -o test
example: varint.cpp OsmData.cpp example.cpp
	g++ o5m.cpp varint.cpp OsmData.cpp example.cpp -o example

