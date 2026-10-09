#ifndef CPPO5M_SINK_H
#define CPPO5M_SINK_H

#include <cstddef>
#include <memory>
#include <ostream>
#include <streambuf>
#include <string>

///Where an encoder writes its bytes. Encoders are written once against this
///interface; writing to a stream, a string or a Python file object is a
///different sink, not a different encoder.
class ByteSink
{
public:
	virtual ~ByteSink() {}

	///Writes len bytes or throws std::runtime_error.
	virtual void Write(const char *data, size_t len) = 0;
	void Write(const std::string &data) { this->Write(data.data(), data.size()); }
};

///Writes to a std::streambuf, which must outlive the sink.
class StreamSink : public ByteSink
{
private:
	std::ostream handle;

public:
	explicit StreamSink(std::streambuf &buffer);
	using ByteSink::Write;
	void Write(const char *data, size_t len) override;
};

///Collects output in memory.
class StringSink : public ByteSink
{
public:
	std::string data;

	using ByteSink::Write;
	void Write(const char *bytes, size_t len) override { data.append(bytes, len); }
};

#endif //CPPO5M_SINK_H
