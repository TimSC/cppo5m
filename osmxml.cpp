#include "osmxml.h"
#include <sstream>
#include <ctime>
#include <assert.h>
#include <cstring>
#include <limits>
extern "C" {
#include "iso8601lib/iso8601.h"
}
using namespace std;

static thread_local class OsmXmlLimits defaultOsmXmlLimits;

static std::string LimitMessage(const std::string &name, size_t limit, size_t actual)
{
	stringstream ss;
	ss << name << " limit exceeded; maximum is " << limit << ", got " << actual;
	return ss.str();
}

OsmXmlLimits::OsmXmlLimits()
{
	maxBytes = 0;
	maxDepth = 0;
	maxObjects = 0;
	maxTagsPerObject = 0;
	maxWayNodesPerObject = 0;
	maxRelationMembersPerObject = 0;
	maxAttributesPerElement = 0;
	maxAttributeBytes = 0;
}

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

void SetDefaultOsmXmlLimits(const class OsmXmlLimits &limits)
{
	defaultOsmXmlLimits = limits;
}

class OsmXmlLimits GetDefaultOsmXmlLimits()
{
	return defaultOsmXmlLimits;
}

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
			case '\n': dst << "&#10;"; break;
			default: dst << ch; break;
		}
	}
	return dst.str();
}

void XmlAttsToMap(const XML_Char **atts, std::map<std::string, std::string> &attribs)
{
	size_t i=0;
	while(atts[i] != NULL)
	{
		attribs[atts[i]] = atts[i+1];
		i += 2;
	}
}

static void StartElement(void *userData, const XML_Char *name, const XML_Char **atts)
{
	((class OsmXmlDecodeString *)userData)->StartElement(name, atts);
}

static void EndElement(void *userData, const XML_Char *name)
{
	((class OsmXmlDecodeString *)userData)->EndElement(name);
}

static void StartChangeElement(void *userData, const XML_Char *name, const XML_Char **atts)
{
	((class OsmChangeXmlDecodeString *)userData)->StartElement(name, atts);
}

static void EndChangeElement(void *userData, const XML_Char *name)
{
	((class OsmChangeXmlDecodeString *)userData)->EndElement(name);
}

// ************* Decoder *************

OsmXmlDecodeString::OsmXmlDecodeString() : OsmDecoder()
{
	xmlDepth = 0;
	parseCompleted = false;
	parseCompletedOk = false;
	parser = XML_ParserCreate(NULL);
	XML_SetUserData(parser, this);
	XML_SetElementHandler(parser, ::StartElement, ::EndElement);
	this->firstParseCall = true;
	stopProcessing = false;
	limits = GetDefaultOsmXmlLimits();
	bytesDecoded = 0;
	objectCount = 0;
	tagCount = 0;
	wayNodeCount = 0;
	relationMemberCount = 0;
}

OsmXmlDecodeString::~OsmXmlDecodeString()
{
	if(!this->parseCompleted)
		this->DecodeFinish();
	XML_ParserFree(parser);
}

void OsmXmlDecodeString::SetLimits(const class OsmXmlLimits &limitsIn)
{
	limits = limitsIn;
}

bool OsmXmlDecodeString::FailLimit(const std::string &message)
{
	errString = message;
	stopProcessing = true;
	XML_StopParser(parser, XML_FALSE);
	return false;
}

bool OsmXmlDecodeString::CheckLimit(size_t value, size_t limit, const std::string &message)
{
	if(limit > 0 && value > limit)
		return FailLimit(message);
	return true;
}

bool OsmXmlDecodeString::CheckElementLimits(const XML_Char *name, const XML_Char **atts)
{
	size_t attributeCount = 0;
	size_t attributeBytes = strlen(name);
	for(size_t i=0; atts[i] != NULL; i += 2)
	{
		attributeCount++;
		attributeBytes += strlen(atts[i]) + strlen(atts[i+1]);
	}

	if(!CheckLimit(attributeCount, limits.maxAttributesPerElement,
		LimitMessage("PGMAP_XML_MAX_ATTRIBUTES_PER_ELEMENT", limits.maxAttributesPerElement, attributeCount)))
		return false;
	if(!CheckLimit(attributeBytes, limits.maxAttributeBytes,
		LimitMessage("PGMAP_XML_MAX_ATTRIBUTE_BYTES", limits.maxAttributeBytes, attributeBytes)))
		return false;
	return true;
}

