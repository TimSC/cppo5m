
all: example selftest dectest examplexml exampleosmchange

%.co: %.c
	gcc -fPIC -Wall -c -o $@ $<
%.o: %.cpp
	g++ -fPIC -Wall -c -std=c++11 -o $@ $<

selftest: o5m.o varint.o selftest.o
	g++ $^ -Wall -std=c++11 -o $@
dectest: o5m.o varint.o dectest.o
	g++ $^ -Wall -std=c++11 -o $@
example: o5m.o varint.o OsmData.o osmxml.o example.o iso8601lib/iso8601.co
	g++ $^ -I/usr/include/libxml2 -lexpat -Wall -std=c++11 -o $@
examplexml: o5m.o varint.o OsmData.o osmxml.o examplexml.o iso8601lib/iso8601.co
	g++ $^ -I/usr/include/libxml2 -lexpat -Wall -std=c++11 -o $@
exampleosmchange: o5m.o varint.o OsmData.o osmxml.o exampleosmchange.o iso8601lib/iso8601.co
	g++ $^ -g -I/usr/include/libxml2 -lexpat -Wall -std=c++11 -o $@

