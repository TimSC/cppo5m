#ifndef _OSMXML_H
#define _OSMXML_H

#include "o5m.h"
#include <expat.h>
#ifdef PYTHON_AWARE
#include <Python.h>
#endif

///Decodes a binary OSM XML stream and fires a series of events to the output object derived from IDataStreamHandler
class OsmXmlDecodeString
{
protected:
	XML_Parser parser;
	int xmlDepth;
	std::string currentObjectType, lastObjectType;
	TagMap metadataMap, tags;
	std::vector<int64_t> memObjIds;
	std::vector<std::string> memObjTypes, memObjRoles;
	bool firstParseCall;

	void DecodeMetaData(class MetaData &metaData);

public:
	class IDataStreamHandler *output;
	std::string errString;
	bool parseCompletedOk;

	OsmXmlDecodeString();
	virtual ~OsmXmlDecodeString();

	virtual bool DecodeNext() {return false;};
	virtual void DecodeHeader() {};
	bool DecodeSubString(const char *xml, size_t len, bool done);

	void StartElement(const XML_Char *name, const XML_Char **atts);
	void EndElement(const XML_Char *name);
};

/// This handles data from a std::streambuf input.
class OsmXmlDecode : public OsmXmlDecodeString
{
private:
	char decodeBuff[10*1024];
	std::istream handle;

public:
	OsmXmlDecode(std::streambuf &handleIn);
	virtual ~OsmXmlDecode();

	bool DecodeNext();
	void DecodeHeader();
};

class OsmXmlEncodeBase : public IDataStreamHandler
{
protected:
	void WriteStart();
	void EncodeMetaData(const class MetaData &metaData, std::stringstream &ss);

public:
	OsmXmlEncodeBase();
	virtual ~OsmXmlEncodeBase();

	void Sync();
	void Reset();
	void Finish();

	void StoreIsDiff(bool);
	void StoreBounds(double x1, double y1, double x2, double y2);
	void StoreNode(int64_t objId, const class MetaData &metaData, 
		const TagMap &tags, double lat, double lon);
	void StoreWay(int64_t objId, const class MetaData &metaData, 
		const TagMap &tags, const std::vector<int64_t> &refs);
	void StoreRelation(int64_t objId, const class MetaData &metaData, const TagMap &tags, 
		const std::vector<std::string> &refTypeStrs, const std::vector<int64_t> &refIds, 
		const std::vector<std::string> &refRoles);

	virtual void write (const char* s, streamsize n)=0;
	virtual void operator<< (const string &val)=0;
};

class OsmXmlEncode : public OsmXmlEncodeBase
{
private:
	std::ostream handle;
	
	virtual void write (const char* s, streamsize n)
	{
		this->handle.write(s, n);
	}

	virtual void operator<< (const string &val)
	{
		this->handle << val;
	}

public:
	OsmXmlEncode(std::streambuf &handle);
	virtual ~OsmXmlEncode();
};

#ifdef PYTHON_AWARE
class PyOsmXmlEncode : public OsmXmlEncodeBase
{
private:
	PyObject* m_PyObj;
	PyObject* m_Write;

	virtual void write (const char* s, streamsize n)
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

	virtual void operator<< (const string &val)
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

public:
	PyOsmXmlEncode(PyObject* obj);
	virtual ~PyOsmXmlEncode();	

};
#endif //PYTHON_AWARE

#endif //_OSMXML_H

