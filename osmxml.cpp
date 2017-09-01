#include "osmxml.h"
#include <sstream>
#include <ctime>
using namespace std;

// ********* Utility classes ***********

// https://stackoverflow.com/a/9907752/4288232
std::string escapexml(const std::string& src) {
	std::stringstream dst;
	for (size_t i=0; i<src.size(); i++)
	{
		char ch = src[i];
		switch (ch) {
			case '&': dst << "&amp;"; break;
			case '\'': dst << "&apos;"; break;
			case '"': dst << "&quot;"; break;
			case '<': dst << "&lt;"; break;
			case '>': dst << "&gt;"; break;
			default: dst << ch; break;
		}
	}
	return dst.str();
}

// ************* Encoder *************

OsmXmlEncodeBase::OsmXmlEncodeBase() : IDataStreamHandler()
{

}

OsmXmlEncodeBase::~OsmXmlEncodeBase()
{

}

void OsmXmlEncodeBase::WriteStart()
{
	*this << "<?xml version='1.0' encoding='UTF-8'?>\n";
	*this << "<osm version='0.6' upload='true' generator='cppo5m'>\n";
}

void OsmXmlEncodeBase::EncodeMetaData(const class MetaData &metaData, std::stringstream &ss)
{
	if(metaData.timestamp != 0)
	{
		time_t tt = metaData.timestamp;
		char buf[50];
		strftime(buf, sizeof(buf), "%FT%TZ", gmtime(&tt));
		ss << " timestamp='"<<buf<<"'";
	}
	if(metaData.uid != 0)
		ss << " uid='" << metaData.uid << "'";
	if(metaData.username.length() > 0)
		ss << " user='" << escapexml(metaData.username) << "'";
	if(metaData.visible)
		ss << " visible='true'";
	else
		ss << " visible='false'";
	if(metaData.version != 0)
		ss << " version='" << metaData.version << "'";
	if(metaData.changeset != 0)
		ss << " changeset='" << metaData.changeset << "'";
}

void OsmXmlEncodeBase::Sync()
{

}

void OsmXmlEncodeBase::Reset()
{

}

void OsmXmlEncodeBase::Finish()
{
	*this << "</osm>";
}

void OsmXmlEncodeBase::StoreIsDiff(bool)
{

}

void OsmXmlEncodeBase::StoreBounds(double x1, double y1, double x2, double y2)
{
	stringstream ss;
	ss.precision(9);
	ss << "  <bounds minlat='"<<y1<<"' minlon='"<<x1<<"' ";
	ss << "maxlat='"<<y2<<"' maxlon='"<<x2<<"' />" << endl;
	*this << ss.str();
}

void OsmXmlEncodeBase::StoreNode(int64_t objId, const class MetaData &metaData, 
	const TagMap &tags, double lat, double lon)
{
	stringstream ss;
	ss.precision(9);
	ss << "  <node id='"<<objId<<"'";
	this->EncodeMetaData(metaData, ss);
	ss << " lat='"<<lat<<"' lon='"<<lon<<"'";
	if(tags.size() == 0)
		ss <<" />" << endl;
	else
	{
		ss <<">" << endl;

		//Write tags
		for(TagMap::const_iterator it=tags.begin(); it!=tags.end(); it++)
		{
			ss << "    <tag k='"<<escapexml(it->first)<<"' v='"<<escapexml(it->second)<<"' />" << endl;
		}
		ss << "  </node>" << endl;
	}
	*this << ss.str();
}

void OsmXmlEncodeBase::StoreWay(int64_t objId, const class MetaData &metaData, 
	const TagMap &tags, const std::vector<int64_t> &refs)
{
	stringstream ss;
	ss << "  <way id='"<<objId<<"'";
	this->EncodeMetaData(metaData, ss);
	if(tags.size() == 0 && refs.size() == 0)
		ss <<" />" << endl;
	else
	{
		ss <<">" << endl;

		//Write node IDs
		for(size_t i=0; i<refs.size(); i++)
			ss << "    <nd ref='"<<refs[i]<<"' />" << endl;

		//Write tags
		for(TagMap::const_iterator it=tags.begin(); it!=tags.end(); it++)
			ss << "    <tag k='"<<escapexml(it->first)<<"' v='"<<escapexml(it->second)<<"' />" << endl;

		ss << "  </way>" << endl;
	}
	*this << ss.str();
}

void OsmXmlEncodeBase::StoreRelation(int64_t objId, const class MetaData &metaData, const TagMap &tags, 
	const std::vector<std::string> &refTypeStrs, const std::vector<int64_t> &refIds,
	const std::vector<std::string> &refRoles)
{
	stringstream ss;
	ss << "  <relation id='"<<objId<<"'";
	this->EncodeMetaData(metaData, ss);
	if(tags.size() == 0 && refTypeStrs.size() == 0)
		ss <<" />" << endl;
	else
	{
		ss <<">" << endl;

		//Write node IDs
		for(size_t i=0; i<refTypeStrs.size(); i++)
			ss << "    <member type='"<<refTypeStrs[i]<<"' ref='"<<refIds[i]<<"' role='"<<escapexml(refRoles[i])<<"' />" << endl;

		//Write tags
		for(TagMap::const_iterator it=tags.begin(); it!=tags.end(); it++)
			ss << "    <tag k='"<<escapexml(it->first)<<"' v='"<<escapexml(it->second)<<"' />" << endl;

		ss << "  </relation>" << endl;
	}
	*this << ss.str();
}

// ****************************

OsmXmlEncode::OsmXmlEncode(std::streambuf &handleIn): OsmXmlEncodeBase(), handle(&handleIn)
{
	this->WriteStart();
}

OsmXmlEncode::~OsmXmlEncode()
{

}

