#include "fuzz_common.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	OsmChange decoded;
	try
	{
		LoadFromOsmChangeXml(std::string((const char *)data, size), decoded);
	}
	catch(const OsmDecodeError &)
	{
		return 0;
	}

	//Whatever was accepted must be writable and readable again
	std::ostringstream out;
	SaveToOsmChangeXml(decoded, *out.rdbuf());
	OsmChange again;
	LoadFromOsmChangeXml(out.str(), again);
	if(again.blocks.size() != decoded.blocks.size())
		__builtin_trap();
	return 0;
}
