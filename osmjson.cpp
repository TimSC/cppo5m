#include "osmjson.h"
#include <cmath>
#include <cstdio>
#include <vector>
#include <rapidjson/reader.h>
#include <rapidjson/error/en.h>
#include "osmtime.h"
using namespace std;

///Length of the valid UTF-8 sequence at text[i], or zero if there is not one.
static size_t Utf8Length(const std::string &text, size_t i)
{
	unsigned char c0 = text[i];
	size_t len = 0;
	uint32_t code = 0;
	if(c0 < 0x80) return 1;
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
	return len;
}

void AppendJsonString(const std::string &text, std::string &out)
{
	out.push_back('"');
	size_t i = 0;
	while(i < text.size())
	{
		unsigned char ch = text[i];
		switch(ch)
		{
		case '"': out.append("\\\""); i++; continue;
		case '\\': out.append("\\\\"); i++; continue;
		case '\n': out.append("\\n"); i++; continue;
		case '\r': out.append("\\r"); i++; continue;
		case '\t': out.append("\\t"); i++; continue;
		}
		if(ch < 0x20)
		{
			char buf[8];
			snprintf(buf, sizeof(buf), "\\u%04x", ch);
			out.append(buf);
			i++;
			continue;
		}

		size_t len = Utf8Length(text, i);
		if(len == 0)
		{
			out.append("\xef\xbf\xbd");
			i++;
			continue;
		}
		out.append(text, i, len);
		i += len;
	}
	out.push_back('"');
}

///Appends a coordinate with up to nine decimal places and no trailing zeros.
///JSON has no way to write infinity or NaN, so those are refused.
static void AppendJsonCoord(double value, std::string &out)
{
	if(!std::isfinite(value))
		throw std::invalid_argument("Coordinate is not a finite number");
	char buf[400]; //Room for the largest finite double
	int len = snprintf(buf, sizeof(buf), "%.9f", value);
	while(len > 0 && buf[len-1] == '0')
		len --;
	if(len > 0 && buf[len-1] == '.')
		len --;
	//Negative zero is written as zero
	if(len == 2 && buf[0] == '-' && buf[1] == '0')
		out.push_back('0');
	else
		out.append(buf, len);
}

// ****************************

OsmJsonEncode::OsmJsonEncode(std::shared_ptr<ByteSink> sinkIn, const TagMap &customAttribsIn) :
	OsmEncoder(sinkIn),
	customAttribs(customAttribsIn),
	writtenHeader(false),
	writtenBounds(false),
	inElements(false),
	anyElement(false)
{

}

OsmJsonEncode::OsmJsonEncode(std::streambuf &output, const TagMap &customAttribsIn) :
	OsmJsonEncode(std::make_shared<StreamSink>(output), customAttribsIn)
{

}

void OsmJsonEncode::WriteStart()
{
	std::string out = "{\"version\":";
	TagMap::const_iterator it = customAttribs.find("version");
	AppendJsonString(it != customAttribs.end() ? it->second : std::string("0.6"), out);
	out.append(",\"generator\":");
	it = customAttribs.find("generator");
	AppendJsonString(it != customAttribs.end() ? it->second : std::string("cppo5m"), out);

	for(it = customAttribs.begin(); it != customAttribs.end(); it++)
	{
		if(it->first == "version" || it->first == "generator")
			continue;
		if(it->second.length() == 0)
			continue;
		out.push_back(',');
		AppendJsonString(it->first, out);
		out.push_back(':');
		AppendJsonString(it->second, out);
	}
	this->writtenHeader = true;
	this->Write(out);
}

///Starts an entry of the elements array, opening the array if needed.
void OsmJsonEncode::StartElement(std::string &out)
{
	if(!this->writtenHeader)
		this->WriteStart();
	if(!this->inElements)
	{
		out.append(",\"elements\":[");
		this->inElements = true;
	}
	out.append(this->anyElement ? ",\n" : "\n");
	this->anyElement = true;
}

void OsmJsonEncode::AppendCommon(const char *type, const OsmObject &obj, std::string &out)
{
	out.append("{\"type\":\"");
	out.append(type);
	out.append("\",\"id\":");
	out.append(std::to_string(obj.objId));
}

