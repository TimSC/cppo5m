
all: example selftest dectest
selftest: o5m.cpp varint.cpp selftest.cpp
	g++ $^ -Wall -o $@
dectest: o5m.cpp varint.cpp dectest.cpp
	g++ $^ -Wall -o $@
example: o5m.cpp varint.cpp OsmData.cpp example.cpp
	g++ $^ -Wall -o $@

