#ifndef CPPO5M_DECODER_H
#define CPPO5M_DECODER_H

#include <stdexcept>
#include <string>
#include "handler.h"

///Input could not be decoded: it is malformed, truncated or unsupported.
class OsmDecodeError : public std::runtime_error
{
public:
	explicit OsmDecodeError(const std::string &message) : std::runtime_error(message) {}
};

///Input exceeded one of the caller's limits. limit names the field of
///OsmXmlLimits that was exceeded, such as "maxObjects".
class OsmLimitError : public OsmDecodeError
{
public:
	std::string limit;
	size_t maximum;
	size_t actual;

	OsmLimitError(const std::string &limit, size_t maximum, size_t actual);
};

///Base of the format decoders. A decoder reads from its input and sends what it
///finds to the handler given at construction, which must outlive it.
class OsmDecoder
{
protected:
	IDataStreamHandler &output;
	bool finished;

	///Sends Finish to the handler, once.
	void MarkFinished();

public:
	explicit OsmDecoder(IDataStreamHandler &output);
	virtual ~OsmDecoder();
	OsmDecoder(const OsmDecoder &) = delete;
	OsmDecoder &operator=(const OsmDecoder &) = delete;

	///Decodes the next part of the input, sending zero or more objects to the
	///handler. Returns true if there may be more input. At the end of the input
	///it sends Finish to the handler and returns false, as do later calls.
	///Throws OsmDecodeError for bad input; exceptions from the handler pass through.
	virtual bool DecodeNext() = 0;

	///Decodes the rest of the input.
	void Decode();

	bool IsFinished() const { return finished; }
};

#endif //CPPO5M_DECODER_H