///Appends the metadata members shared by every object.
static void AppendMetaData(const MetaData &metaData, std::string &out)
{
	if(metaData.timestamp != 0)
	{
		std::string text;
		if(AppendOsmTimestamp(metaData.timestamp, text))
			out.append(",\"timestamp\":\"" + text + "\"");
	}
	if(metaData.version != 0)
		out.append(",\"version\":" + std::to_string(metaData.version));
	if(metaData.changeset != 0)
		out.append(",\"changeset\":" + std::to_string(metaData.changeset));
	if(metaData.username.length() > 0)
	{
		out.append(",\"user\":");
		AppendJsonString(metaData.username, out);
	}
	if(metaData.uid != 0)
		out.append(",\"uid\":" + std::to_string(metaData.uid));
	if(!metaData.visible)
		out.append(",\"visible\":false");
}

static void AppendTags(const TagMap &tags, std::string &out)
{
	if(tags.empty())
		return;
	out.append(",\"tags\":{");
	for(TagMap::const_iterator it=tags.begin(); it!=tags.end(); it++)
	{
		if(it != tags.begin())
			out.push_back(',');
		AppendJsonString(it->first, out);
		out.push_back(':');
		AppendJsonString(it->second, out);
	}
	out.push_back('}');
}

void OsmJsonEncode::StoreIsDiff(bool)
{
	if(!this->writtenHeader)
		this->WriteStart();
}

void OsmJsonEncode::StoreBounds(const Bounds &bounds)
{
	if(this->inElements || this->writtenBounds)
		return; //The one bounds member comes before the elements
	if(!this->writtenHeader)
		this->WriteStart();
	this->writtenBounds = true;

	std::string out = ",\"bounds\":{\"minlat\":";
	AppendJsonCoord(bounds.minLat, out);
	out.append(",\"minlon\":");
	AppendJsonCoord(bounds.minLon, out);
	out.append(",\"maxlat\":");
	AppendJsonCoord(bounds.maxLat, out);
	out.append(",\"maxlon\":");
	AppendJsonCoord(bounds.maxLon, out);
	out.push_back('}');
	this->Write(out);
}

void OsmJsonEncode::StoreNode(const OsmNode &node)
{
	std::string out;
	this->StartElement(out);
	this->AppendCommon("node", node, out);
	if(node.metaData.visible)
	{
		out.append(",\"lat\":");
		AppendJsonCoord(node.lat, out);
		out.append(",\"lon\":");
		AppendJsonCoord(node.lon, out);
	}
	AppendMetaData(node.metaData, out);
	AppendTags(node.tags, out);
	out.push_back('}');
	this->Write(out);
}

void OsmJsonEncode::StoreWay(const OsmWay &way)
{
	std::string out;
	this->StartElement(out);
	this->AppendCommon("way", way, out);
	AppendMetaData(way.metaData, out);
	//Like tags, an empty list is left out, as the OSM API does for a deleted way
	if(!way.refs.empty())
	{
		out.append(",\"nodes\":[");
		for(size_t i=0; i<way.refs.size(); i++)
		{
			if(i > 0)
				out.push_back(',');
			out.append(std::to_string(way.refs[i]));
		}
		out.push_back(']');
	}
	AppendTags(way.tags, out);
	out.push_back('}');
	this->Write(out);
}

void OsmJsonEncode::StoreRelation(const OsmRelation &relation)
{
	std::string out;
	this->StartElement(out);
	this->AppendCommon("relation", relation, out);
	AppendMetaData(relation.metaData, out);
	if(!relation.members.empty())
	{
		out.append(",\"members\":[");
		for(size_t i=0; i<relation.members.size(); i++)
		{
			const RelationMember &member = relation.members[i];
			if(i > 0)
				out.push_back(',');
			out.append("{\"type\":\"");
			out.append(ObjectTypeName(member.type));
			out.append("\",\"ref\":" + std::to_string(member.ref) + ",\"role\":");
			AppendJsonString(member.role, out);
			out.push_back('}');
		}
		out.push_back(']');
	}
	AppendTags(relation.tags, out);
	out.push_back('}');
	this->Write(out);
}

void OsmJsonEncode::Finish()
{
	if(!this->writtenHeader)
		this->WriteStart();
	//An empty result still has an elements member
	this->Write(this->inElements ? "\n]}" : ",\"elements\":[]}");
	this->inElements = true;
}

// ************* Decoder *************

static void CheckJsonLimit(const char *name, size_t value, size_t limit)
{
	if(limit > 0 && value > limit)
		throw OsmLimitError(name, limit, value);
}

