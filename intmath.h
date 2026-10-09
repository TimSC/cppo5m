#ifndef CPPO5M_INTMATH_H
#define CPPO5M_INTMATH_H

// Integer helpers for the delta coded formats. Internal to the library.
// Deltas read from a file can be anything, so sums and differences wrap
// instead of overflowing, which would be undefined for signed integers.

#include <cmath>
#include <cstdint>
#include <stdexcept>

inline int64_t WrapAdd(int64_t a, int64_t b)
{
	return (int64_t)((uint64_t)a + (uint64_t)b);
}

inline int64_t WrapSub(int64_t a, int64_t b)
{
	return (int64_t)((uint64_t)a - (uint64_t)b);
}

///Adds a delta to a node coordinate the way o5m defines it: in 32 bit
///arithmetic, wrapping round. A step across the antimeridian from +179 to -179
///degrees is stored as a positive delta that only gives the right answer when
///the sum wraps. Deltas written with 64 bit arithmetic differ only in the bits
///above the 32nd, so they read correctly too.
inline int64_t WrapAdd32(int64_t value, int64_t delta)
{
	return (int32_t)((uint32_t)value + (uint32_t)delta);
}

///The delta from one node coordinate to the next, in 32 bit arithmetic as above.
inline int64_t WrapSub32(int64_t value, int64_t previous)
{
	return (int32_t)((uint32_t)value - (uint32_t)previous);
}

///Rounds a degree value to o5m's units of 100 nanodegrees. Throws
///std::invalid_argument if it is not a number or does not fit in the 32 bits
///the format uses for node coordinates.
inline int64_t RoundCoord32(double degrees)
{
	double scaled = degrees * 1e7;
	if(!std::isfinite(scaled) || std::fabs(scaled) > 2147483647.0)
		throw std::invalid_argument("Coordinate is out of range");
	return std::llround(scaled);
}

///Rounds a scaled coordinate to an integer. Throws std::invalid_argument if
///the value is not a number or is too large to store.
inline int64_t RoundCoord(double scaled)
{
	if(!std::isfinite(scaled) || std::fabs(scaled) >= 9.2e18)
		throw std::invalid_argument("Coordinate is out of range");
	return std::llround(scaled);
}

#endif //CPPO5M_INTMATH_H
