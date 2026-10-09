#include "sink.h"
#include <stdexcept>

StreamSink::StreamSink(std::streambuf &buffer) : handle(&buffer)
{

}

void StreamSink::Write(const char *data, size_t len)
{
	this->handle.write(data, len);
	if(this->handle.fail())
		throw std::runtime_error("Error writing to output stream");
}
