#ifndef _OSMXML_H
#define _OSMXML_H

#include <memory>
#include "o5m.h"
#include "OsmData.h"
#include <expat.h>
#ifdef PYTHON_AWARE
#include <Python.h>
#endif

///Decodes a binary OSM XML stream and fires a series of events to the output object derived from IDataStreamHandler
class OsmXmlDecodeString
{
	friend class OsmChangeXmlDecodeString;
protected:
	XML_Parser parser;
	int xmlDepth;
	std::string currentObjectType, lastObjectType;
	TagMap metadataMap, tags;
	std::vector<int64_t> memObjIds;
	std::vector<std::string> memObjTypes, memObjRoles;
	bool firstParseCall, parseCompleted, stopProcessing;

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
	void DecodeFinish();

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
	void WriteStart(const TagMap &customAttribs);
	void EncodeMetaData(const class MetaData &metaData, std::stringstream &ss);

	virtual void write (const char* s, std::streamsize n);
	virtual void operator<< (const std::string &val);

public:
	OsmXmlEncodeBase();
	virtual ~OsmXmlEncodeBase();

	bool Sync();
	bool Reset();
	bool Finish();

	bool StoreIsDiff(bool);
	bool StoreBounds(double x1, double y1, double x2, double y2);
	bool StoreNode(int64_t objId, const class MetaData &metaData, 
		const TagMap &tags, double lat, double lon);
	bool StoreWay(int64_t objId, const class MetaData &metaData, 
		const TagMap &tags, const std::vector<int64_t> &refs);
	bool StoreRelation(int64_t objId, const class MetaData &metaData, const TagMap &tags, 
		const std::vector<std::string> &refTypeStrs, const std::vector<int64_t> &refIds, 
		const std::vector<std::string> &refRoles);
};

class OsmXmlEncode : public OsmXmlEncodeBase
{
private:
	std::ostream handle;

protected:
	virtual void write (const char* s, std::streamsize n)
	{
		this->handle.write(s, n);
	}

	virtual void operator<< (const std::string &val)
	{
		this->handle << val;
	}

public:
	OsmXmlEncode(std::streambuf &handle, const TagMap &customAttribs);
	virtual ~OsmXmlEncode();
};

#ifdef PYTHON_AWARE
class PyOsmXmlEncode : public OsmXmlEncodeBase
{
private:
	PyObject* m_PyObj;
	PyObject* m_Write;

	virtual void write (const char* s, std::streamsize n);
	virtual void operator<< (const std::string &val);

public:
	PyOsmXmlEncode(PyObject* obj, const TagMap &customAttribs);
	virtual ~PyOsmXmlEncode();	

	void SetOutput(PyObject* obj);
};
#endif //PYTHON_AWARE

///Decodes a binary OSM XML stream and fires a series of events to the output object derived from IDataStreamHandler
class OsmChangeXmlDecodeString
{
protected:
	XML_Parser parser;
	int xmlDepth;
	class OsmXmlDecodeString osmDataDecoder;
	std::shared_ptr<class OsmData> decodeBuff;
	std::string currentAction;
	bool ifunused;	
	bool parseCompleted;

public:
	std::string errString;
	bool parseCompletedOk;
	class IOsmChangeBlock *output;

	OsmChangeXmlDecodeString();
	virtual ~OsmChangeXmlDecodeString();

	bool DecodeSubString(const char *xml, size_t len, bool done);
	void DecodeFinish();

	void StartElement(const XML_Char *name, const XML_Char **atts);
	void EndElement(const XML_Char *name);
};

/// This handles data from a std::streambuf input.
class OsmChangeXmlDecode : public OsmChangeXmlDecodeString
{
private:
	char decodeBuff[10*1024];
	std::istream handle;

public:
	OsmChangeXmlDecode(std::streambuf &handleIn);
	virtual ~OsmChangeXmlDecode();

	bool DecodeNext();
	void DecodeHeader();
};

class OsmChangeXmlEncode : public OsmXmlEncodeBase
{
private:
	std::ostream handle;
	TagMap customAttribs;
	bool separateActions;

	void EncodeBySingleAction(const std::string &action, const std::vector<const class OsmObject *> &objs);

public:
	OsmChangeXmlEncode(std::streambuf &fiIn, const TagMap &customAttribsIn, bool separateActionsIn=false);
	virtual ~OsmChangeXmlEncode();

	void Encode(const class OsmChange &osmChange);

	virtual void write (const char* s, std::streamsize n);
	virtual void operator<< (const std::string &val);
};

#endif //_OSMXML_H

