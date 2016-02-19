#include <stdint.h>
#include <stdio.h>
#include <stdexcept>
#include <sstream>

const int INTERNAL_BUFF_SIZE = 16;

// ****** Varint and zigzag encodings ******

uint64_t DecodeVarint(std::istream &str)
{
	unsigned contin = 1;
	size_t offset = 0;
	uint64_t total = 0;
	while (contin) {
		char rawBuff = str.get();
		if(str.fail())
			throw std::runtime_error("Read result has unexpected length");

		uint64_t val = *(unsigned char *)(&rawBuff);
		contin = (val & 0x80) != 0;
		total += (val & 0x7f) << offset;
		offset += 7;
	}

	return total;
}

uint64_t DecodeVarint(const char *str)
{
	std::stringstream test(str);
	std::istringstream ss(test.str());
	return DecodeVarint(ss);
}

int64_t DecodeZigzag(std::istream &str)
{
	unsigned char contin = 1;
	uint64_t offset = 0;
	uint64_t total = 0;
	while (contin) {
		char rawBuff = str.get();
		if(str.fail())
			throw std::runtime_error("Read result has unexpected length");

		uint64_t val = *(unsigned char *)(&rawBuff);
		contin = (val & 0x80) != 0;
		total += (val & 0x7f) << offset;
		offset += 7;
	}

	return (total >> 1) ^ (-(total & 1));
}

int64_t DecodeZigzag(const char *str)
{
	std::stringstream test(str);
	std::istringstream ss(test.str());
	return DecodeZigzag(ss);
}

void EncodeVarint(uint64_t val, std::string &out)
{
	out.resize(INTERNAL_BUFF_SIZE);
	unsigned char more = 1;
	unsigned int cursor = 0;
	
	while (more) {
		unsigned char sevenBits = val & 0x7f;
		val = val >> 7;
		more = val != 0;
		if(cursor < out.capacity()) {
			out[cursor] = (more << 7) + sevenBits;
			cursor ++;	
		}
		else
			throw std::runtime_error("Internal buffer overflow while encoding varint");
	}

	out.resize(cursor);
}

std::string EncodeVarint(uint64_t val)
{
	std::string out;
	EncodeVarint(val, out);
	return out;
}

void EncodeZigzag(int64_t val, std::string &out)
{
	out.resize(INTERNAL_BUFF_SIZE);
	unsigned char more = 1;
	unsigned int cursor = 0;
	uint64_t zz = 0;

	zz = (val << 1) ^ (val >> (sizeof(int64_t)*8-1));

	while (more) {
		unsigned char sevenBits = zz & 0x7f;
		zz = zz >> 7;
		more = zz != 0;
		if(cursor < out.capacity()) {
			out[cursor] = (more << 7) + sevenBits;
			cursor ++;	
		}
		else
			throw std::runtime_error("Internal buffer overflow while encoding varint");
	}

	out.resize(cursor);	
}

std::string EncodeZigzag(int64_t val)
{
	std::string out;
	EncodeZigzag(val, out);
	return out;
}


