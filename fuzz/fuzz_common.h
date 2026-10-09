#ifndef CPPO5M_FUZZ_COMMON_H
#define CPPO5M_FUZZ_COMMON_H

// Shared by the fuzz targets. A decoder given arbitrary bytes must either
// succeed or throw OsmDecodeError. Anything else, such as a crash, a sanitizer
// report, another exception type or a hang, is a bug and stops the fuzzer.

#include <cstdint>
#include <sstream>
#include <string>
#include "../cppo5m.h"

///Decodes data in the given format and, if that succeeds, checks that what
///came out can be written in every format and read back.
inline void FuzzDecode(OsmFormat format, const uint8_t *data, size_t size)
{
	OsmData decoded;
	try
	{
		std::string input((const char *)data, size);
		std::istringstream in(input);
		auto decoder = MakeDecoder(format, *in.rdbuf(), decoded);
		decoder->Decode();
	}
	catch(const OsmDecodeError &)
	{
		return;
	}

	const OsmFormat formats[] = {OsmFormat::O5m, OsmFormat::OsmXml, OsmFormat::Pbf, OsmFormat::OsmJson};
	for(OsmFormat outFormat : formats)
	{
		auto sink = std::make_shared<StringSink>();
		try
		{
			auto encoder = MakeEncoder(outFormat, sink);
			decoded.StreamTo(*encoder);
		}
		catch(const std::invalid_argument &)
		{
			continue; //For example a zero byte in a string, which o5m cannot hold
		}
		catch(const std::range_error &)
		{
			continue; //A value too large for PBF
		}

		//Our own output must always decode
		OsmData again;
		std::istringstream in(sink->data);
		auto decoder = MakeDecoder(outFormat, *in.rdbuf(), again);
		decoder->Decode();
		if(again.nodes.size() != decoded.nodes.size() || again.ways.size() != decoded.ways.size() ||
			again.relations.size() != decoded.relations.size())
			__builtin_trap();
	}
}

#endif //CPPO5M_FUZZ_COMMON_H
