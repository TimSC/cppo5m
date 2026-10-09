#include "osmxml.h"
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <expat.h>
#include "osmtime.h"
using namespace std;

void OsmXmlLimits::Apply(const std::map<std::string, int64_t> &limitsIn)
{
	for(std::map<std::string, int64_t>::const_iterator it=limitsIn.begin(); it!=limitsIn.end(); it++)
	{
		if(it->second < 0)
			throw invalid_argument("XML parser limits must be zero or greater");

		size_t value = (size_t)it->second;
		if(it->first == "max_bytes")
			maxBytes = value;
		else if(it->first == "max_depth")
			maxDepth = value;
		else if(it->first == "max_objects")
			maxObjects = value;
		else if(it->first == "max_tags_per_object")
			maxTagsPerObject = value;
		else if(it->first == "max_members_per_object")
		{
			maxWayNodesPerObject = value;
			maxRelationMembersPerObject = value;
		}
		else if(it->first == "max_way_nodes_per_object")
			maxWayNodesPerObject = value;
		else if(it->first == "max_relation_members_per_object")
			maxRelationMembersPerObject = value;
		else if(it->first == "max_attributes_per_element")
			maxAttributesPerElement = value;
		else if(it->first == "max_attribute_bytes")
			maxAttributeBytes = value;
		else
			throw invalid_argument("Unknown XML parser limit: " + it->first);
	}
}

static void CheckLimit(const char *name, size_t value, size_t limit)
{
	if(limit > 0 && value > limit)
		throw OsmLimitError(name, limit, value);
}

static const char *FindAttrib(const char **atts, const char *name)
{
	for(size_t i=0; atts[i] != nullptr; i += 2)
		if(strcmp(atts[i], name) == 0)
			return atts[i+1];
	return nullptr;
}

// ************* Expat plumbing *************

XmlPushParser::XmlPushParser(const OsmXmlLimits &limitsIn) :
	parser(XML_ParserCreate(nullptr)),
	bytesFed(0),
	begun(false),
	complete(false),
	failed(false),
	limits(limitsIn),
	depth(0)
{
	if(this->parser == nullptr)
		throw std::bad_alloc();
	XML_SetUserData(this->parser, this);
	XML_SetElementHandler(this->parser, XmlPushParser::StartCallback, XmlPushParser::EndCallback);
}

XmlPushParser::~XmlPushParser()
{
	XML_ParserFree(this->parser);
}

// Expat is C code, so exceptions must not unwind through it. The callbacks
// keep the first exception, stop the parser, and Feed rethrows it.

void XmlPushParser::StartCallback(void *userData, const char *name, const char **atts)
{
	XmlPushParser *self = (XmlPushParser *)userData;
	if(self->pending)
		return;
	try
	{
		self->depth ++;
		CheckLimit("maxDepth", self->depth, self->limits.maxDepth);
		self->CheckElementLimits(name, atts);
		self->OnStartElement(name, atts);
	}
	catch(...)
	{
		self->pending = std::current_exception();
		XML_StopParser(self->parser, XML_FALSE);
	}
}

void XmlPushParser::EndCallback(void *userData, const char *name)
{
	XmlPushParser *self = (XmlPushParser *)userData;
	if(self->pending)
		return;
	try
	{
		self->OnEndElement(name);
		self->depth --;
	}
	catch(...)
	{
		self->pending = std::current_exception();
		XML_StopParser(self->parser, XML_FALSE);
	}
}

void XmlPushParser::CheckElementLimits(const char *name, const char **atts)
{
	if(limits.maxAttributesPerElement == 0 && limits.maxAttributeBytes == 0)
		return;
	size_t attributeCount = 0;
	size_t attributeBytes = strlen(name);
	for(size_t i=0; atts[i] != nullptr; i += 2)
	{
		attributeCount++;
		attributeBytes += strlen(atts[i]) + strlen(atts[i+1]);
	}
	CheckLimit("maxAttributesPerElement", attributeCount, limits.maxAttributesPerElement);
	CheckLimit("maxAttributeBytes", attributeBytes, limits.maxAttributeBytes);
}

