# Fuzzing

The decoders read untrusted input, so each has a libFuzzer target. Build them
from the repository root with clang:

	make fuzz

Then run one against its seed corpus, for example for a minute:

	./fuzz/fuzz_o5m fuzz/corpus/o5m -max_total_time=60
	./fuzz/fuzz_osmxml fuzz/corpus/osmxml -max_total_time=60
	./fuzz/fuzz_osmchangexml fuzz/corpus/osmchangexml -max_total_time=60
	./fuzz/fuzz_osmjson fuzz/corpus/osmjson -max_total_time=60
	./fuzz/fuzz_pbf fuzz/corpus/pbf -max_total_time=60

A target fails if a decoder crashes, trips a sanitizer, hangs, or throws
anything other than `OsmDecodeError`. Input that decodes is also written out in
every format and read back. libFuzzer saves a failing input as `crash-*`; pass
that file to the target to reproduce it. New inputs it finds are added to the
corpus directory, so keep those out of commits unless they are worth keeping as
seeds.
