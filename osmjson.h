#ifndef CPPO5M_OSMJSON_H
#define CPPO5M_OSMJSON_H

#include <memory>
#include <string>
#include "encoder.h"
#include "model.h"

///Encodes a stream of map objects as OSM JSON, the format the OSM API returns
///for requests ending in .json:
///
///    {"version":"0.6","generator":"...","bounds":{...},"elements":[
///     {"type":"node","id":1,"lat":50.1,"lon":-1.2,...,"tags":{...}},
///     {"type":"way","id":2,...,"nodes":[1,...],"tags":{...}},
///     {"type":"relation","id":3,...,"members":[{"type":"way","ref":2,"role":""}]}
///    ]}
///
///Metadata that is zero or empty is left out, as are empty tags. A deleted
///object has "visible":false, and a deleted node has no position. There is no
///decoder for this format.
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

///Appends text as a JSON string, including the quotes. Bytes that are not
///valid UTF-8 become U+FFFD so the output is always valid JSON.
void AppendJsonString(const std::string &text, std::string &out);

#endif //CPPO5M_OSMJSON_H