void XmlPushParser::Feed(const char *data, size_t len, bool final)
{
	if(this->failed)
		throw OsmDecodeError("XML parser cannot continue after an earlier error");
	if(this->complete)
		throw OsmDecodeError("XML document is already complete");

	try
	{
		if(!this->begun)
		{
			this->begun = true;
			this->OnBegin();
		}

		if(len > SIZE_MAX - this->bytesFed)
			throw OsmLimitError("maxBytes", limits.maxBytes, SIZE_MAX);
		this->bytesFed += len;
		CheckLimit("maxBytes", this->bytesFed, limits.maxBytes);

		//Expat takes an int length, so pass very large buffers in pieces
		const size_t maxPiece = 1 << 30;
		size_t offset = 0;
		do
		{
			size_t piece = len - offset;
			if(piece > maxPiece)
				piece = maxPiece;
			bool last = final && offset + piece == len;
			XML_Status status = XML_Parse(this->parser, data + offset, (int)piece, last);
			if(this->pending)
				std::rethrow_exception(this->pending);
			if(status == XML_STATUS_ERROR)
			{
				std::string message = "XML error: ";
				message += XML_ErrorString(XML_GetErrorCode(this->parser));
				message += " at line " + std::to_string(XML_GetCurrentLineNumber(this->parser));
				throw OsmDecodeError(message);
			}
			offset += piece;
		}
		while(offset < len);

		if(final)
		{
			this->complete = true;
			this->OnComplete();
		}
	}
	catch(...)
	{
		this->failed = true;
		this->pending = nullptr;
		throw;
	}
}

// ************* Object elements *************

static int64_t ParseInt(const char *text)
{
	return strtoll(text, nullptr, 10);
}

///Parses a latitude or longitude. Text that is not a number reads as zero, as
///before, but infinities and NaN are refused: no format can store them.
static double ParseCoord(const char *text)
{
	double value = strtod(text, nullptr);
	if(!std::isfinite(value))
		throw OsmDecodeError(std::string("Invalid coordinate: ") + text);
	return value;
}

OsmXmlObjectReader::OsmXmlObjectReader(const OsmXmlLimits &limitsIn) :
	limits(limitsIn),
	active(false),
	type(ObjectType::Node),
	objectCount(0),
	tagCount(0)
{

}

bool OsmXmlObjectReader::IsObject(const char *name)
{
	return strcmp(name, "node") == 0 || strcmp(name, "way") == 0 || strcmp(name, "relation") == 0;
}

OsmObject &OsmXmlObjectReader::Current()
{
	switch(this->type)
	{
	case ObjectType::Way: return this->way;
	case ObjectType::Relation: return this->relation;
	default: return this->node;
	}
}

void OsmXmlObjectReader::StartObject(const char *name, const char **atts)
{
	this->objectCount ++;
	CheckLimit("maxObjects", this->objectCount, limits.maxObjects);

	this->type = ObjectTypeFromName(name);
	this->active = true;
	this->tagCount = 0;
	this->node.lat = 0.0;
	this->node.lon = 0.0;
	this->way.refs.clear();
	this->relation.members.clear();

	OsmObject &obj = this->Current();
	obj.objId = 0;
	obj.metaData = MetaData();
	obj.tags.clear();

	for(size_t i=0; atts[i] != nullptr; i += 2)
	{
		const char *key = atts[i];
		const char *value = atts[i+1];
		if(strcmp(key, "id") == 0)
			obj.objId = ParseInt(value);
		else if(strcmp(key, "version") == 0)
			obj.metaData.version = ParseInt(value);
		else if(strcmp(key, "timestamp") == 0)
			obj.metaData.timestamp = ParseOsmTimestamp(value);
		else if(strcmp(key, "changeset") == 0)
			obj.metaData.changeset = ParseInt(value);
		else if(strcmp(key, "uid") == 0)
			obj.metaData.uid = ParseInt(value);
		else if(strcmp(key, "user") == 0)
			obj.metaData.username = value;
		else if(strcmp(key, "visible") == 0)
			obj.metaData.visible = strcmp(value, "false") != 0;
		else if(strcmp(key, "current") == 0)
			obj.metaData.current = strcmp(value, "false") != 0;
		else if(this->type == ObjectType::Node && strcmp(key, "lat") == 0)
			this->node.lat = ParseCoord(value);
		else if(this->type == ObjectType::Node && strcmp(key, "lon") == 0)
			this->node.lon = ParseCoord(value);
	}
}

