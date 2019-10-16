
all: example selftest dectest examplexml exampleosmchange o5mconvert

%.co: %.c
	gcc -fPIC -Wall -c -o $@ $<
%.o: %.cpp
	g++ -fPIC -Wall -c -std=c++11 -o $@ $<

selftest: o5m.o varint.o selftest.o OsmData.o
	g++ $^ -Wall -std=c++11 -o $@
dectest: o5m.o varint.o dectest.o OsmData.o
	g++ $^ -Wall -std=c++11 -o $@
example: o5m.o varint.o OsmData.o osmxml.o example.o utils.o iso8601lib/iso8601.co
	g++ $^ -I/usr/include/libxml2 -lexpat -Wall -std=c++11 -o $@
examplexml: o5m.o varint.o OsmData.o osmxml.o examplexml.o utils.o iso8601lib/iso8601.co
	g++ $^ -I/usr/include/libxml2 -lexpat -Wall -std=c++11 -o $@
exampleosmchange: o5m.o varint.o OsmData.o osmxml.o exampleosmchange.o utils.o iso8601lib/iso8601.co
	g++ $^ -I/usr/include/libxml2 -lexpat -Wall -std=c++11 -o $@
o5mconvert: o5m.o varint.o OsmData.o osmxml.o utils.o iso8601lib/iso8601.co o5mconvert.cpp
	g++ $^ -I/usr/include/libxml2 -lexpat -lboost_program_options -Wall -std=c++11 -o $@
pbfx: o5m.o varint.o OsmData.o osmxml.o utils.o iso8601lib/iso8601.co pbf.o pbf/fileformat.pb.cc pbf/osmformat.pb.cc
	g++ $^ -I/usr/include/libxml2 -lexpat -g -Wall -std=c++11 -lprotobuf -lboost_iostreams -o $@

