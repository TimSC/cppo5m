#ifndef CPPO5M_OSMXML_H
#define CPPO5M_OSMXML_H

#include <cstdint>
#include <exception>
#include <istream>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "decoder.h"
#include "encoder.h"
#include "model.h"

struct XML_ParserStruct;

///Limits applied while parsing XML from an untrusted source. Zero means no
///limit. Exceeding one throws OsmLimitError naming the field.
struct OsmXmlLimits
{
	size_t maxBytes = 0; ///<Total document size
	size_t maxDepth = 0; ///<Element nesting
	size_t maxObjects = 0; ///<Nodes, ways and relations in the document
	size_t maxTagsPerObject = 0;
	size_t maxWayNodesPerObject = 0;
	size_t maxRelationMembersPerObject = 0;
	size_t maxAttributesPerElement = 0;
	size_t maxAttributeBytes = 0; ///<Element name plus attribute names and values

	///Sets limits by name: max_bytes, max_depth, max_objects,
	///max_tags_per_object, max_way_nodes_per_object,
	///max_relation_members_per_object, max_members_per_object (both of the
	///previous two), max_attributes_per_element and max_attribute_bytes.
	///Throws std::invalid_argument for an unknown name or a negative value.
	void Apply(const std::map<std::string, int64_t> &limits);
};

///Shared expat plumbing for the push parsers below. Not for direct use.
class XmlPushParser
{
private:
	XML_ParserStruct *parser;
	std::exception_ptr pending;
	size_t bytesFed;
	bool begun;
	bool complete;
	bool failed;

	static void StartCallback(void *userData, const char *name, const char **atts);
	static void EndCallback(void *userData, const char *name);
	void CheckElementLimits(const char *name, const char **atts);

protected:
	OsmXmlLimits limits;
	int depth; ///<Nesting of the element being handled; the root element is 1

	virtual void OnStartElement(const char *name, const char **atts) = 0;
	virtual void OnEndElement(const char *name) = 0;
	virtual void OnBegin() {}
	virtual void OnComplete() {}

public:
	explicit XmlPushParser(const OsmXmlLimits &limits);
	virtual ~XmlPushParser();
	XmlPushParser(const XmlPushParser &) = delete;
	XmlPushParser &operator=(const XmlPushParser &) = delete;

	///Parses the next piece of the document. Pieces may split the text
	///anywhere. Pass final=true with the last piece, which may be empty.
	///Throws OsmDecodeError for malformed XML and OsmLimitError for a limit;
	///exceptions from the handler pass through. After any exception the parser
	///cannot be used again.
	void Feed(const char *data, size_t len, bool final);
	void Feed(const std::string &data, bool final) { this->Feed(data.data(), data.size(), final); }

	///True once the final piece has been parsed successfully.
	bool IsComplete() const { return complete; }
};

///Reads one node, way or relation element. Shared by the OSM and osmChange
///parsers. Not for direct use.
class OsmXmlObjectReader
{
private:
	const OsmXmlLimits &limits;
	bool active;
	ObjectType type;
	OsmNode node;
	OsmWay way;
	OsmRelation relation;
	size_t objectCount, tagCount;

	OsmObject &Current();

public:
	explicit OsmXmlObjectReader(const OsmXmlLimits &limits);

	///True if the element name is one this reader handles.
	static bool IsObject(const char *name);
	void StartObject(const char *name, const char **atts);
	void StartChild(const char *name, const char **atts);
	///Sends the completed object to the handler.
	void EndObject(IDataStreamHandler &output);
	ObjectType Type() const { return type; }
};

///Push parser for OSM XML: feed it text as it arrives and it sends objects to
///the handler. Use OsmXmlDecode to read from a stream.
class OsmXmlParser : public XmlPushParser
{
private:
	IDataStreamHandler &output;
	OsmXmlObjectReader reader;
	bool inObject, anyObject;
	ObjectType lastType;

protected:
	void OnStartElement(const char *name, const char **atts) override;
	void OnEndElement(const char *name) override;
	void OnBegin() override;
	void OnComplete() override;

public:
	explicit OsmXmlParser(IDataStreamHandler &output, const OsmXmlLimits &limits = OsmXmlLimits());
};

///Decodes OSM XML from a stream.
class OsmXmlDecode : public OsmDecoder
{
private:
	std::istream handle;
	OsmXmlParser parser;
	std::vector<char> buffer;

public:
	OsmXmlDecode(std::streambuf &input, IDataStreamHandler &output,
		const OsmXmlLimits &limits = OsmXmlLimits());

	bool DecodeNext() override;
};

///Encodes a stream of map objects as OSM XML.
class OsmXmlEncode : public OsmEncoder
{
private:
	TagMap customAttribs;
	bool writtenHeader;

	void WriteStart();

public:
	///customAttribs become attributes of the root element. "version" and
	///"generator" replace the defaults; entries with an empty value are skipped.
	explicit OsmXmlEncode(std::shared_ptr<ByteSink> sink, const TagMap &customAttribs = TagMap());
	///Writes to a stream buffer, which must outlive the encoder.
	explicit OsmXmlEncode(std::streambuf &output, const TagMap &customAttribs = TagMap());

	void StoreIsDiff(bool isDiff) override;
	void StoreBounds(const Bounds &bounds) override;
	void StoreNode(const OsmNode &node) override;
	void StoreWay(const OsmWay &way) override;
	void StoreRelation(const OsmRelation &relation) override;
	void Finish() override;
};

// Building blocks shared with the osmChange encoder.

///Appends text escaped for use inside a double quoted attribute.
void AppendXmlEscaped(const std::string &text, std::string &out);
///Appends the root element's attributes, including a leading space.
void AppendXmlRootAttribs(const TagMap &customAttribs, std::string &out);
void AppendXmlNode(const OsmNode &node, std::string &out);
void AppendXmlWay(const OsmWay &way, std::string &out);
void AppendXmlRelation(const OsmRelation &relation, std::string &out);

#endif //CPPO5M_OSMXML_H