void OsmXmlObjectReader::StartChild(const char *name, const char **atts)
{
	if(!this->active)
		return;

	if(strcmp(name, "tag") == 0)
	{
		this->tagCount ++;
		CheckLimit("maxTagsPerObject", this->tagCount, limits.maxTagsPerObject);
		const char *k = FindAttrib(atts, "k");
		const char *v = FindAttrib(atts, "v");
		this->Current().tags[k ? k : ""] = v ? v : "";
	}
	else if(strcmp(name, "nd") == 0 && this->type == ObjectType::Way)
	{
		CheckLimit("maxWayNodesPerObject", this->way.refs.size() + 1, limits.maxWayNodesPerObject);
		const char *ref = FindAttrib(atts, "ref");
		this->way.refs.push_back(ref ? ParseInt(ref) : 0);
	}
	else if(strcmp(name, "member") == 0 && this->type == ObjectType::Relation)
	{
		CheckLimit("maxRelationMembersPerObject", this->relation.members.size() + 1,
			limits.maxRelationMembersPerObject);
		const char *memberType = FindAttrib(atts, "type");
		const char *ref = FindAttrib(atts, "ref");
		const char *role = FindAttrib(atts, "role");

		RelationMember member;
		if(memberType == nullptr || !ObjectTypeFromName(memberType, member.type))
			throw OsmDecodeError(std::string("Relation member has an unknown type: ") +
				(memberType ? memberType : ""));
		member.ref = ref ? ParseInt(ref) : 0;
		if(role != nullptr)
			member.role = role;
		this->relation.members.push_back(member);
	}
}

void OsmXmlObjectReader::EndObject(IDataStreamHandler &output)
{
	if(!this->active)
		return;
	this->active = false;
	this->Current().StreamTo(output);
}

// ************* OSM XML parser *************

OsmXmlParser::OsmXmlParser(IDataStreamHandler &outputIn, const OsmXmlLimits &limitsIn) :
	XmlPushParser(limitsIn),
	output(outputIn),
	reader(this->limits),
	inObject(false),
	anyObject(false),
	lastType(ObjectType::Node)
{

}

void OsmXmlParser::OnBegin()
{
	this->output.StoreIsDiff(false);
}

void OsmXmlParser::OnStartElement(const char *name, const char **atts)
{
	if(this->depth == 1)
	{
		if(strcmp(name, "osm") != 0)
			throw OsmDecodeError(std::string("Expected an osm root element but found ") + name);
	}
	else if(this->depth == 2)
	{
		if(OsmXmlObjectReader::IsObject(name))
		{
			this->reader.StartObject(name, atts);
			this->inObject = true;
		}
		else if(strcmp(name, "bounds") == 0)
		{
			Bounds bounds;
			const char *value = FindAttrib(atts, "minlon");
			if(value) bounds.minLon = ParseCoord(value);
			value = FindAttrib(atts, "minlat");
			if(value) bounds.minLat = ParseCoord(value);
			value = FindAttrib(atts, "maxlon");
			if(value) bounds.maxLon = ParseCoord(value);
			value = FindAttrib(atts, "maxlat");
			if(value) bounds.maxLat = ParseCoord(value);
			this->output.StoreBounds(bounds);
		}
		//Other elements, such as note and meta, are skipped along with their children
	}
	else if(this->depth == 3 && this->inObject)
	{
		this->reader.StartChild(name, atts);
	}
}

void OsmXmlParser::OnEndElement(const char *name)
{
	if(this->depth == 2 && this->inObject)
	{
		this->inObject = false;
		//Some encoders need to know when the object type changes
		if(this->anyObject && this->reader.Type() != this->lastType)
			this->output.Reset();
		this->anyObject = true;
		this->lastType = this->reader.Type();
		this->reader.EndObject(this->output);
	}
}

void OsmXmlParser::OnComplete()
{
	this->output.Finish();
}

// ***********************************

OsmXmlDecode::OsmXmlDecode(std::streambuf &input, IDataStreamHandler &outputIn,
	const OsmXmlLimits &limits):
	OsmDecoder(outputIn),
	handle(&input),
	parser(outputIn, limits),
	buffer(64 * 1024)
{

}

bool OsmXmlDecode::DecodeNext()
{
	if(this->finished)
		return false;

	this->handle.read(this->buffer.data(), this->buffer.size());
	size_t count = this->handle.gcount();
	if(this->handle.bad())
		throw OsmDecodeError("Error reading XML input");
	bool done = count == 0;
	this->parser.Feed(this->buffer.data(), count, done);
	if(done)
		this->finished = true; //The parser has already sent Finish
	return !done;
}

// ************* Encoder *************