void OsmXmlDecodeString::StartElement(const XML_Char *name, const XML_Char **atts)
{
	this->xmlDepth ++;
	//cout << this->xmlDepth << " startel " << name << endl;

	if(!CheckLimit(this->xmlDepth, limits.maxDepth,
		LimitMessage("PGMAP_XML_MAX_DEPTH", limits.maxDepth, this->xmlDepth)))
		return;
	if(!CheckElementLimits(name, atts))
		return;

	std::map<std::string, std::string> attribs;
	XmlAttsToMap(atts, attribs);

	if(this->xmlDepth == 2)
	{
		this->currentObjectType = name;
		this->metadataMap = attribs;
		tagCount = 0;
		wayNodeCount = 0;
		relationMemberCount = 0;
		if(strcmp(name, "node") == 0 || strcmp(name, "way") == 0 || strcmp(name, "relation") == 0)
		{
			objectCount++;
			if(!CheckLimit(objectCount, limits.maxObjects,
				LimitMessage("CHANGESETS_MAXIMUM_ELEMENTS", limits.maxObjects, objectCount)))
				return;
		}
	}

	else if(this->xmlDepth == 3)
	{
		if(strcmp(name, "tag") == 0)
		{
			tagCount++;
			if(!CheckLimit(tagCount, limits.maxTagsPerObject,
				LimitMessage("PGMAP_XML_MAX_TAGS_PER_OBJECT", limits.maxTagsPerObject, tagCount)))
				return;
			this->tags[attribs["k"]] = attribs["v"];
		}
		if(strcmp(name, "nd") == 0 && currentObjectType == "way")
		{
			wayNodeCount++;
			if(!CheckLimit(wayNodeCount, limits.maxWayNodesPerObject,
				LimitMessage("WAYNODES_MAXIMUM", limits.maxWayNodesPerObject, wayNodeCount)))
				return;
			this->memObjIds.push_back(atol(attribs["ref"].c_str()));
		}
		if(strcmp(name, "member") == 0 && currentObjectType == "relation")
		{
			relationMemberCount++;
			if(!CheckLimit(relationMemberCount, limits.maxRelationMembersPerObject,
				LimitMessage("RELATION_MEMBERS_MAXIMUM", limits.maxRelationMembersPerObject, relationMemberCount)))
				return;
			this->memObjIds.push_back(atol(attribs["ref"].c_str()));
			this->memObjTypes.push_back(attribs["type"]);
			this->memObjRoles.push_back(attribs["role"]);
		}
	}
}