///Feeds a std::istream to rapidjson, counting bytes against the size limit.
class JsonInputStream
{
private:
	std::istream &stream;
	std::vector<char> buffer;
	size_t pos, len, consumed, maxBytes;
	bool ended;

	void Fill()
	{
		if(this->ended)
			return;
		this->consumed += this->len;
		this->stream.read(this->buffer.data(), this->buffer.size());
		if(this->stream.bad())
			throw OsmDecodeError("Error reading JSON input");
		this->len = this->stream.gcount();
		this->pos = 0;
		if(this->len == 0)
			this->ended = true;
		CheckJsonLimit("maxBytes", this->consumed + this->len, this->maxBytes);
	}

public:
	typedef char Ch;

	JsonInputStream(std::istream &streamIn, size_t maxBytesIn) :
		stream(streamIn), buffer(64 * 1024), pos(0), len(0), consumed(0), maxBytes(maxBytesIn), ended(false) {}

	Ch Peek()
	{
		if(this->pos >= this->len)
			this->Fill();
		return this->pos < this->len ? this->buffer[this->pos] : '\0';
	}
	Ch Take()
	{
		Ch c = this->Peek();
		if(this->pos < this->len)
			this->pos ++;
		return c;
	}
	size_t Tell() const { return this->consumed + this->pos; }

	//Output half of the stream concept, which a reader never uses
	Ch *PutBegin() { return nullptr; }
	void Put(Ch) {}
	void Flush() {}
	size_t PutEnd(Ch *) { return 0; }
};

///Receives rapidjson's events and turns the document into handler calls.
class JsonDocumentHandler
{
private:
	enum class Context { Top, Bounds, Elements, Element, Tags, Nodes, Members, Member };
	enum class Kind { Null, Bool, Integer, Double, String };

	///One scalar value, in whichever form the parser reported it.
	struct Scalar
	{
		Kind kind;
		bool boolean;
		bool negative; ///<For integers: the value is below zero
		uint64_t magnitude; ///<For integers: the absolute value
		double number;
		const char *text;
		size_t length;
	};

	IDataStreamHandler &output;
	const OsmXmlLimits &limits;

	std::vector<Context> stack;
	size_t skipDepth; ///<Nesting inside a value that is being ignored
	TagMap attributes; ///<Document attributes collected so far
	bool elementsSeen;

	///Sends the document attributes, once, before anything else is sent.
	void FlushAttributes()
	{
		if(!this->attributes.empty())
			this->output.StoreAttributes(this->attributes);
		this->attributes.clear();
	}

	std::string key;
	bool rootSeen;

	//The element being read. Its type may arrive after its other members, so
	//everything is collected and the right kind of object built at the end.
	bool haveType;
	ObjectType type;
	int64_t objId;
	double lat, lon;
	MetaData metaData;
	TagMap tags;
	std::vector<int64_t> nodes;
	std::vector<RelationMember> members;
	RelationMember member;
	bool memberHasType;
	Bounds bounds;

	size_t objectCount;
	bool anyObject;
	ObjectType lastType;
	OsmNode node;
	OsmWay way;
	OsmRelation relation;

	static int64_t ToInt(const Scalar &v, const char *what)
	{
		if(v.kind != Kind::Integer)
			throw OsmDecodeError(std::string("JSON ") + what + " must be a whole number");
		if(v.negative)
		{
			if(v.magnitude > (uint64_t)INT64_MAX + 1)
				throw OsmDecodeError(std::string("JSON ") + what + " is out of range");
			return (int64_t)(0 - v.magnitude);
		}
		if(v.magnitude > (uint64_t)INT64_MAX)
			throw OsmDecodeError(std::string("JSON ") + what + " is out of range");
		return (int64_t)v.magnitude;
	}

	static uint64_t ToUnsigned(const Scalar &v, const char *what)
	{
		if(v.kind != Kind::Integer)
			throw OsmDecodeError(std::string("JSON ") + what + " must be a whole number");
		if(v.negative && v.magnitude != 0)
			throw OsmDecodeError(std::string("JSON ") + what + " must not be negative");
		return v.magnitude;
	}

	static double ToDouble(const Scalar &v, const char *what)
	{
		if(v.kind == Kind::Double)
			return v.number;
		if(v.kind == Kind::Integer)
			return v.negative ? -(double)v.magnitude : (double)v.magnitude;
		throw OsmDecodeError(std::string("JSON ") + what + " must be a number");
	}