///Length of the valid UTF-8 sequence at text[i] encoding a character XML 1.0
///allows, or zero if there is not one.
static size_t XmlCharLength(const std::string &text, size_t i)
{
	unsigned char c0 = text[i];
	size_t len = 0;
	uint32_t code = 0;
	if(c0 < 0x80) { len = 1; code = c0; }
	else if(c0 >= 0xc2 && c0 <= 0xdf) { len = 2; code = c0 & 0x1f; }
	else if(c0 >= 0xe0 && c0 <= 0xef) { len = 3; code = c0 & 0x0f; }
	else if(c0 >= 0xf0 && c0 <= 0xf4) { len = 4; code = c0 & 0x07; }
	else return 0;
	if(i + len > text.size())
		return 0;
	for(size_t j=1; j<len; j++)
	{
		unsigned char c = text[i+j];
		if((c & 0xc0) != 0x80)
			return 0;
		code = (code << 6) | (c & 0x3f);
	}

	//Overlong forms, surrogates and values beyond Unicode are not UTF-8
	if((len == 3 && code < 0x800) || (len == 4 && code < 0x10000) || code > 0x10ffff)
		return 0;
	if(code >= 0xd800 && code <= 0xdfff)
		return 0;
	//XML 1.0 excludes most control characters and two noncharacters
	if(code < 0x20 && code != 0x09 && code != 0x0a && code != 0x0d)
		return 0;
	if(code == 0xfffe || code == 0xffff)
		return 0;
	return len;
}

void AppendXmlEscaped(const std::string &text, std::string &out)
{
	size_t i = 0;
	while(i < text.size())
	{
		char ch = text[i];
		switch(ch)
		{
		case '&': out.append("&amp;"); i++; continue;
		case '\'': out.append("&apos;"); i++; continue;
		case '"': out.append("&quot;"); i++; continue;
		case '<': out.append("&lt;"); i++; continue;
		case '>': out.append("&gt;"); i++; continue;
		//Written as references so attribute value normalisation keeps them
		case '\n': out.append("&#10;"); i++; continue;
		case '\r': out.append("&#13;"); i++; continue;
		case '\t': out.append("&#9;"); i++; continue;
		}

		//Anything XML cannot hold becomes the replacement character, so the
		//document is always well formed whatever the strings contain
		size_t len = XmlCharLength(text, i);
		if(len == 0)
		{
			out.append("\xef\xbf\xbd");
			i++;
			continue;
		}
		out.append(text, i, len);
		i += len;
	}
}

///True if name can be used as an XML attribute name.
static bool IsXmlName(const std::string &name)
{
	if(name.empty())
		return false;
	for(size_t i=0; i<name.size(); i++)
	{
		unsigned char c = name[i];
		bool start = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == ':';
		bool rest = start || (c >= '0' && c <= '9') || c == '-' || c == '.';
		if(i == 0 ? !start : !rest)
			return false;
	}
	return true;
}

static void AppendAttrib(const char *name, const std::string &value, std::string &out)
{
	out.push_back(' ');
	out.append(name);
	out.append("=\"");
	AppendXmlEscaped(value, out);
	out.push_back('"');
}

static void AppendCoord(double value, std::string &out)
{
	char buf[400]; //Room for the largest finite double
	snprintf(buf, sizeof(buf), "%.9f", value);
	out.append(buf);
}

void AppendXmlRootAttribs(const TagMap &customAttribs, std::string &out)
{
	TagMap::const_iterator it = customAttribs.find("version");
	AppendAttrib("version", it != customAttribs.end() ? it->second : std::string("0.6"), out);
	it = customAttribs.find("generator");
	AppendAttrib("generator", it != customAttribs.end() ? it->second : std::string("cppo5m"), out);

	for(it = customAttribs.begin(); it != customAttribs.end(); it++)
	{
		if(it->first == "version" || it->first == "generator")
			continue;
		if(it->second.length() == 0)
			continue;
		if(!IsXmlName(it->first))
			throw std::invalid_argument("Not a valid XML attribute name: " + it->first);
		AppendAttrib(it->first.c_str(), it->second, out);
	}
}

static void AppendObjectStart(const char *name, const OsmObject &obj, std::string &out)
{
	out.append("  <");
	out.append(name);
	out.append(" id=\"");
	out.append(std::to_string(obj.objId));
	out.push_back('"');

	const MetaData &metaData = obj.metaData;
	if(metaData.timestamp != 0)
	{
		std::string text;
		if(AppendOsmTimestamp(metaData.timestamp, text))
			out.append(" timestamp=\"" + text + "\"");
	}
	if(metaData.uid != 0)
		out.append(" uid=\"" + std::to_string(metaData.uid) + "\"");
	if(metaData.username.length() > 0)
		AppendAttrib("user", metaData.username, out);
	out.append(metaData.visible ? " visible=\"true\"" : " visible=\"false\"");
	if(metaData.version != 0)
		out.append(" version=\"" + std::to_string(metaData.version) + "\"");
	if(metaData.changeset != 0)
		out.append(" changeset=\"" + std::to_string(metaData.changeset) + "\"");
}

