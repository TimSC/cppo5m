#include "utils.h"
#include <iostream>
#include <sstream>
#include "o5m.h"
#include "osmxml.h"
using namespace std;

// ******* Utility funcs **********

void LoadFromO5m(std::streambuf &fi, std::shared_ptr<class IDataStreamHandler> output)
{
	class O5mDecode dec(fi);
	dec.output = output;
	dec.DecodeHeader();

	while (fi.in_avail()>0)
	{
		bool ok = dec.DecodeNext();
		if(!ok) break;
	}

	dec.DecodeFinish();
}

void LoadFromOsmXml(std::streambuf &fi, std::shared_ptr<class IDataStreamHandler> output)
{
	class OsmXmlDecode dec(fi);
	dec.output = output;
	dec.DecodeHeader();

	while (fi.in_avail()>0)
	{
		bool ok = dec.DecodeNext();
		if(!ok)
		{
			cout << dec.errString << endl;
			break;
		}
	}

	dec.DecodeFinish();
}

void LoadFromOsmChangeXml(std::streambuf &fi, std::shared_ptr<class IOsmChangeBlock> output)
{
	class OsmChangeXmlDecode dec(fi);
	dec.output = output;
	dec.DecodeHeader();

	while (fi.in_avail()>0)
	{
		bool ok = dec.DecodeNext();
		if(!ok)
		{
			cout << dec.errString << endl;
			break;
		}
	}

	dec.DecodeFinish();
}

void SaveToO5m(const class OsmData &osmData, std::streambuf &fi)
{
	class O5mEncode enc(fi);
	osmData.StreamTo(enc);
}

void SaveToOsmXml(const class OsmData &osmData, std::streambuf &fi)
{
	TagMap empty;
	class OsmXmlEncode enc(fi, empty);
	osmData.StreamTo(enc);
}

void SaveToOsmChangeXml(const class OsmChange &osmChange, bool separateActions, std::streambuf &fi)
{
	TagMap empty;
	class OsmChangeXmlEncode enc(fi, empty, separateActions);
	enc.Encode(osmChange);
}

void LoadFromO5m(const std::string &fi, std::shared_ptr<class IDataStreamHandler> output)
{
	std::istringstream buff(fi);
	LoadFromO5m(*buff.rdbuf(), output);
}

void LoadFromOsmXml(const std::string &fi, std::shared_ptr<class IDataStreamHandler> output)
{
	std::istringstream buff(fi);
	LoadFromOsmXml(*buff.rdbuf(), output);
}

void LoadFromOsmChangeXml(const std::string &fi, std::shared_ptr<class IOsmChangeBlock> output)
{
	std::istringstream buff(fi);
	LoadFromOsmChangeXml(*buff.rdbuf(), output);
}

