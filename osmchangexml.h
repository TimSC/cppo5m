#ifndef CPPO5M_OSMCHANGEXML_H
#define CPPO5M_OSMCHANGEXML_H

#include <istream>
#include <memory>
#include <string>
#include <vector>
#include "osmxml.h"

///Push parser for osmChange XML. Each completed action element is sent to the
///handler as one block. Use OsmChangeXmlDecode to read from a stream.
class OsmChangeXmlParser : public XmlPushParser
{
private:
	IOsmChangeHandler &output;
	OsmXmlObjectReader reader;
	OsmChangeBlock block;
	bool inAction, inObject;

protected:
	void OnStartElement(const char *name, const char **atts) override;
	void OnEndElement(const char *name) override;
	void OnComplete() override;

public:
	explicit OsmChangeXmlParser(IOsmChangeHandler &output, const OsmXmlLimits &limits = OsmXmlLimits());
};

///Decodes osmChange XML from a stream.
class OsmChangeXmlDecode
{
private:
	std::istream handle;
	OsmChangeXmlParser parser;
	std::vector<char> buffer;
	bool finished;

public:
	OsmChangeXmlDecode(std::streambuf &input, IOsmChangeHandler &output,
		const OsmXmlLimits &limits = OsmXmlLimits());

	///Same contract as OsmDecoder::DecodeNext.
	bool DecodeNext();
	void Decode();
	bool IsFinished() const { return finished; }
};

///Encodes osmChange XML, either block by block as a handler or from a whole
///OsmChange with Encode.
class OsmChangeXmlEncode : public IOsmChangeHandler
{
private:
	std::shared_ptr<ByteSink> sink;
	TagMap customAttribs;
	bool separateActions;
	bool writtenHeader;

	void WriteStart();

public:
	///With separateActions each object gets its own action element. Deletes
	///are written relations first, then ways, then nodes.
	explicit OsmChangeXmlEncode(std::shared_ptr<ByteSink> sink, const TagMap &customAttribs = TagMap(),
		bool separateActions = false);
	///Writes to a stream buffer, which must outlive the encoder.
	explicit OsmChangeXmlEncode(std::streambuf &output, const TagMap &customAttribs = TagMap(),
		bool separateActions = false);

	void StoreChangeBlock(const OsmChangeBlock &block) override;
	void Finish() override;

	///Writes a complete document.
	void Encode(const OsmChange &osmChange);
};

#endif //CPPO5M_OSMCHANGEXML_H
