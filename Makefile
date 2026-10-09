CXX ?= g++
CC ?= gcc
CXXFLAGS ?= -O2 -g
CPPO5M_CXXFLAGS = -std=c++17 -Wall -Wextra -Wno-unused-parameter -fPIC $(CXXFLAGS)
LIBS = -lexpat -lprotobuf -lz

LIB = libcppo5m.a
OBJS = model.o sink.o decoder.o varint.o o5m.o osmxml.o osmchangexml.o osmjson.o pbf.o filters.o io.o \
	pbf/fileformat.pb.o pbf/osmformat.pb.o iso8601lib/iso8601.co
HEADERS = $(wildcard *.h) $(wildcard pbf/*.h)
# Headers a program using the library includes
PUBLIC_HEADERS = cppo5m.h model.h handler.h sink.h pysink.h decoder.h encoder.h o5m.h osmxml.h \
	osmchangexml.h osmjson.h pbf.h filters.h io.h varint.h fixeddeque.h
PREFIX ?= /usr/local
DESTDIR ?=

EXAMPLES = examples/example examples/examplestream examples/exampleosmchange
TESTS = tests/test_cppo5m tests/test_fixeddeque
TOOLS = o5mconvert

all: $(LIB) $(TOOLS) $(EXAMPLES) $(TESTS)

# Generated protobuf sources produce warnings we cannot fix
pbf/%.pb.o: pbf/%.pb.cc
	$(CXX) -std=c++17 -fPIC $(CXXFLAGS) -w -c -o $@ $<
%.co: %.c
	$(CC) -fPIC -Wall -O2 -c -o $@ $<
%.o: %.cpp $(HEADERS)
	$(CXX) $(CPPO5M_CXXFLAGS) -c -o $@ $<

$(LIB): $(OBJS)
	rm -f $@
	ar rcs $@ $^

o5mconvert: tools/o5mconvert.o $(LIB)
	$(CXX) $^ $(LIBS) -o $@
examples/%: examples/%.o $(LIB)
	$(CXX) $^ $(LIBS) -o $@
tests/%: tests/%.o $(LIB)
	$(CXX) $^ $(LIBS) -o $@

# Run the tests from this directory: they read files under tests/data
test: $(TESTS)
	./tests/test_fixeddeque
	./tests/test_cppo5m

# Installs the library, its headers and o5mconvert. Programs then build with
#   g++ yours.cpp -lcppo5m -lexpat -lprotobuf -lz   and   #include <cppo5m/cppo5m.h>
install: $(LIB) o5mconvert
	install -d $(DESTDIR)$(PREFIX)/lib $(DESTDIR)$(PREFIX)/include/cppo5m $(DESTDIR)$(PREFIX)/bin
	install -m 644 $(LIB) $(DESTDIR)$(PREFIX)/lib/
	install -m 644 $(PUBLIC_HEADERS) $(DESTDIR)$(PREFIX)/include/cppo5m/
	install -m 755 o5mconvert $(DESTDIR)$(PREFIX)/bin/

# Fuzz the decoders with libFuzzer; needs clang. Run one with, for example,
#   ./fuzz/fuzz_o5m fuzz/corpus/o5m -max_total_time=60
FUZZ_CXX ?= clang++
FUZZ_FLAGS = -std=c++17 -g -O1 -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined
FUZZ_SRCS = model.cpp sink.cpp decoder.cpp varint.cpp o5m.cpp osmxml.cpp osmchangexml.cpp osmjson.cpp pbf.cpp \
	filters.cpp io.cpp pbf/fileformat.pb.cc pbf/osmformat.pb.cc
FUZZERS = fuzz/fuzz_o5m fuzz/fuzz_osmxml fuzz/fuzz_osmchangexml fuzz/fuzz_pbf

fuzz/iso8601.fuzz.o: iso8601lib/iso8601.c
	$(FUZZ_CXX:clang++=clang) -g -O1 -fsanitize=address,undefined -c -o $@ $<
fuzz/fuzz_%: fuzz/fuzz_%.cpp fuzz/fuzz_common.h fuzz/iso8601.fuzz.o $(FUZZ_SRCS) $(HEADERS)
	$(FUZZ_CXX) $(FUZZ_FLAGS) -w $< $(FUZZ_SRCS) fuzz/iso8601.fuzz.o $(LIBS) -o $@
fuzz: $(FUZZERS)

# Regenerate the protobuf sources if your protobuf library version differs
proto:
	protoc -I=proto proto/osmformat.proto proto/fileformat.proto --cpp_out=pbf

clean:
	rm -f *.o pbf/*.o iso8601lib/*.co tools/*.o examples/*.o tests/*.o fuzz/*.o \
		$(LIB) $(TOOLS) $(EXAMPLES) $(TESTS) $(FUZZERS) example-out.o5m example-out.osm

.PHONY: all test install fuzz proto clean
.PRECIOUS: %.o pbf/%.pb.o
