#ifndef CPPO5M_IO_H
#define CPPO5M_IO_H

#include <memory>
#include <streambuf>
#include <string>
#include "decoder.h"
#include "encoder.h"
#include "model.h"
#include "osmxml.h"

// Convenience functions for whole documents, and factories that pick a
// decoder or encoder by format.

enum class OsmFormat
{
	O5m,
	OsmXml,
	Pbf,
	OsmJson
};

///Works out the format from a file name ending in .o5m, .o5c, .osm, .xml,
///.pbf or .json, optionally followed by .gz. The data is not decompressed here: wrap
///the stream yourself. Throws std::invalid_argument for anything else.
OsmFormat FormatFromFilename(const std::string &filename);

///The decoder reads from input and writes to output; both must outlive it.
///limits applies to XML and JSON only.
std::unique_ptr<OsmDecoder> MakeDecoder(OsmFormat format, std::streambuf &input,
	IDataStreamHandler &output, const OsmXmlLimits &limits = OsmXmlLimits());
///xmlAttribs are document attributes, written by every format except PBF; see
///OsmXmlEncode, OsmJsonEncode and O5mEncode.
std::unique_ptr<OsmEncoder> MakeEncoder(OsmFormat format, std::shared_ptr<ByteSink> sink,
	const TagMap &xmlAttribs = TagMap());
std::unique_ptr<OsmEncoder> MakeEncoder(OsmFormat format, std::streambuf &output,
	const TagMap &xmlAttribs = TagMap());

// Decode a whole document. These throw OsmDecodeError for bad input.
void LoadFromO5m(std::streambuf &input, IDataStreamHandler &output);
void LoadFromOsmXml(std::streambuf &input, IDataStreamHandler &output,
	const OsmXmlLimits &limits = OsmXmlLimits());
void LoadFromPbf(std::streambuf &input, IDataStreamHandler &output);
void LoadFromOsmJson(std::streambuf &input, IDataStreamHandler &output,
	const OsmXmlLimits &limits = OsmXmlLimits());
void LoadFromOsmChangeXml(std::streambuf &input, IOsmChangeHandler &output,
	const OsmXmlLimits &limits = OsmXmlLimits());

// The same, reading from a string holding the document.
void LoadFromO5m(const std::string &data, IDataStreamHandler &output);
void LoadFromOsmXml(const std::string &data, IDataStreamHandler &output,
	const OsmXmlLimits &limits = OsmXmlLimits());
void LoadFromPbf(const std::string &data, IDataStreamHandler &output);
void LoadFromOsmJson(const std::string &data, IDataStreamHandler &output,
	const OsmXmlLimits &limits = OsmXmlLimits());
void LoadFromOsmChangeXml(const std::string &data, IOsmChangeHandler &output,
	const OsmXmlLimits &limits = OsmXmlLimits());

// Encode a whole document.
void SaveToO5m(const OsmData &osmData, std::streambuf &output);
void SaveToOsmXml(const OsmData &osmData, std::streambuf &output, const TagMap &customAttribs = TagMap());
void SaveToPbf(const OsmData &osmData, std::streambuf &output);
void SaveToOsmJson(const OsmData &osmData, std::streambuf &output, const TagMap &customAttribs = TagMap());
void SaveToOsmChangeXml(const OsmChange &osmChange, std::streambuf &output,
	bool separateActions = false, const TagMap &customAttribs = TagMap());

#endif //CPPO5M_IO_H
