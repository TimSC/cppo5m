#include "decoder.h"
#include "encoder.h"
#include <sstream>

static std::string LimitMessage(const std::string &limit, size_t maximum, size_t actual)
{
	std::stringstream ss;
	ss << limit << " limit exceeded; maximum is " << maximum << ", got " << actual;
	return ss.str();
}

OsmLimitError::OsmLimitError(const std::string &limit, size_t maximum, size_t actual) :
	OsmDecodeError(LimitMessage(limit, maximum, actual)),
	limit(limit), maximum(maximum), actual(actual)
{

}

OsmDecoder::OsmDecoder(IDataStreamHandler &output) : output(output), finished(false)
{

}

OsmDecoder::~OsmDecoder()
{

}

void OsmDecoder::MarkFinished()
{
	if(this->finished)
		return;
	this->finished = true;
	this->output.Finish();
}

void OsmDecoder::Decode()
{
	while(this->DecodeNext()) {}
}

// **************************************************

OsmEncoder::OsmEncoder(std::shared_ptr<ByteSink> sinkIn) : sink(sinkIn)
{
	if(!this->sink)
		throw std::invalid_argument("Encoder sink is null");
}

void OsmEncoder::SetSink(std::shared_ptr<ByteSink> sinkIn)
{
	if(!sinkIn)
		throw std::invalid_argument("Encoder sink is null");
	this->sink = sinkIn;
}
