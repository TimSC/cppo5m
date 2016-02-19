#include <stdint.h>
#include <sstream>
#include <string>

uint64_t DecodeVarint(std::istream &str);
uint64_t DecodeVarint(const char *str);
int64_t DecodeZigzag(std::istream &str);
int64_t DecodeZigzag(const char *str);
void EncodeVarint(uint64_t val, std::string &out);
std::string EncodeVarint(uint64_t val);
void EncodeZigzag(int64_t val, std::string &out);
std::string EncodeZigzag(int64_t val);

