
all: example selftest dectest examplexml
selftest: o5m.cpp varint.cpp selftest.cpp
	g++ $^ -Wall -o $@
dectest: o5m.cpp varint.cpp dectest.cpp
	g++ $^ -Wall -o $@
example: o5m.cpp varint.cpp OsmData.cpp osmxml.cpp example.cpp
	g++ $^ -Wall -o $@
examplexml: o5m.cpp varint.cpp OsmData.cpp osmxml.cpp examplexml.cpp
	g++ $^ -Wall -o $@

