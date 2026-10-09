#include "io.h"
#include <sstream>
#include "o5m.h"
#include "osmchangexml.h"
#include "osmjson.h"
#include "pbf.h"
using namespace std;

static bool EndsWith(const std::string &text, const std::string &ending)
{
	return text.size() >= ending.size() &&
		text.compare(text.size() - ending.size(), ending.size(), ending) == 0;
}

OsmFormat FormatFromFilename(const std::string &filename)
{
	std::string name = filename;
	if(EndsWith(name, ".gz"))
		name.resize(name.size() - 3);

	if(EndsWith(name, ".o5m") || EndsWith(name, ".o5c"))
		return OsmFormat::O5m;
	if(EndsWith(name, ".osm") || EndsWith(name, ".xml"))
		return OsmFormat::OsmXml;
	if(EndsWith(name, ".pbf"))
		return OsmFormat::Pbf;
	if(EndsWith(name, ".json"))
		return OsmFormat::OsmJson;
	throw invalid_argument("File extension not recognised: " + filename);
}

std::unique_ptr<OsmDecoder> MakeDecoder(OsmFormat format, std::streambuf &input,
	IDataStreamHandler &output, const OsmXmlLimits &limits)
{
	switch(format)
	{
	case OsmFormat::O5m:
		return std::unique_ptr<OsmDecoder>(new O5mDecode(input, output));
	case OsmFormat::OsmXml:
		return std::unique_ptr<OsmDecoder>(new OsmXmlDecode(input, output, limits));
	case OsmFormat::Pbf:
		return std::unique_ptr<OsmDecoder>(new PbfDecode(input, output));
	case OsmFormat::OsmJson:
		return std::unique_ptr<OsmDecoder>(new OsmJsonDecode(input, output, limits));
	}
	throw invalid_argument("Unknown format");
}

std::unique_ptr<OsmEncoder> MakeEncoder(OsmFormat format, std::shared_ptr<ByteSink> sink,
	const TagMap &xmlAttribs)
{
	switch(format)
	{
	case OsmFormat::O5m:
		return std::unique_ptr<OsmEncoder>(new O5mEncode(sink));
	case OsmFormat::OsmXml:
		return std::unique_ptr<OsmEncoder>(new OsmXmlEncode(sink, xmlAttribs));
	case OsmFormat::Pbf:
		return std::unique_ptr<OsmEncoder>(new PbfEncode(sink));
	case OsmFormat::OsmJson:
		return std::unique_ptr<OsmEncoder>(new OsmJsonEncode(sink, xmlAttribs));
	}
	throw invalid_argument("Unknown format");
}

std::unique_ptr<OsmEncoder> MakeEncoder(OsmFormat format, std::streambuf &output,
	const TagMap &xmlAttribs)
{
	return MakeEncoder(format, std::make_shared<StreamSink>(output), xmlAttribs);
}

// ******* Whole documents **********

void LoadFromO5m(std::streambuf &input, IDataStreamHandler &output)
{
	O5mDecode dec(input, output);
	dec.Decode();
}

void LoadFromOsmXml(std::streambuf &input, IDataStreamHandler &output, const OsmXmlLimits &limits)
{
	OsmXmlDecode dec(input, output, limits);
	dec.Decode();
}

void LoadFromPbf(std::streambuf &input, IDataStreamHandler &output)
{
	PbfDecode dec(input, output);
	dec.Decode();
}

void LoadFromOsmJson(std::streambuf &input, IDataStreamHandler &output, const OsmXmlLimits &limits)
{
	OsmJsonDecode dec(input, output, limits);
	dec.Decode();
}

void LoadFromOsmChangeXml(std::streambuf &input, IOsmChangeHandler &output, const OsmXmlLimits &limits)
{
	OsmChangeXmlDecode dec(input, output, limits);
	dec.Decode();
}

void LoadFromO5m(const std::string &data, IDataStreamHandler &output)
{
	std::istringstream buff(data);
	LoadFromO5m(*buff.rdbuf(), output);
}

void LoadFromOsmXml(const std::string &data, IDataStreamHandler &output, const OsmXmlLimits &limits)
{
	//No stream needed: hand the whole document to the parser
	OsmXmlParser parser(output, limits);
	parser.Feed(data, true);
}

void LoadFromPbf(const std::string &data, IDataStreamHandler &output)
{
	std::istringstream buff(data);
	LoadFromPbf(*buff.rdbuf(), output);
}

void LoadFromOsmJson(const std::string &data, IDataStreamHandler &output, const OsmXmlLimits &limits)
{
	std::istringstream buff(data);
	LoadFromOsmJson(*buff.rdbuf(), output, limits);
}

void LoadFromOsmChangeXml(const std::string &data, IOsmChangeHandler &output, const OsmXmlLimits &limits)
{
	OsmChangeXmlParser parser(output, limits);
	parser.Feed(data, true);
}

// **********************************************************

void SaveToO5m(const OsmData &osmData, std::streambuf &output)
{
	O5mEncode enc(output);
	osmData.StreamTo(enc);
}

void SaveToOsmXml(const OsmData &osmData, std::streambuf &output, const TagMap &customAttribs)
{
	OsmXmlEncode enc(output, customAttribs);
	osmData.StreamTo(enc);
}

void SaveToPbf(const OsmData &osmData, std::streambuf &output)
{
	PbfEncode enc(output);
	osmData.StreamTo(enc);
}

void SaveToOsmJson(const OsmData &osmData, std::streambuf &output, const TagMap &customAttribs)
{
	OsmJsonEncode enc(output, customAttribs);
	osmData.StreamTo(enc);
}

void SaveToOsmChangeXml(const OsmChange &osmChange, std::streambuf &output,
	bool separateActions, const TagMap &customAttribs)
{
	OsmChangeXmlEncode enc(output, customAttribs, separateActions);
	enc.Encode(osmChange);
}
