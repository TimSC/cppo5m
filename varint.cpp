#include "varint.h"
#include <sstream>
#include <stdexcept>

uint64_t DecodeVarint(std::istream &str)
{
	uint64_t total = 0;
	unsigned offset = 0;
	while(true)
	{
		int raw = str.get();
		if(raw == std::char_traits<char>::eof())
			throw std::runtime_error("End of input inside a varint");
		uint64_t val = (unsigned char)raw;
		//The tenth byte may only carry the one remaining bit
		if(offset >= 64 || (offset == 63 && (val & 0x7e) != 0))
			throw std::runtime_error("Varint is too long");
		total |= (val & 0x7f) << offset;
		if((val & 0x80) == 0)
			return total;
		offset += 7;
	}
}

uint64_t DecodeVarint(const std::string &str)
{
	std::istringstream ss(str);
	return DecodeVarint(ss);
}

int64_t DecodeZigzag(std::istream &str)
{
	uint64_t zz = DecodeVarint(str);
	return (int64_t)(zz >> 1) ^ -(int64_t)(zz & 1);
}

int64_t DecodeZigzag(const std::string &str)
{
	std::istringstream ss(str);
	return DecodeZigzag(ss);
}

void AppendVarint(uint64_t val, std::string &out)
{
	while(val >= 0x80)
	{
		out.push_back((char)((val & 0x7f) | 0x80));
		val >>= 7;
	}
	out.push_back((char)val);
}

void AppendZigzag(int64_t val, std::string &out)
{
	AppendVarint(((uint64_t)val << 1) ^ (uint64_t)(val >> 63), out);
}

std::string EncodeVarint(uint64_t val)
{
	std::string out;
	AppendVarint(val, out);
	return out;
}

std::string EncodeZigzag(int64_t val)
{
	std::string out;
	AppendZigzag(val, out);
	return out;
}
