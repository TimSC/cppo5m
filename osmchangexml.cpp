#include "osmchangexml.h"
#include <cstring>
using namespace std;

// ************* Osm Change Decoder *************

OsmChangeXmlParser::OsmChangeXmlParser(IOsmChangeHandler &outputIn, const OsmXmlLimits &limitsIn) :
	XmlPushParser(limitsIn),
	output(outputIn),
	reader(this->limits),
	inAction(false),
	inObject(false)
{

}

void OsmChangeXmlParser::OnStartElement(const char *name, const char **atts)
{
	if(this->depth == 1)
	{
		if(strcmp(name, "osmChange") != 0)
			throw OsmDecodeError(std::string("Expected an osmChange root element but found ") + name);
	}
	else if(this->depth == 2)
	{
		if(strcmp(name, "create") != 0 && strcmp(name, "modify") != 0 && strcmp(name, "delete") != 0)
			throw OsmDecodeError(std::string("Unknown osmChange action: ") + name);

		this->block.action = name;
		this->block.ifUnused = false;
		for(size_t i=0; atts[i] != nullptr; i += 2)
			if(strcmp(atts[i], "if-unused") == 0)
				this->block.ifUnused = true;
		this->block.data.Clear();
		this->block.data.isDiff = true;
		this->inAction = true;
	}
	else if(this->depth == 3 && this->inAction)
	{
		if(OsmXmlObjectReader::IsObject(name))
		{
			this->reader.StartObject(name, atts);
			this->inObject = true;
		}
	}
	else if(this->depth == 4 && this->inObject)
	{
		this->reader.StartChild(name, atts);
	}
}

void OsmChangeXmlParser::OnEndElement(const char *name)
{
	if(this->depth == 3 && this->inObject)
	{
		this->inObject = false;
		this->reader.EndObject(this->block.data);
	}
	else if(this->depth == 2 && this->inAction)
	{
		this->inAction = false;
		this->output.StoreChangeBlock(this->block);
		this->block.data.Clear();
	}
}

void OsmChangeXmlParser::OnComplete()
{
	this->output.Finish();
}

// ***********************************

OsmChangeXmlDecode::OsmChangeXmlDecode(std::streambuf &input, IOsmChangeHandler &output,
	const OsmXmlLimits &limits):
	handle(&input),
	parser(output, limits),
	buffer(64 * 1024),
	finished(false)
{

}

bool OsmChangeXmlDecode::DecodeNext()
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
		this->finished = true;
	return !done;
}

void OsmChangeXmlDecode::Decode()
{
	while(this->DecodeNext()) {}
}

// *************************************

OsmChangeXmlEncode::OsmChangeXmlEncode(std::shared_ptr<ByteSink> sinkIn, const TagMap &customAttribsIn,
	bool separateActionsIn) :
	sink(sinkIn),
	customAttribs(customAttribsIn),
	separateActions(separateActionsIn),
	writtenHeader(false)
{
	if(!this->sink)
		throw std::invalid_argument("Encoder sink is null");
}

OsmChangeXmlEncode::OsmChangeXmlEncode(std::streambuf &output, const TagMap &customAttribsIn,
	bool separateActionsIn) :
	OsmChangeXmlEncode(std::make_shared<StreamSink>(output), customAttribsIn, separateActionsIn)
{

}

void OsmChangeXmlEncode::WriteStart()
{
	std::string out = "<?xml version='1.0' encoding='UTF-8'?>\n<osmChange";
	AppendXmlRootAttribs(this->customAttribs, out);
	out.append(">\n");
	this->writtenHeader = true;
	this->sink->Write(out);
}

void OsmChangeXmlEncode::StoreChangeBlock(const OsmChangeBlock &block)
{
	if(block.action != "create" && block.action != "modify" && block.action != "delete")
		throw std::invalid_argument("Unknown osmChange action: " + block.action);
	if(!this->writtenHeader)
		this->WriteStart();

	std::string open = "<" + block.action;
	if(block.ifUnused && block.action == "delete")
		open.append(" if-unused=\"true\"");
	open.append(">\n");
	std::string close = "</" + block.action + ">\n";

	//Objects are written so that nothing is referenced before it exists, or
	//after it has gone
	std::vector<std::string> objects;
	const OsmData &data = block.data;
	objects.reserve(data.nodes.size() + data.ways.size() + data.relations.size());
	if(block.action != "delete")
	{
		for(const OsmNode &node : data.nodes)
			{ objects.emplace_back(); AppendXmlNode(node, objects.back()); }
		for(const OsmWay &way : data.ways)
			{ objects.emplace_back(); AppendXmlWay(way, objects.back()); }
		for(const OsmRelation &relation : data.relations)
			{ objects.emplace_back(); AppendXmlRelation(relation, objects.back()); }
	}
	else
	{
		for(const OsmRelation &relation : data.relations)
			{ objects.emplace_back(); AppendXmlRelation(relation, objects.back()); }
		for(const OsmWay &way : data.ways)
			{ objects.emplace_back(); AppendXmlWay(way, objects.back()); }
		for(const OsmNode &node : data.nodes)
			{ objects.emplace_back(); AppendXmlNode(node, objects.back()); }
	}

	if(this->separateActions)
	{
		for(const std::string &object : objects)
		{
			this->sink->Write(open);
			this->sink->Write(object);
			this->sink->Write(close);
		}
	}
	else
	{
		this->sink->Write(open);
		for(const std::string &object : objects)
			this->sink->Write(object);
		this->sink->Write(close);
	}
}

void OsmChangeXmlEncode::Finish()
{
	if(!this->writtenHeader)
		this->WriteStart();
	this->sink->Write("</osmChange>\n");
}

void OsmChangeXmlEncode::Encode(const OsmChange &osmChange)
{
	for(const OsmChangeBlock &block : osmChange.blocks)
		this->StoreChangeBlock(block);
	this->Finish();
}