	static std::string ToString(const Scalar &v, const char *what)
	{
		if(v.kind != Kind::String)
			throw OsmDecodeError(std::string("JSON ") + what + " must be a string");
		return std::string(v.text, v.length);
	}

	void CheckDepth()
	{
		CheckJsonLimit("maxDepth", this->stack.size() + this->skipDepth, limits.maxDepth);
	}

	void StartElement()
	{
		this->objectCount ++;
		CheckJsonLimit("maxObjects", this->objectCount, limits.maxObjects);
		this->haveType = false;
		this->objId = 0;
		this->lat = 0.0;
		this->lon = 0.0;
		this->metaData = MetaData();
		this->tags.clear();
		this->nodes.clear();
		this->members.clear();
	}

	void EndElement()
	{
		if(!this->haveType)
			throw OsmDecodeError("JSON element has no type");

		//Some encoders need to know when the object type changes
		if(this->anyObject && this->type != this->lastType)
			this->output.Reset();
		this->anyObject = true;
		this->lastType = this->type;

		switch(this->type)
		{
		case ObjectType::Node:
			this->node.objId = this->objId;
			this->node.metaData = this->metaData;
			this->node.tags.swap(this->tags);
			this->node.lat = this->lat;
			this->node.lon = this->lon;
			this->output.StoreNode(this->node);
			break;
		case ObjectType::Way:
			this->way.objId = this->objId;
			this->way.metaData = this->metaData;
			this->way.tags.swap(this->tags);
			this->way.refs.swap(this->nodes);
			this->output.StoreWay(this->way);
			break;
		case ObjectType::Relation:
			this->relation.objId = this->objId;
			this->relation.metaData = this->metaData;
			this->relation.tags.swap(this->tags);
			this->relation.members.swap(this->members);
			this->output.StoreRelation(this->relation);
			break;
		}
	}

	void ElementMember(const Scalar &v)
	{
		if(this->key == "type")
		{
			std::string name = ToString(v, "element type");
			if(!ObjectTypeFromName(name, this->type))
				throw OsmDecodeError("JSON element has an unknown type: " + name);
			this->haveType = true;
		}
		else if(this->key == "id") this->objId = ToInt(v, "element id");
		else if(this->key == "lat") this->lat = ToDouble(v, "latitude");
		else if(this->key == "lon") this->lon = ToDouble(v, "longitude");
		else if(this->key == "timestamp")
			this->metaData.timestamp = ParseOsmTimestamp(ToString(v, "timestamp").c_str());
		else if(this->key == "version") this->metaData.version = ToUnsigned(v, "version");
		else if(this->key == "changeset") this->metaData.changeset = ToInt(v, "changeset");
		else if(this->key == "user") this->metaData.username = ToString(v, "user");
		else if(this->key == "uid") this->metaData.uid = ToUnsigned(v, "uid");
		else if(this->key == "visible")
		{
			if(v.kind != Kind::Bool)
				throw OsmDecodeError("JSON visible must be true or false");
			this->metaData.visible = v.boolean;
		}
		else if(this->key == "error")
		{
			//The OSM API adds this when it fails part way through a response
			throw OsmDecodeError("JSON document reports an error: " +
				(v.kind == Kind::String ? std::string(v.text, v.length) : std::string("unknown")));
		}
		//Anything else is not something this library stores
	}

