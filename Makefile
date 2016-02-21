
all: example selftest dectest
selftest: o5m.cpp varint.cpp selftest.cpp
	g++ o5m.cpp varint.cpp selftest.cpp -o selftest
dectest: o5m.cpp varint.cpp dectest.cpp
	g++ o5m.cpp varint.cpp dectest.cpp -o dectest
example: o5m.cpp varint.cpp OsmData.cpp example.cpp
	g++ o5m.cpp varint.cpp OsmData.cpp example.cpp -o example

