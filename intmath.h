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

///Rounds a scaled coordinate to an integer. Throws std::invalid_argument if
///the value is not a number or is too large to store.
inline int64_t RoundCoord(double scaled)
{
	if(!std::isfinite(scaled) || std::fabs(scaled) >= 9.2e18)
		throw std::invalid_argument("Coordinate is out of range");
	return std::llround(scaled);
}

#endif //CPPO5M_INTMATH_H
