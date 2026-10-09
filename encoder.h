#ifndef CPPO5M_ENCODER_H
#define CPPO5M_ENCODER_H

#include <memory>
#include <string>
#include "handler.h"
#include "sink.h"

///Base of the format encoders: a stream handler that writes to a ByteSink.
///Nothing is written until the first call that produces output, and the
///document is only complete once Finish has been called.
class OsmEncoder : public IDataStreamHandler
{
protected:
	std::shared_ptr<ByteSink> sink;

	void Write(const char *data, size_t len) { this->sink->Write(data, len); }
	void Write(const std::string &data) { this->sink->Write(data.data(), data.size()); }

public:
	explicit OsmEncoder(std::shared_ptr<ByteSink> sink);
	virtual ~OsmEncoder() {}

	///Later output goes to a different sink. The encoder's state is unchanged,
	///so this can split one document across several buffers.
	void SetSink(std::shared_ptr<ByteSink> sink);
	std::shared_ptr<ByteSink> GetSink() const { return this->sink; }
};

#endif //CPPO5M_ENCODER_H