	bool OnScalar(const Scalar &v)
	{
		if(this->skipDepth > 0)
			return true;
		if(this->stack.empty())
			throw OsmDecodeError("JSON document must be an object");

		switch(this->stack.back())
		{
		case Context::Top:
			if(this->key == "elements" || this->key == "bounds")
				throw OsmDecodeError("JSON " + this->key + " has the wrong type");
			//Strings beside the version and generator are document attributes.
			//They can only be passed on if they come before the elements.
			if(v.kind == Kind::String && this->key != "version" && this->key != "generator" && !this->elementsSeen)
				this->attributes[this->key] = std::string(v.text, v.length);
			break;
		case Context::Bounds:
			if(this->key == "minlat") this->bounds.minLat = ToDouble(v, "bounds");
			else if(this->key == "minlon") this->bounds.minLon = ToDouble(v, "bounds");
			else if(this->key == "maxlat") this->bounds.maxLat = ToDouble(v, "bounds");
			else if(this->key == "maxlon") this->bounds.maxLon = ToDouble(v, "bounds");
			break;
		case Context::Elements:
			throw OsmDecodeError("JSON elements must be objects");
		case Context::Element:
			if(this->key == "tags" || this->key == "nodes" || this->key == "members")
				throw OsmDecodeError("JSON " + this->key + " has the wrong type");
			this->ElementMember(v);
			break;
		case Context::Tags:
			CheckJsonLimit("maxTagsPerObject", this->tags.size() + 1, limits.maxTagsPerObject);
			this->tags[this->key] = ToString(v, "tag value");
			break;
		case Context::Nodes:
			CheckJsonLimit("maxWayNodesPerObject", this->nodes.size() + 1, limits.maxWayNodesPerObject);
			this->nodes.push_back(ToInt(v, "way node"));
			break;
		case Context::Members:
			throw OsmDecodeError("JSON relation members must be objects");
		case Context::Member:
			if(this->key == "type")
			{
				std::string name = ToString(v, "member type");
				if(!ObjectTypeFromName(name, this->member.type))
					throw OsmDecodeError("JSON relation member has an unknown type: " + name);
				this->memberHasType = true;
			}
			else if(this->key == "ref") this->member.ref = ToInt(v, "member ref");
			else if(this->key == "role") this->member.role = ToString(v, "member role");
			break;
		}
		return true;
	}

	bool Integer(bool negative, uint64_t magnitude)
	{
		Scalar v = Scalar();
		v.kind = Kind::Integer;
		v.negative = negative;
		v.magnitude = magnitude;
		return this->OnScalar(v);
	}

public:
	JsonDocumentHandler(IDataStreamHandler &outputIn, const OsmXmlLimits &limitsIn) :
		output(outputIn), limits(limitsIn), skipDepth(0), elementsSeen(false), rootSeen(false),
		haveType(false), type(ObjectType::Node), objId(0), lat(0.0), lon(0.0),
		memberHasType(false), objectCount(0), anyObject(false), lastType(ObjectType::Node) {}

	bool Complete() const { return this->rootSeen && this->stack.empty(); }

	bool Null() { Scalar v = Scalar(); v.kind = Kind::Null; return this->OnScalar(v); }
	bool Bool(bool b) { Scalar v = Scalar(); v.kind = Kind::Bool; v.boolean = b; return this->OnScalar(v); }
	bool Int(int i) { return this->Int64(i); }
	bool Uint(unsigned u) { return this->Integer(false, u); }
	bool Int64(int64_t i) { return i < 0 ? this->Integer(true, 0 - (uint64_t)i) : this->Integer(false, (uint64_t)i); }
	bool Uint64(uint64_t u) { return this->Integer(false, u); }
	bool Double(double d)
	{
		Scalar v = Scalar();
		v.kind = Kind::Double;
		v.number = d;
		return this->OnScalar(v);
	}
	bool RawNumber(const char *, rapidjson::SizeType, bool) { return false; } //Not requested
	bool String(const char *str, rapidjson::SizeType length, bool)
	{
		Scalar v = Scalar();
		v.kind = Kind::String;
		v.text = str;
		v.length = length;
		return this->OnScalar(v);
	}

	bool Key(const char *str, rapidjson::SizeType length, bool)
	{
		if(this->skipDepth == 0)
			this->key.assign(str, length);
		return true;
	}

	bool StartObject()
	{
		if(this->skipDepth > 0)
		{
			this->skipDepth ++;
			this->CheckDepth();
			return true;
		}
		if(this->stack.empty())
		{
			if(this->rootSeen)
				throw OsmDecodeError("JSON document has more than one root");
			this->rootSeen = true;
			this->stack.push_back(Context::Top);
			this->CheckDepth();
			return true;
		}

		switch(this->stack.back())
		{
		case Context::Top:
			if(this->key == "bounds")
			{
				this->FlushAttributes();
				this->bounds = Bounds();
				this->stack.push_back(Context::Bounds);
			}
			else if(this->key == "elements")
				throw OsmDecodeError("JSON elements must be an array");
			else
				this->skipDepth = 1;
			break;
		case Context::Elements:
			this->StartElement();
			this->stack.push_back(Context::Element);
			break;
		case Context::Element:
			if(this->key == "tags")
				this->stack.push_back(Context::Tags);
			else if(this->key == "nodes" || this->key == "members")
				throw OsmDecodeError("JSON " + this->key + " must be an array");
			else
				this->skipDepth = 1; //For example the center Overpass can add
			break;
		case Context::Members:
			CheckJsonLimit("maxRelationMembersPerObject", this->members.size() + 1,
				limits.maxRelationMembersPerObject);
			this->member = RelationMember();
			this->memberHasType = false;
			this->stack.push_back(Context::Member);
			break;
		case Context::Tags:
			throw OsmDecodeError("JSON tag value must be a string");
		case Context::Nodes:
			throw OsmDecodeError("JSON way node must be a whole number");
		case Context::Bounds:
		case Context::Member:
			this->skipDepth = 1;
			break;
		}
		this->CheckDepth();
		return true;
	}