void OsmXmlDecodeString::EndElement(const XML_Char *name)
{
	//cout << this->xmlDepth << " endel " << name << endl;
	
	if(this->xmlDepth == 2)
	{	
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

			if(output != nullptr)
				stopProcessing |= output->StoreBounds(minlon, minlat, maxlon, maxlat);
		}
		else
		{
			if(this->lastObjectType != this->currentObjectType && output != nullptr)
			{
				stopProcessing |= output->Sync();
				stopProcessing |= output->Reset();
			}

			int64_t objId = 0;
			TagMap::iterator it = this->metadataMap.find("id");
			if(it != this->metadataMap.end())
				objId = atol(it->second.c_str());

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

				if(output != nullptr)
					stopProcessing |= output->StoreNode(objId, metaData, this->tags, lat, lon);
			}
			else if(this->currentObjectType == "way")
			{
				if(output != nullptr)
					stopProcessing |= output->StoreWay(objId, metaData, this->tags, this->memObjIds);
			}
			else if(this->currentObjectType == "relation")
			{
				if(output != nullptr)
					stopProcessing |= output->StoreRelation(objId, metaData, this->tags, 
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

void OsmXmlDecodeString::DecodeMetaData(class MetaData &metaData)
{
	TagMap::iterator it = this->metadataMap.find("version");
	if(it != this->metadataMap.end())
		metaData.version = atol(it->second.c_str());
	it = this->metadataMap.find("timestamp");
	if(it != this->metadataMap.end())
	{
		struct tm dt;
		int timezoneOffsetMin=0;
		ParseIso8601Datetime(it->second.c_str(), &dt, &timezoneOffsetMin);
		TmToUtc(&dt, timezoneOffsetMin);
		metaData.timestamp = (int64_t)timegm(&dt);
	}
	it = this->metadataMap.find("changeset");
	if(it != this->metadataMap.end())
		metaData.changeset = atol(it->second.c_str());
	it = this->metadataMap.find("uid");
	if(it != this->metadataMap.end())
		metaData.uid = atol(it->second.c_str());
	it = this->metadataMap.find("user");
	if(it != this->metadataMap.end())
		metaData.username = it->second;
	it = this->metadataMap.find("visible");
	if(it != this->metadataMap.end())
		metaData.visible = it->second != "false"; 
	it = this->metadataMap.find("current");
	if(it != this->metadataMap.end())
		metaData.current = it->second != "false";
}

bool OsmXmlDecodeString::DecodeSubString(const char *xml, size_t len, bool done)
{
	if(output == nullptr)
		throw runtime_error("OsmXmlDecode output pointer is null");
	if(this->parseCompleted)
		throw runtime_error("Decode already finished");

	if(len > std::numeric_limits<size_t>::max() - bytesDecoded)
		return FailLimit("XML_UPLOAD_MAXIMUM_BYTES limit exceeded");
	bytesDecoded += len;
	if(!CheckLimit(bytesDecoded, limits.maxBytes,
		LimitMessage("XML_UPLOAD_MAXIMUM_BYTES", limits.maxBytes, bytesDecoded)))
		return false;

	if(this->firstParseCall)
	{
		output->StoreIsDiff(false);
		this->firstParseCall = false;
	}

	if (XML_Parse(parser, xml, len, done) == XML_STATUS_ERROR)
	{
		if(stopProcessing)
			return false;
		stringstream ss;
		ss << XML_ErrorString(XML_GetErrorCode(parser))
			<< " at line " << XML_GetCurrentLineNumber(parser) << endl;
		errString = ss.str();
		return false;
	}
	if(done)
	{
		parseCompletedOk = true;
	}
	if(stopProcessing)
		return false;
	return !done;
}

void OsmXmlDecodeString::DecodeFinish()
{
	if(parseCompleted)
		throw runtime_error("Decode already finished");
	if(output != nullptr)
		output->Finish();
	this->output = nullptr;
	parseCompleted = true;
}

// ***********************************

OsmXmlDecode::OsmXmlDecode(std::streambuf &handleIn):
	OsmXmlDecodeString(),
	handle(&handleIn)
{
	output = nullptr;
}

OsmXmlDecode::~OsmXmlDecode()
{

}

bool OsmXmlDecode::DecodeNext()
{
	handle.read((char *)decodeBuff, sizeof(decodeBuff));

	bool done = handle.gcount()==0;
	return DecodeSubString(decodeBuff, handle.gcount(), done);
}

void OsmXmlDecode::DecodeHeader()
{

}

// ************* Encoder *************

OsmXmlEncodeBase::OsmXmlEncodeBase() : IDataStreamHandler()
{

}

OsmXmlEncodeBase::~OsmXmlEncodeBase()
{

}

void OsmXmlEncodeBase::WriteStart(const TagMap &customAttribs)
{
	*this << "<?xml version='1.0' encoding='UTF-8'?>\n";
	*this << "<osm";
	TagMap::const_iterator it = customAttribs.find("version");
	if(it != customAttribs.end())
	{
		*this << " version=\""; 
		*this << escapexml(it->second);
		*this << "\"";
	}
	else
		*this << " version=\"0.6\"";

	it = customAttribs.find("generator");
	if(it != customAttribs.end())
	{
		*this << " generator=\"";
		*this << escapexml(it->second);
		*this << "\"";
	}
	else
		*this << " generator=\"cppo5m\"";

	for(it = customAttribs.begin(); it != customAttribs.end(); it++)
	{
		if(it->first == "version" || it->first == "generator")
			continue;
		if(it->second.length() == 0)
			continue;
		*this << " ";
		*this << escapexml(it->first);
		*this <<"=\"";
		*this << escapexml(it->second);
		*this <<"\"";
	}

	*this << ">\n";
}

void OsmXmlEncodeBase::EncodeMetaData(const class MetaData &metaData, std::stringstream &ss)
{
	if(metaData.timestamp != 0)
	{
		time_t tt = metaData.timestamp;
		char buf[50];
		struct tm tmbuf;
		strftime(buf, sizeof(buf), "%FT%TZ", gmtime_r(&tt, &tmbuf));
		ss << " timestamp=\""<<buf<<"\"";
	}
	if(metaData.uid != 0)
		ss << " uid=\"" << metaData.uid << "\"";
	if(metaData.username.length() > 0)
		ss << " user=\"" << escapexml(metaData.username) << "\"";
	if(metaData.visible)
		ss << " visible=\"true\"";
	else
		ss << " visible=\"false\"";
	if(metaData.version != 0)
		ss << " version=\"" << metaData.version << "\"";
	if(metaData.changeset != 0)
		ss << " changeset=\"" << metaData.changeset << "\"";
}

bool OsmXmlEncodeBase::Sync()
{
	return false;
}

bool OsmXmlEncodeBase::Reset()
{
	return false;
}

bool OsmXmlEncodeBase::Finish()
{
	*this << "</osm>";
	return false;
}

bool OsmXmlEncodeBase::StoreIsDiff(bool)
{
	return false;
}

bool OsmXmlEncodeBase::StoreBounds(double x1, double y1, double x2, double y2)
{
	stringstream ss;
	ss.precision(9);
	ss << fixed << "  <bounds minlat=\""<<y1<<"\" minlon=\""<<x1<<"\" ";
	ss << "maxlat=\""<<y2<<"\" maxlon=\""<<x2<<"\" />" << endl;
	*this << ss.str();
	return false;
}

bool OsmXmlEncodeBase::StoreNode(int64_t objId, const class MetaData &metaData, 
	const TagMap &tags, double lat, double lon)
{
	stringstream ss;
	ss.precision(9);
	ss << "  <node id=\""<<objId<<"\"";
	this->EncodeMetaData(metaData, ss);
	ss << fixed << " lat=\""<<lat<<"\" lon=\""<<lon<<"\"";
	if(tags.size() == 0)
		ss <<" />" << endl;
	else
	{
		ss <<">" << endl;

		//Write tags
		for(TagMap::const_iterator it=tags.begin(); it!=tags.end(); it++)
		{
			ss << "    <tag k=\""<<escapexml(it->first)<<"\" v=\""<<escapexml(it->second)<<"\" />" << endl;
		}
		ss << "  </node>" << endl;
	}
	*this << ss.str();
	return false;
}

bool OsmXmlEncodeBase::StoreWay(int64_t objId, const class MetaData &metaData, 
	const TagMap &tags, const std::vector<int64_t> &refs)
{
	stringstream ss;
	ss << "  <way id=\""<<objId<<"\"";
	this->EncodeMetaData(metaData, ss);
	if(tags.size() == 0 && refs.size() == 0)
		ss <<" />" << endl;
	else
	{
		ss <<">" << endl;

		//Write node IDs
		for(size_t i=0; i<refs.size(); i++)
			ss << "    <nd ref=\""<<refs[i]<<"\" />" << endl;

		//Write tags
		for(TagMap::const_iterator it=tags.begin(); it!=tags.end(); it++)
			ss << "    <tag k=\""<<escapexml(it->first)<<"\" v=\""<<escapexml(it->second)<<"\" />" << endl;

		ss << "  </way>" << endl;
	}
	*this << ss.str();
	return false;
}

bool OsmXmlEncodeBase::StoreRelation(int64_t objId, const class MetaData &metaData, const TagMap &tags, 
	const std::vector<std::string> &refTypeStrs, const std::vector<int64_t> &refIds,
	const std::vector<std::string> &refRoles)
{
	if(refTypeStrs.size() != refIds.size() || refTypeStrs.size() != refRoles.size())
		throw std::invalid_argument("Length of ref vectors must be equal");

	stringstream ss;
	ss << "  <relation id=\""<<objId<<"\"";
	this->EncodeMetaData(metaData, ss);
	if(tags.size() == 0 && refTypeStrs.size() == 0)
		ss <<" />" << endl;
	else
	{
		ss <<">" << endl;

		//Write node IDs
		for(size_t i=0; i<refTypeStrs.size(); i++)
			ss << "    <member type=\""<<escapexml(refTypeStrs[i])<<"\" ref=\""<<refIds[i]<<"\" role=\""<<escapexml(refRoles[i])<<"\" />" << endl;

		//Write tags
		for(TagMap::const_iterator it=tags.begin(); it!=tags.end(); it++)
			ss << "    <tag k=\""<<escapexml(it->first)<<"\" v=\""<<escapexml(it->second)<<"\" />" << endl;

		ss << "  </relation>" << endl;
	}
	*this << ss.str();
	return false;
}

void OsmXmlEncodeBase::write (const char* s, std::streamsize n) {}

void OsmXmlEncodeBase::operator<< (const std::string &val) {}

// ****************************

OsmXmlEncode::OsmXmlEncode(std::streambuf &handleIn, const TagMap &customAttribs): OsmXmlEncodeBase(), handle(&handleIn)
{
	this->WriteStart(customAttribs);
}

OsmXmlEncode::~OsmXmlEncode()
{

}

// ****************************

#ifdef PYTHON_AWARE
PyOsmXmlEncode::PyOsmXmlEncode(PyObject* obj, const TagMap &customAttribs): OsmXmlEncodeBase()
{
	m_Write = NULL;
	m_PyObj = NULL;

	this->SetOutput(obj);
	this->WriteStart(customAttribs);
}

PyOsmXmlEncode::~PyOsmXmlEncode()
{
	Py_XDECREF(m_Write);
	Py_XDECREF(m_PyObj);
}

void PyOsmXmlEncode::write (const char* s, streamsize n)
{
	if(this->m_Write == NULL)
		return;
	#if PY_MAJOR_VERSION < 3
	PyObject* ret = PyObject_CallFunction(m_Write, (char *)"s#", s, n);
	#else
	PyObject* ret = PyObject_CallFunction(m_Write, (char *)"y#", s, n);
	#endif 
	Py_XDECREF(ret);
}

void PyOsmXmlEncode::operator<< (const string &val)
{
	if(this->m_Write == NULL)
		return;
	#if PY_MAJOR_VERSION < 3
	PyObject* ret = PyObject_CallFunction(m_Write, (char *)"s#", val.c_str(), val.length());
	#else
	PyObject* ret = PyObject_CallFunction(m_Write, (char *)"y#", val.c_str(), val.length());
	#endif 
	Py_XDECREF(ret);
}

void PyOsmXmlEncode::SetOutput(PyObject* obj)
{
	Py_XDECREF(m_Write);
	Py_XDECREF(m_PyObj);
	m_PyObj = obj;
	m_Write = PyObject_GetAttrString(obj, "write");
	Py_INCREF(m_PyObj);
}

#endif //PYTHON_AWARE

// ************* Osm Change Decoder *************

OsmChangeXmlDecodeString::OsmChangeXmlDecodeString():
	decodeBuff(new class OsmData())
{
	output = nullptr;
	xmlDepth = 0;
	parseCompleted = false;
	parseCompletedOk = false;
	ifunused = false;
	limits = GetDefaultOsmXmlLimits();
	bytesDecoded = 0;
	parser = XML_ParserCreate(NULL);
	XML_SetUserData(parser, this);
	XML_SetElementHandler(parser, ::StartChangeElement, ::EndChangeElement);
	decodeBuff->StoreIsDiff(true);
	osmDataDecoder.output = decodeBuff.get();
	osmDataDecoder.SetLimits(limits);
}

OsmChangeXmlDecodeString::~OsmChangeXmlDecodeString()
{
	if(!this->parseCompleted)
		this->DecodeFinish();
	XML_ParserFree(parser);
}

void OsmChangeXmlDecodeString::SetLimits(const class OsmXmlLimits &limitsIn)
{
	limits = limitsIn;
	osmDataDecoder.SetLimits(limitsIn);
}

bool OsmChangeXmlDecodeString::FailLimit(const std::string &message)
{
	errString = message;
	XML_StopParser(parser, XML_FALSE);
	return false;
}

bool OsmChangeXmlDecodeString::CheckLimit(size_t value, size_t limit, const std::string &message)
{
	if(limit > 0 && value > limit)
		return FailLimit(message);
	return true;
}

bool OsmChangeXmlDecodeString::CheckElementLimits(const XML_Char *name, const XML_Char **atts)
{
	size_t attributeCount = 0;
	size_t attributeBytes = strlen(name);
	for(size_t i=0; atts[i] != NULL; i += 2)
	{
		attributeCount++;
		attributeBytes += strlen(atts[i]) + strlen(atts[i+1]);
	}

	if(!CheckLimit(attributeCount, limits.maxAttributesPerElement,
		LimitMessage("PGMAP_XML_MAX_ATTRIBUTES_PER_ELEMENT", limits.maxAttributesPerElement, attributeCount)))
		return false;
	if(!CheckLimit(attributeBytes, limits.maxAttributeBytes,
		LimitMessage("PGMAP_XML_MAX_ATTRIBUTE_BYTES", limits.maxAttributeBytes, attributeBytes)))
		return false;
	return true;
}

void OsmChangeXmlDecodeString::StartElement(const XML_Char *name, const XML_Char **atts)
{
	this->xmlDepth ++;
	//cout << this->xmlDepth << " startel " << name << endl;

	if(!CheckLimit(this->xmlDepth, limits.maxDepth,
		LimitMessage("PGMAP_XML_MAX_DEPTH", limits.maxDepth, this->xmlDepth)))
		return;
	if(!CheckElementLimits(name, atts))
		return;

	if(this->xmlDepth == 2)
	{
		std::map<std::string, std::string> attribs;
		XmlAttsToMap(atts, attribs);

		currentAction = name;
		std::map<std::string, std::string>::iterator it = attribs.find("if-unused");
		this->ifunused = (it != attribs.end());
	}
	else if(this->xmlDepth > 2)
	{
		osmDataDecoder.xmlDepth = this->xmlDepth - 2;
		osmDataDecoder.StartElement(name, atts);
	}
}

void OsmChangeXmlDecodeString::EndElement(const XML_Char *name)
{
	//cout << this->xmlDepth << " endel " << name << endl;
	
	if(this->xmlDepth == 2)
	{
		output->StoreOsmData(currentAction, *decodeBuff, this->ifunused);
		decodeBuff->Clear();
		currentAction = "";
		ifunused = false;
	}
	else if(this->xmlDepth > 2)
	{
		osmDataDecoder.xmlDepth = this->xmlDepth - 1;
		osmDataDecoder.EndElement(name);
	}

	this->xmlDepth --;
}

bool OsmChangeXmlDecodeString::DecodeSubString(const char *xml, size_t len, bool done)
{
	if(this->parseCompleted)
		throw runtime_error("Decode already finished");
	if(output == NULL)
		throw runtime_error("OsmXmlDecode output pointer is null");

	if(len > std::numeric_limits<size_t>::max() - bytesDecoded)
		return FailLimit("XML_UPLOAD_MAXIMUM_BYTES limit exceeded");
	bytesDecoded += len;
	if(!CheckLimit(bytesDecoded, limits.maxBytes,
		LimitMessage("XML_UPLOAD_MAXIMUM_BYTES", limits.maxBytes, bytesDecoded)))
		return false;

	if (XML_Parse(parser, xml, len, done) == XML_STATUS_ERROR)
	{
		if(errString.size() > 0)
			return false;
		stringstream ss;
		ss << XML_ErrorString(XML_GetErrorCode(parser))
			<< " at line " << XML_GetCurrentLineNumber(parser) << endl;
		errString = ss.str();
		return false;
	}
	if(done)
	{
		parseCompletedOk = true;
	}
	return !done;
}

void OsmChangeXmlDecodeString::DecodeFinish()
{
	if(this->parseCompleted)
		throw runtime_error("Decode already finished");

	this->osmDataDecoder.DecodeFinish();
	this->decodeBuff->Finish();
	this->output = nullptr;

	this->parseCompleted = true;
}

// ***********************************

OsmChangeXmlDecode::OsmChangeXmlDecode(std::streambuf &handleIn):
	OsmChangeXmlDecodeString(),
	handle(&handleIn)
{

}

OsmChangeXmlDecode::~OsmChangeXmlDecode()
{

}

bool OsmChangeXmlDecode::DecodeNext()
{
	handle.read((char *)readBuff, sizeof(readBuff));

	bool done = handle.gcount()==0;
	return DecodeSubString(readBuff, handle.gcount(), done);
}

void OsmChangeXmlDecode::DecodeHeader()
{

}

// *************************************

OsmChangeXmlEncode::OsmChangeXmlEncode(std::streambuf &fiIn, const TagMap &customAttribsIn, bool separateActionsIn) : 
	OsmXmlEncodeBase(), handle(&fiIn)
{
	customAttribs = customAttribsIn;
	separateActions = separateActionsIn;
}

OsmChangeXmlEncode::~OsmChangeXmlEncode()
{

}

void OsmChangeXmlEncode::EncodeBySingleAction(const std::string &action, const std::vector<const class OsmObject *> &objs)
{
	for(size_t i=0; i<objs.size(); i++)
	{
		stringstream actionTagOpen;
		actionTagOpen << "<" << action << ">" << endl;
		*this << actionTagOpen.str();

		objs[i]->StreamTo(*this);

		stringstream actionTagClose;
		actionTagClose << "</" << action << ">" << endl;
		*this << actionTagClose.str();
	}
}

void OsmChangeXmlEncode::Encode(const class OsmChange &osmChange)
{
	*this << "<osmChange version=\"0.6\" generator=\"cppo5m\">\n";
	for(size_t i=0; i<osmChange.blocks.size(); i++)
	{
		const class OsmData &block = osmChange.blocks[i];
		const std::string &action = osmChange.actions[i];

		if(separateActions)
		{
			std::vector<const class OsmObject *> objs;

			if(action != "delete")
			{
				for(size_t j=0; j<block.nodes.size(); j++) 
					objs.push_back(&block.nodes[j]);
				for(size_t j=0; j<block.ways.size(); j++) 
					objs.push_back(&block.ways[j]);
				for(size_t j=0; j<block.relations.size(); j++) 
					objs.push_back(&block.relations[j]);
			}
			else
			{
				for(size_t j=0; j<block.relations.size(); j++) 
					objs.push_back(&block.relations[j]);
				for(size_t j=0; j<block.ways.size(); j++) 
					objs.push_back(&block.ways[j]);
				for(size_t j=0; j<block.nodes.size(); j++) 
					objs.push_back(&block.nodes[j]);
			}

			this->EncodeBySingleAction(action, objs);
		}
		else
		{
			stringstream actionTagOpen;
			actionTagOpen << "<" << action << ">" << endl;
			*this << actionTagOpen.str();

			if(action != "delete")
			{
				for(size_t j=0; j<block.nodes.size(); j++) 
					block.nodes[j].StreamTo(*this);
				for(size_t j=0; j<block.ways.size(); j++) 
					block.ways[j].StreamTo(*this);
				for(size_t j=0; j<block.relations.size(); j++) 
					block.relations[j].StreamTo(*this);
			}
			else
			{
				for(size_t j=0; j<block.relations.size(); j++) 
					block.relations[j].StreamTo(*this);
				for(size_t j=0; j<block.ways.size(); j++) 
					block.ways[j].StreamTo(*this);
				for(size_t j=0; j<block.nodes.size(); j++) 
					block.nodes[j].StreamTo(*this);
			}

			stringstream actionTagClose;
			actionTagClose << "</" << action << ">" << endl;
			*this << actionTagClose.str();
		}
	}

	*this << "</osmChange>\n";
}

void OsmChangeXmlEncode::write (const char* s, streamsize n)
{
	this->handle.write(s, n);
}

void OsmChangeXmlEncode::operator<< (const string &val)
{
	this->handle << val;
}