static void AppendTags(const TagMap &tags, std::string &out)
{
	for(TagMap::const_iterator it=tags.begin(); it!=tags.end(); it++)
	{
		out.append("    <tag k=\"");
		AppendXmlEscaped(it->first, out);
		out.append("\" v=\"");
		AppendXmlEscaped(it->second, out);
		out.append("\" />\n");
	}
}

static void AppendObjectEnd(const char *name, bool empty, std::string &out)
{
	if(empty)
	{
		out.append(" />\n");
		return;
	}
	out.append("  </");
	out.append(name);
	out.append(">\n");
}

void AppendXmlNode(const OsmNode &node, std::string &out)
{
	AppendObjectStart("node", node, out);
	out.append(" lat=\"");
	AppendCoord(node.lat, out);
	out.append("\" lon=\"");
	AppendCoord(node.lon, out);
	out.push_back('"');

	bool empty = node.tags.empty();
	if(!empty)
	{
		out.append(">\n");
		AppendTags(node.tags, out);
	}
	AppendObjectEnd("node", empty, out);
}

void AppendXmlWay(const OsmWay &way, std::string &out)
{
	AppendObjectStart("way", way, out);

	bool empty = way.tags.empty() && way.refs.empty();
	if(!empty)
	{
		out.append(">\n");
		for(size_t i=0; i<way.refs.size(); i++)
			out.append("    <nd ref=\"" + std::to_string(way.refs[i]) + "\" />\n");
		AppendTags(way.tags, out);
	}
	AppendObjectEnd("way", empty, out);
}

void AppendXmlRelation(const OsmRelation &relation, std::string &out)
{
	AppendObjectStart("relation", relation, out);

	bool empty = relation.tags.empty() && relation.members.empty();
	if(!empty)
	{
		out.append(">\n");
		for(size_t i=0; i<relation.members.size(); i++)
		{
			const RelationMember &member = relation.members[i];
			out.append("    <member type=\"");
			out.append(ObjectTypeName(member.type));
			out.append("\" ref=\"" + std::to_string(member.ref) + "\" role=\"");
			AppendXmlEscaped(member.role, out);
			out.append("\" />\n");
		}
		AppendTags(relation.tags, out);
	}
	AppendObjectEnd("relation", empty, out);
}

// ****************************

OsmXmlEncode::OsmXmlEncode(std::shared_ptr<ByteSink> sinkIn, const TagMap &customAttribsIn) :
	OsmEncoder(sinkIn),
	customAttribs(customAttribsIn),
	writtenHeader(false)
{

}

OsmXmlEncode::OsmXmlEncode(std::streambuf &output, const TagMap &customAttribsIn) :
	OsmXmlEncode(std::make_shared<StreamSink>(output), customAttribsIn)
{

}

void OsmXmlEncode::WriteStart()
{
	std::string out = "<?xml version='1.0' encoding='UTF-8'?>\n<osm";
	AppendXmlRootAttribs(this->customAttribs, out);
	out.append(">\n");
	this->writtenHeader = true;
	this->Write(out);
}

void OsmXmlEncode::StoreIsDiff(bool)
{
	if(!this->writtenHeader)
		this->WriteStart();
}

void OsmXmlEncode::StoreBounds(const Bounds &bounds)
{
	if(!this->writtenHeader)
		this->WriteStart();
	std::string out = "  <bounds minlat=\"";
	AppendCoord(bounds.minLat, out);
	out.append("\" minlon=\"");
	AppendCoord(bounds.minLon, out);
	out.append("\" maxlat=\"");
	AppendCoord(bounds.maxLat, out);
	out.append("\" maxlon=\"");
	AppendCoord(bounds.maxLon, out);
	out.append("\" />\n");
	this->Write(out);
}

void OsmXmlEncode::StoreNode(const OsmNode &node)
{
	if(!this->writtenHeader)
		this->WriteStart();
	std::string out;
	AppendXmlNode(node, out);
	this->Write(out);
}

void OsmXmlEncode::StoreWay(const OsmWay &way)
{
	if(!this->writtenHeader)
		this->WriteStart();
	std::string out;
	AppendXmlWay(way, out);
	this->Write(out);
}

void OsmXmlEncode::StoreRelation(const OsmRelation &relation)
{
	if(!this->writtenHeader)
		this->WriteStart();
	std::string out;
	AppendXmlRelation(relation, out);
	this->Write(out);
}

void OsmXmlEncode::Finish()
{
	if(!this->writtenHeader)
		this->WriteStart();
	this->Write("</osm>");
}
