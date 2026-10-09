#include "fuzz_common.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	FuzzDecode(OsmFormat::Pbf, data, size);
	return 0;
}
