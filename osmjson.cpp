#include "osmjson.h"
#include <cmath>
#include <cstdio>
#include <ctime>
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
		time_t tt = metaData.timestamp;
		char buf[50];
		struct tm tmbuf;
		if(gmtime_r(&tt, &tmbuf) != nullptr && strftime(buf, sizeof(buf), "%FT%TZ", &tmbuf) > 0)
		{
			out.append(",\"timestamp\":\"");
			out.append(buf);
			out.push_back('"');
		}
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
	out.append(",\"nodes\":[");
	for(size_t i=0; i<way.refs.size(); i++)
	{
		if(i > 0)
			out.push_back(',');
		out.append(std::to_string(way.refs[i]));
	}
	out.push_back(']');
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
