# cppo5m

Reading and writing OpenStreetMap data in C++: o5m, OSM XML, osmChange XML, PBF and OSM JSON.

Data moves as a stream. A decoder reads a file and calls a *handler* once for each node, way and relation; an encoder is a handler that writes a file. Connecting one to the other converts between formats without holding the map in memory, and `OsmData` is a handler that keeps everything when that is what you want.

## Building

	sudo apt install g++ make libexpat1-dev libprotobuf-dev protobuf-compiler zlib1g-dev rapidjson-dev

	git clone https://github.com/TimSC/cppo5m.git --recursive
	cd cppo5m
	make
	make test

This builds the static library `libcppo5m.a`, the `o5mconvert` tool, the examples and the tests. Link your program with:

	g++ -std=c++17 yours.cpp libcppo5m.a -lexpat -lprotobuf -lz

To install the library, its headers and `o5mconvert` (under `/usr/local` unless `PREFIX` says otherwise):

	sudo make install

after which programs include `<cppo5m/cppo5m.h>` and link with `-lcppo5m -lexpat -lprotobuf -lz`.

There is also a CMake build, which runs the same tests through `ctest` and installs a package other CMake projects can find:

	cmake -B build && cmake --build build && ctest --test-dir build
	sudo cmake --install build

	find_package(cppo5m REQUIRED)
	target_link_libraries(yours PRIVATE cppo5m::cppo5m)

Alternatively, `add_subdirectory(cppo5m)` builds just the library as part of your project. Set `CPPO5M_PYTHON=ON` to include `PySink`.

If your protobuf library is a different version from the one the sources in `pbf/` were generated with, regenerate them with `make proto`.

## Loading and saving

	#include "cppo5m.h"
	#include <fstream>
	#include <iostream>

	int main()
	{
		std::filebuf infi;
		infi.open("tests/data/o5mtest.o5m", std::ios::in | std::ios::binary);

		OsmData osmData;
		LoadFromO5m(infi, osmData);
		std::cout << "nodes " << osmData.nodes.size() << std::endl;
		std::cout << "ways " << osmData.ways.size() << std::endl;
		std::cout << "relations " << osmData.relations.size() << std::endl;

		std::filebuf outfi;
		outfi.open("out.osm", std::ios::out | std::ios::binary);
		SaveToOsmXml(osmData, outfi);
	}

There are matching `LoadFromOsmXml`, `LoadFromPbf`, `LoadFromOsmJson`, `LoadFromOsmChangeXml`, `SaveToO5m`, `SaveToPbf`, `SaveToOsmJson` and `SaveToOsmChangeXml` functions. `FormatFromFilename`, `MakeDecoder` and `MakeEncoder` choose a format at run time.

## Streaming

Derive from `IDataStreamHandler` and override what you need:

	class CountNodes : public IDataStreamHandler
	{
	public:
		size_t count = 0;
		void StoreNode(const OsmNode &node) override { count ++; }
	};

	CountNodes counter;
	PbfDecode decoder(infi, counter);
	decoder.Decode();

To convert, give the decoder an encoder as its handler:

	O5mEncode encoder(outfi);
	OsmXmlDecode decoder(infi, encoder);
	decoder.Decode();

`Decode()` reads everything. `DecodeNext()` reads one piece and returns false at the end, which lets you stop early or interleave other work. Handlers can be chained: `DeduplicateOsm` and `SortOsm` in `filters.h` pass a stream on to another handler.

Encoders write to a `ByteSink`. Constructing one from a `std::streambuf` is the usual case; `StringSink` collects output in memory, and `PySink` (built when `PYTHON_AWARE` is defined) writes to a Python file object.

## Errors

Decoders throw `OsmDecodeError` for input that is malformed or cut short. XML or JSON read from an untrusted source can be bounded with `OsmXmlLimits`; exceeding a limit throws `OsmLimitError`, a kind of `OsmDecodeError` that names the limit. An exception thrown by your handler passes through the decoder unchanged. Encoders throw if the output cannot be written, or if asked to write something the format cannot hold.

The decoders read untrusted input, so each has a fuzz target; see `fuzz/README.md`.

## Layout

| File | Contents |
|---|---|
| `cppo5m.h` | Includes everything below |
| `model.h` | `OsmNode`, `OsmWay`, `OsmRelation`, `OsmData`, `OsmChange` |
| `handler.h` | `IDataStreamHandler`, `IOsmChangeHandler` |
| `decoder.h`, `encoder.h` | Base classes and the error types |
| `sink.h`, `pysink.h` | Where encoders write |
| `o5m.h`, `osmxml.h`, `osmchangexml.h`, `pbf.h` | One decoder and one encoder for each format |
| `osmjson.h` | Decoder and encoder for OSM JSON, as returned by the OSM API and Overpass |
| `filters.h` | `FindBbox`, `DeduplicateOsm`, `SortOsm` |
| `io.h` | Whole-document helpers and format factories |
| `tools/`, `examples/`, `tests/`, `fuzz/` | `o5mconvert`, example programs, tests and sample data, fuzz targets |

## o5mconvert

A conversion tool modelled after osmconvert, mainly useful for testing. The format comes from the file extension unless given explicitly, and `-` reads from the console:

	./o5mconvert input.osm -o output.pbf
	gunzip -c data.o5m.gz | ./o5mconvert - --in-o5m --out-osm > data.osm

`--sort` orders the output by ID, holding the whole file in memory. `--out-null` reads the input without writing anything, to check that a file decodes.

## Format notes

* Coordinates are stored to seven decimal places in o5m and, with the default settings, in PBF.
* PBF keeps only the first bounding box, and stores versions and user IDs in 32 bits; the encoder refuses larger values.
* The PBF encoder writes blocks of up to 8000 objects, as the format recommends, and holds one block in memory at a time. `maxBlockObjects` changes this.
* XML output is always well formed: bytes that are not valid UTF-8, and characters XML 1.0 does not allow, are written as U+FFFD. o5m and PBF store strings as they are.
* OSM JSON leaves out metadata that is zero or empty, empty tags, empty way node and relation member lists, and the position of a deleted node, as the OSM API does. Only bounds sent before the first object are written.
* The JSON decoder skips members it has no place for, such as the `center` and `geometry` Overpass can add. It reads the whole document in its first `DecodeNext` call, because the parser cannot pause; objects still reach the handler one at a time.
* Coordinates must be finite numbers; the XML decoder and the o5m and PBF encoders refuse anything else.
* The PBF encoder does not write the visible flag unless `encodeHistorical` is set.
* In o5m an object with version zero has no room for its timestamp, changeset or user.
