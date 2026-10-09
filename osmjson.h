#ifndef CPPO5M_OSMJSON_H
#define CPPO5M_OSMJSON_H

#include <istream>
#include <memory>
#include <string>
#include "decoder.h"
#include "encoder.h"
#include "model.h"
#include "osmxml.h"

///Encodes a stream of map objects as OSM JSON, the format the OSM API returns
///for requests ending in .json:
///
///    {"version":"0.6","generator":"...","bounds":{...},"elements":[
///     {"type":"node","id":1,"lat":50.1,"lon":-1.2,...,"tags":{...}},
///     {"type":"way","id":2,...,"nodes":[1,...],"tags":{...}},
///     {"type":"relation","id":3,...,"members":[{"type":"way","ref":2,"role":""}]}
///    ]}
///
///Metadata that is zero or empty is left out, as are empty tags, an empty
///list of way nodes and an empty list of relation members. A deleted object
///has "visible":false, and a deleted node has no position. This matches what
///the OSM API writes.
class OsmJsonEncode : public OsmEncoder
{
private:
	TagMap customAttribs;
	bool writtenHeader, writtenBounds, inElements, anyElement;

	void WriteStart();
	void StartElement(std::string &out);
	void AppendCommon(const char *type, const OsmObject &obj, std::string &out);

public:
	///customAttribs become members of the top level object, as strings.
	///"version" and "generator" replace the defaults; entries with an empty
	///value are skipped.
	explicit OsmJsonEncode(std::shared_ptr<ByteSink> sink, const TagMap &customAttribs = TagMap());
	///Writes to a stream buffer, which must outlive the encoder.
	explicit OsmJsonEncode(std::streambuf &output, const TagMap &customAttribs = TagMap());

	void StoreIsDiff(bool isDiff) override;
	///Only bounds sent before the first object can be written, and only the first of those.
	void StoreBounds(const Bounds &bounds) override;
	void StoreNode(const OsmNode &node) override;
	void StoreWay(const OsmWay &way) override;
	void StoreRelation(const OsmRelation &relation) override;
	void Finish() override;
};

///Decodes OSM JSON, as written by OsmJsonEncode, the OSM API and Overpass.
///
///Members this library has no place for are skipped, such as the "center" and
///"geometry" Overpass can add. An element whose type is not node, way or
///relation is an error, as is the "error" element the OSM API appends when a
///response is incomplete.
///
///The limits are the ones used for XML and guard the same things; the two
///about XML attributes do not apply. Nesting depth counts every object and
///array, so an ordinary document needs maxDepth of at least 5.
///
///The parser cannot pause part way, so the first call to DecodeNext reads the
///whole document, sending objects to the handler as they complete, and the
///second sends Finish.
class OsmJsonDecode : public OsmDecoder
{
private:
	std::istream handle;
	OsmXmlLimits limits;
	bool parsed;

public:
	OsmJsonDecode(std::streambuf &input, IDataStreamHandler &output,
		const OsmXmlLimits &limits = OsmXmlLimits());

	bool DecodeNext() override;
};

///Appends text as a JSON string, including the quotes. Bytes that are not
///valid UTF-8 become U+FFFD so the output is always valid JSON.
void AppendJsonString(const std::string &text, std::string &out);

#endif //CPPO5M_OSMJSON_H
