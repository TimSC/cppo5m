#include "osmxml.h"
#include <sstream>
#include <ctime>
#include <assert.h>
#include <cstring>
#include "iso8601.h"
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

static void StartElement(void *userData, const XML_Char *name, const XML_Char **atts)
{
	((class OsmXmlDecode *)userData)->StartElement(name, atts);
}

static void EndElement(void *userData, const XML_Char *name)
{
	((class OsmXmlDecode *)userData)->EndElement(name);
}

// ************* Decoder *************

OsmXmlDecode::OsmXmlDecode(std::streambuf &handleIn):
	handle(&handleIn),
	output(NULL)
{
	xmlDepth = 0;
	parseCompletedOk = false;
	parser = XML_ParserCreate(NULL);
	XML_SetUserData(parser, this);
	XML_SetElementHandler(parser, ::StartElement, ::EndElement);
	this->firstParseCall = true;
}

OsmXmlDecode::~OsmXmlDecode()
{
	XML_ParserFree(parser);
}

bool OsmXmlDecode::DecodeNext()
{
	if(output == NULL)
		throw runtime_error("OsmXmlDecode output pointer is null");
	
	if(this->firstParseCall)
	{
		output->StoreIsDiff(false);
		this->firstParseCall = false;
	}

	handle.read((char *)decodeBuff, sizeof(decodeBuff));

	bool done = handle.gcount()==0;
	if (XML_Parse(parser, decodeBuff, handle.gcount(), done) == XML_STATUS_ERROR)
	{
		stringstream ss;
		ss << XML_ErrorString(XML_GetErrorCode(parser))
			<< " at line " << XML_GetCurrentLineNumber(parser) << endl;
		errString = ss.str();
		return false;
	}
	if(done)
	{
		output->Finish();
		parseCompletedOk = true;
	}
	return !done;
}

void OsmXmlDecode::DecodeHeader()
{

}

void OsmXmlDecode::StartElement(const XML_Char *name, const XML_Char **atts)
{
	this->xmlDepth ++;
	//cout << this->xmlDepth << " startel " << name << endl;

	std::map<std::string, std::string> attribs;
	size_t i=0;
	while(atts[i] != NULL)
	{
		attribs[atts[i]] = atts[i+1];
		i += 2;
	}

	if(this->xmlDepth == 2)
	{
		this->currentObjectType = name;
		this->metadataMap = attribs;
	}

	else if(this->xmlDepth == 3)
	{
		if(strcmp(name, "tag") == 0)
		{
			this->tags[attribs["k"]] = attribs["v"];
		}
		if(strcmp(name, "nd") == 0 && currentObjectType == "way")
		{
			this->memObjIds.push_back(atoi(attribs["ref"].c_str()));
		}
		if(strcmp(name, "member") == 0 && currentObjectType == "relation")
		{
			this->memObjIds.push_back(atoi(attribs["ref"].c_str()));
			this->memObjTypes.push_back(attribs["type"]);
			this->memObjRoles.push_back(attribs["role"]);
		}
	}
}

void OsmXmlDecode::EndElement(const XML_Char *name)
{
	//cout << this->xmlDepth << " endel " << name << endl;
	
	if(this->xmlDepth == 2)
	{
		cout << currentObjectType << endl;
		for(TagMap::iterator it=this->metadataMap.begin(); it!=this->metadataMap.end(); it++)
			cout << it->first << "," << it->second << endl;
		for(TagMap::iterator it=this->tags.begin(); it!=this->tags.end(); it++)
			cout << "tags " << it->first << "," << it->second << endl;
	
		if(this->currentObjectType == "bounds")
		{
			double minlat=0.0, minlon=0.0, maxlat=0.0, maxlon=0.0;

			TagMap::iterator it = this->metadataMap.find("minlat");
			if(it != this->metadataMap.end())
				minlat = atof(it->second.c_str());
			it = this->metadataMap.find("minlon");
			if(it != this->metadataMap.end())
				minlon = atof(it->second.c_str());
			it = this->metadataMap.find("maxlat");
			if(it != this->metadataMap.end())
				maxlat = atof(it->second.c_str());
			it = this->metadataMap.find("maxlon");
			if(it != this->metadataMap.end())
				maxlon = atof(it->second.c_str());

			output->StoreBounds(minlon, minlat, maxlon, maxlat);
		}
		else
		{
			if(this->lastObjectType != this->currentObjectType)
				output->Sync();

			int64_t objId = 0;
			TagMap::iterator it = this->metadataMap.find("id");
			if(it != this->metadataMap.end())
				objId = atoi(it->second.c_str());

			class MetaData metaData;
			DecodeMetaData(metaData);

			if(this->currentObjectType == "node")
			{
				double lat = 0.0, lon = 0.0;
				it = this->metadataMap.find("lat");
				if(it != this->metadataMap.end())
					lat = atof(it->second.c_str());
				it = this->metadataMap.find("lon");
				if(it != this->metadataMap.end())
					lon = atof(it->second.c_str());

				output->StoreNode(objId, metaData, this->tags, lat, lon);
			}
			else if(this->currentObjectType == "way")
			{
				output->StoreWay(objId, metaData, this->tags, this->memObjIds);
			}
			else if(this->currentObjectType == "relation")
			{
				output->StoreRelation(objId, metaData, this->tags, 
					this->memObjTypes, this->memObjIds, this->memObjRoles);
			}

			this->lastObjectType = this->currentObjectType;
		}

		//Clear data ready for further processing
		this->currentObjectType = "";
		this->metadataMap.clear();
		this->tags.clear();
		this->memObjIds.clear();
		this->memObjTypes.clear();
		this->memObjRoles.clear();
	}

	this->xmlDepth --;
}

void OsmXmlDecode::DecodeMetaData(class MetaData &metaData)
{
	TagMap::iterator it = this->metadataMap.find("version");
	if(it != this->metadataMap.end())
		metaData.version = atoi(it->second.c_str());
	it = this->metadataMap.find("timestamp");
	if(it != this->metadataMap.end())
	{
		struct tm dt;
		parse_iso8601_datetime(it->second.c_str(), dt);
		time_t ts = mktime (&dt);
		metaData.timestamp = (int64_t)ts;
	}
	it = this->metadataMap.find("changeset");
	if(it != this->metadataMap.end())
		metaData.changeset = atoi(it->second.c_str());
	it = this->metadataMap.find("uid");
	if(it != this->metadataMap.end())
		metaData.uid = atoi(it->second.c_str());
	it = this->metadataMap.find("user");
	if(it != this->metadataMap.end())
		metaData.username = it->second;
	it = this->metadataMap.find("visible");
	if(it != this->metadataMap.end())
		metaData.visible = it->second != "false"; 
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

#ifdef PYTHON_AWARE
PyOsmXmlEncode::PyOsmXmlEncode(PyObject* obj): OsmXmlEncodeBase()
{
	m_PyObj = obj;
	Py_INCREF(m_PyObj);
	m_Write = PyObject_GetAttrString(m_PyObj, "write");
	this->WriteStart();
}

PyOsmXmlEncode::~PyOsmXmlEncode()
{
	Py_XDECREF(m_Write);
	Py_XDECREF(m_PyObj);
}

#endif //PYTHON_AWARE

