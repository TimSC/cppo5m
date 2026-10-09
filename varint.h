#ifndef CPPO5M_VARINT_H
#define CPPO5M_VARINT_H

#include <cstdint>
#include <istream>
#include <string>

// Variable length integers as used by o5m: seven bits per byte, least
// significant group first, with the top bit set on every byte but the last.
// Signed values are zigzag coded first so small magnitudes stay short.

///Reads one unsigned varint. Throws std::runtime_error if the stream ends
///first or the value does not fit in 64 bits.
uint64_t DecodeVarint(std::istream &str);
uint64_t DecodeVarint(const std::string &str);
int64_t DecodeZigzag(std::istream &str);
int64_t DecodeZigzag(const std::string &str);

///Appends the encoded value to out.
void AppendVarint(uint64_t val, std::string &out);
void AppendZigzag(int64_t val, std::string &out);

std::string EncodeVarint(uint64_t val);
std::string EncodeZigzag(int64_t val);

#endif //CPPO5M_VARINT_H