	bool EndObject(rapidjson::SizeType)
	{
		if(this->skipDepth > 0)
		{
			this->skipDepth --;
			return true;
		}

		Context finished = this->stack.back();
		this->stack.pop_back();
		switch(finished)
		{
		case Context::Top:
			if(!this->elementsSeen)
				this->FlushAttributes();
			break;
		case Context::Bounds:
			this->output.StoreBounds(this->bounds);
			break;
		case Context::Element:
			this->EndElement();
			break;
		case Context::Member:
			if(!this->memberHasType)
				throw OsmDecodeError("JSON relation member has no type");
			this->members.push_back(this->member);
			break;
		default:
			break;
		}
		return true;
	}

	bool StartArray()
	{
		if(this->skipDepth > 0)
		{
			this->skipDepth ++;
			this->CheckDepth();
			return true;
		}
		if(this->stack.empty())
			throw OsmDecodeError("JSON document must be an object");

		switch(this->stack.back())
		{
		case Context::Top:
			if(this->key == "elements")
			{
				this->FlushAttributes();
				this->elementsSeen = true;
				this->stack.push_back(Context::Elements);
			}
			else if(this->key == "bounds")
				throw OsmDecodeError("JSON bounds must be an object");
			else
				this->skipDepth = 1;
			break;
		case Context::Element:
			if(this->key == "nodes")
				this->stack.push_back(Context::Nodes);
			else if(this->key == "members")
				this->stack.push_back(Context::Members);
			else if(this->key == "tags")
				throw OsmDecodeError("JSON tags must be an object");
			else
				this->skipDepth = 1; //For example the geometry Overpass can add
			break;
		case Context::Elements:
			throw OsmDecodeError("JSON elements must be objects");
		case Context::Members:
			throw OsmDecodeError("JSON relation members must be objects");
		case Context::Tags:
			throw OsmDecodeError("JSON tag value must be a string");
		case Context::Nodes:
			throw OsmDecodeError("JSON way node must be a whole number");
		case Context::Bounds:
		case Context::Member:
			this->skipDepth = 1;
			break;
		}
		this->CheckDepth();
		return true;
	}

	bool EndArray(rapidjson::SizeType)
	{
		if(this->skipDepth > 0)
			this->skipDepth --;
		else
			this->stack.pop_back();
		return true;
	}
};

OsmJsonDecode::OsmJsonDecode(std::streambuf &input, IDataStreamHandler &outputIn,
	const OsmXmlLimits &limitsIn) :
	OsmDecoder(outputIn),
	handle(&input),
	limits(limitsIn),
	parsed(false)
{

}

bool OsmJsonDecode::DecodeNext()
{
	if(this->finished)
		return false;
	if(this->parsed)
	{
		this->MarkFinished();
		return false;
	}
	this->parsed = true;

	this->output.StoreIsDiff(false);

	JsonInputStream stream(this->handle, this->limits.maxBytes);
	JsonDocumentHandler handler(this->output, this->limits);
	rapidjson::Reader reader;
	//Iterative parsing keeps deeply nested input off the call stack, and full
	//precision makes coordinates read back exactly as written
	rapidjson::ParseResult result = reader.Parse<rapidjson::kParseIterativeFlag |
		rapidjson::kParseFullPrecisionFlag>(stream, handler);
	if(result.IsError())
	{
		std::string message = "JSON error: ";
		message += rapidjson::GetParseError_En(result.Code());
		message += " at offset " + std::to_string(result.Offset());
		throw OsmDecodeError(message);
	}
	if(!handler.Complete())
		throw OsmDecodeError("JSON document is incomplete");
	return true;
}
