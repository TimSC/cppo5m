#include <cassert>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include "o5m.h"
#include "intmath.h"
#include "varint.h"
using namespace std;

static const unsigned char O5M_NODE = 0x10;
static const unsigned char O5M_WAY = 0x11;
static const unsigned char O5M_RELATION = 0x12;
static const unsigned char O5M_BBOX = 0xdb;
static const unsigned char O5M_HEADER = 0xe0;
static const unsigned char O5M_EOF = 0xfe;
static const unsigned char O5M_RESET = 0xff;

static void ReadExactLength(std::istream &str, char *out, size_t len)
{
	size_t total = 0;
	while(total < len)
	{
		str.read(&out[total], len-total);
		total += str.gcount();
		if(str.fail() && total < len)
			throw OsmDecodeError("o5m input ends part way through an object");
	}
}

// ****** o5m decoder ******

O5mDecode::O5mDecode(std::streambuf &input, IDataStreamHandler &output) :
	OsmDecoder(output),
	handle(&input),
	headerRead(false),
	refTableLengthThreshold(250),
	refTableMaxSize(15000),
	maxObjectBytes(256 * 1024 * 1024)
{
	this->stringPairs.SetBufferSize(this->refTableMaxSize);
	this->ResetDeltaCoding();
}

void O5mDecode::ResetDeltaCoding()
{
	this->lastObjId = 0;
	this->lastTimeStamp = 0;
	this->lastChangeSet = 0;
	this->stringPairs.Clear();
	this->lastLat = 0;
	this->lastLon = 0;
	this->lastRefNode = 0;
	this->lastRefWay = 0;
	this->lastRefRelation = 0;
}

bool O5mDecode::DecodeNext()
{
	if(this->finished)
		return false;

	bool more = true;
	try
	{
		if(!this->headerRead)
		{
			this->ReadHeader();
			return true;
		}
		more = this->DecodeBlock();
	}
	catch(const OsmDecodeError &)
	{
		throw;
	}
	catch(const std::bad_alloc &)
	{
		throw;
	}
	catch(const std::runtime_error &err)
	{
		//Low level read failures; handler exceptions never pass through here
		throw OsmDecodeError(std::string("o5m decode failed: ") + err.what());
	}

	//Pending objects are handed over outside the try block, so exceptions
	//thrown by the handler reach the caller unchanged.
	if(!more)
	{
		this->MarkFinished();
		return false;
	}
	return true;
}

void O5mDecode::ReadHeader()
{
	int first = this->handle.get();
	if(first == std::char_traits<char>::eof())
		throw OsmDecodeError("o5m input is empty");
	if((unsigned char)first != O5M_RESET)
		throw OsmDecodeError("o5m input does not start with a reset byte");
	int second = this->handle.get();
	if(second == std::char_traits<char>::eof() || (unsigned char)second != O5M_HEADER)
		throw OsmDecodeError("o5m header is missing");

	std::string fileType;
	this->ReadBlock(fileType);
	if(fileType != "o5m2" && fileType != "o5c2")
		throw OsmDecodeError("o5m header has an unknown file type");
	this->headerRead = true;
	this->output.StoreIsDiff(fileType == "o5c2");
}

void O5mDecode::ReadBlock(std::string &out)
{
	uint64_t length = DecodeVarint(this->handle);
	if(length > this->maxObjectBytes)
		throw OsmDecodeError("o5m object is larger than the maximum allowed size");
	out.resize(length);
	if(length > 0)
		ReadExactLength(this->handle, &out[0], length);
}

///Reads one block. Returns false at the end of the input.
bool O5mDecode::DecodeBlock()
{
	int raw = this->handle.get();
	if(raw == std::char_traits<char>::eof())
		return false; //Tolerate a missing end marker

	unsigned char code = (unsigned char)raw;
	switch(code)
	{
	case O5M_NODE:
		this->DecodeNode();
		return true;
	case O5M_WAY:
		this->DecodeWay();
		return true;
	case O5M_RELATION:
		this->DecodeRelation();
		return true;
	case O5M_BBOX:
		{
			Bounds bounds;
			this->DecodeBoundingBox(bounds);
			this->output.StoreBounds(bounds);
		}
		return true;
	case O5M_RESET:
		this->ResetDeltaCoding();
		this->output.Reset();
		return true;
	case O5M_EOF:
		return false;
	}
	if(code >= 0xf0)
		return true; //Other single byte codes carry no data

	//Skip anything else, which includes sync markers and repeated headers
	this->ReadBlock(this->tmpBuff);
	return true;
}

void O5mDecode::DecodeBoundingBox(Bounds &out)
{
	this->ReadBlock(this->tmpBuff);
	std::istringstream stream(this->tmpBuff);

	//South-western corner then north-eastern corner
	out.minLon = DecodeZigzag(stream) / 1e7;
	out.minLat = DecodeZigzag(stream) / 1e7;
	out.maxLon = DecodeZigzag(stream) / 1e7;
	out.maxLat = DecodeZigzag(stream) / 1e7;
}

void O5mDecode::DecodeSingleString(std::istream &stream, std::string &out)
{
	out.clear();
	while(true)
	{
		int code = stream.get();
		if(code == std::char_traits<char>::eof())
			throw OsmDecodeError("o5m string is not terminated");
		if(code == 0x00)
			return;
		out.push_back((char)code);
	}
}

void O5mDecode::ConsiderAddToStringRefTable(const std::string &firstStr, const std::string &secondStr)
{
	if(firstStr.size() + secondStr.size() <= this->refTableLengthThreshold)
	{
		this->combinedRawTmpBuff = "";
		this->combinedRawTmpBuff.append(firstStr);
		this->combinedRawTmpBuff.append("\x00", 1);
		this->combinedRawTmpBuff.append(secondStr);
		this->combinedRawTmpBuff.append("\x00", 1);
		this->AddBuffToStringRefTable(this->combinedRawTmpBuff);
	}
}

void O5mDecode::AddBuffToStringRefTable(const std::string &buff)
{
	//Make sure it does not grow forever
	if(this->stringPairs.AvailableSpace() == 0)
		this->stringPairs.PopFront();

	this->stringPairs.PushBack(buff);
}

///Reads a string pair, either inline or as a reference to an earlier one.
///Returns false if the stream has no more data.
bool O5mDecode::ReadStringPair(std::istream &stream, std::string &firstStr, std::string &secondStr)
{
	if(stream.peek() == std::char_traits<char>::eof())
	{
		firstStr.clear();
		secondStr.clear();
		return false;
	}

	uint64_t ref = DecodeVarint(stream);
	if(ref == 0x00)
	{
		//Found new pair of strings
		this->DecodeSingleString(stream, firstStr);
		this->DecodeSingleString(stream, secondStr);
		this->ConsiderAddToStringRefTable(firstStr, secondStr);
	}
	else
	{
		if(ref > this->stringPairs.Size())
			throw OsmDecodeError("o5m string reference is out of range");
		const std::string &prevPair = this->stringPairs[this->stringPairs.Size()-ref];
		std::istringstream ss(prevPair);
		this->DecodeSingleString(ss, firstStr);
		this->DecodeSingleString(ss, secondStr);
	}
	return true;
}

///Decodes the ID and metadata that start every object.
void O5mDecode::DecodeObjectStart(std::istream &stream, OsmObject &obj)
{
	this->lastObjId = WrapAdd(this->lastObjId, DecodeZigzag(stream));
	obj.objId = this->lastObjId;

	MetaData &out = obj.metaData;
	out = MetaData();
	out.version = DecodeVarint(stream);
	if(out.version != 0)
	{
		this->lastTimeStamp = WrapAdd(this->lastTimeStamp, DecodeZigzag(stream));
		out.timestamp = this->lastTimeStamp;
		if(out.timestamp != 0)
		{
			this->lastChangeSet = WrapAdd(this->lastChangeSet, DecodeZigzag(stream));
			out.changeset = this->lastChangeSet;

			std::string uidStr;
			this->ReadStringPair(stream, uidStr, out.username);
			if(uidStr.size() > 0)
			{
				std::istringstream uidStream(uidStr);
				out.uid = DecodeVarint(uidStream);
			}
		}
	}
}

void O5mDecode::DecodeTags(std::istream &stream, TagMap &out)
{
	out.clear();
	std::string firstString, secondString;
	while(this->ReadStringPair(stream, firstString, secondString))
		out[firstString] = secondString;
}

void O5mDecode::DecodeNode()
{
	this->ReadBlock(this->tmpBuff);
	std::istringstream stream(this->tmpBuff);
	OsmNode &node = this->tmpNode;

	this->DecodeObjectStart(stream, node);

	this->lastLon = WrapAdd(this->lastLon, DecodeZigzag(stream));
	this->lastLat = WrapAdd(this->lastLat, DecodeZigzag(stream));
	node.lon = this->lastLon / 1e7;
	node.lat = this->lastLat / 1e7;

	this->DecodeTags(stream, node.tags);
	this->output.StoreNode(node);
}

void O5mDecode::DecodeWay()
{
	this->ReadBlock(this->tmpBuff);
	std::istringstream stream(this->tmpBuff);
	OsmWay &way = this->tmpWay;

	this->DecodeObjectStart(stream, way);

	uint64_t refLen = DecodeVarint(stream);
	if(refLen > this->tmpBuff.size())
		throw OsmDecodeError("o5m way reference section is longer than the way");
	std::string refData(refLen, '\0');
	if(refLen > 0)
		ReadExactLength(stream, &refData[0], refLen);
	std::istringstream refStream(refData);

	way.refs.clear();
	while(refStream.peek() != std::char_traits<char>::eof())
	{
		this->lastRefNode = WrapAdd(this->lastRefNode, DecodeZigzag(refStream));
		way.refs.push_back(this->lastRefNode);
	}

	this->DecodeTags(stream, way.tags);
	this->output.StoreWay(way);
}

void O5mDecode::DecodeRelation()
{
	this->ReadBlock(this->tmpBuff);
	std::istringstream stream(this->tmpBuff);
	OsmRelation &relation = this->tmpRelation;

	this->DecodeObjectStart(stream, relation);

	uint64_t refLen = DecodeVarint(stream);
	if(refLen > this->tmpBuff.size())
		throw OsmDecodeError("o5m relation member section is longer than the relation");
	std::string refData(refLen, '\0');
	if(refLen > 0)
		ReadExactLength(stream, &refData[0], refLen);
	std::istringstream refStream(refData);

	relation.members.clear();
	std::string typeAndRole;
	while(refStream.peek() != std::char_traits<char>::eof())
	{
		int64_t deltaRef = DecodeZigzag(refStream);

		uint64_t refIndex = DecodeVarint(refStream); //Index into reference table
		if(refIndex == 0)
		{
			this->DecodeSingleString(refStream, typeAndRole);
			if(typeAndRole.size() <= this->refTableLengthThreshold)
				this->AddBuffToStringRefTable(typeAndRole);
		}
		else
		{
			if(refIndex > this->stringPairs.Size())
				throw OsmDecodeError("o5m string reference is out of range");
			typeAndRole = this->stringPairs[this->stringPairs.Size()-refIndex];
		}

		if(typeAndRole.size() < 1)
			throw OsmDecodeError("o5m relation member has no type");

		RelationMember member;
		member.role.assign(typeAndRole, 1, std::string::npos);
		switch(typeAndRole[0])
		{
		case '0':
			this->lastRefNode = WrapAdd(this->lastRefNode, deltaRef);
			member.type = ObjectType::Node;
			member.ref = this->lastRefNode;
			break;
		case '1':
			this->lastRefWay = WrapAdd(this->lastRefWay, deltaRef);
			member.type = ObjectType::Way;
			member.ref = this->lastRefWay;
			break;
		case '2':
			this->lastRefRelation = WrapAdd(this->lastRefRelation, deltaRef);
			member.type = ObjectType::Relation;
			member.ref = this->lastRefRelation;
			break;
		default:
			throw OsmDecodeError("o5m relation member has an invalid type");
		}
		relation.members.push_back(member);
	}

	this->DecodeTags(stream, relation.tags);
	this->output.StoreRelation(relation);
}

// ************** o5m encoder ****************

O5mEncode::O5mEncode(std::shared_ptr<ByteSink> sinkIn) :
	OsmEncoder(sinkIn),
	refTableLengthThreshold(250),
	refTableMaxSize(15000),
	runningRefOffset(0),
	writtenHeader(false)
{
	this->stringPairs.SetBufferSize(this->refTableMaxSize);
	this->ResetDeltaCoding();
}

O5mEncode::O5mEncode(std::streambuf &output) :
	O5mEncode(std::make_shared<StreamSink>(output))
{

}

void O5mEncode::WriteStart(bool isDiff)
{
	const char start[] = {(char)O5M_RESET, (char)O5M_HEADER};
	this->Write(start, 2);
	std::string headerData = isDiff ? "o5c2" : "o5m2";
	this->Write(EncodeVarint(headerData.size()));
	this->Write(headerData);

	this->writtenHeader = true;
	this->ResetDeltaCoding();
}

void O5mEncode::WriteBlock(char code, const std::string &data)
{
	this->Write(&code, 1);
	this->Write(EncodeVarint(data.size()));
	this->Write(data);
}

void O5mEncode::ResetDeltaCoding()
{
	this->lastObjId = 0;
	this->lastTimeStamp = 0;
	this->lastChangeSet = 0;
	this->stringPairs.Clear();
	this->stringPairsDict.clear();
	this->runningRefOffset = 0;
	this->lastLat = 0;
	this->lastLon = 0;
	this->lastRefNode = 0;
	this->lastRefWay = 0;
	this->lastRefRelation = 0;
}

void O5mEncode::StoreIsDiff(bool isDiff)
{
	if(!this->writtenHeader)
		this->WriteStart(isDiff);
}

void O5mEncode::StoreBounds(const Bounds &bounds)
{
	if(!this->writtenHeader)
		this->WriteStart(false);

	//South-western corner then north-eastern corner
	std::string bboxData;
	AppendZigzag(RoundCoord(bounds.minLon * 1e7), bboxData);
	AppendZigzag(RoundCoord(bounds.minLat * 1e7), bboxData);
	AppendZigzag(RoundCoord(bounds.maxLon * 1e7), bboxData);
	AppendZigzag(RoundCoord(bounds.maxLat * 1e7), bboxData);

	this->WriteBlock((char)O5M_BBOX, bboxData);
}

///Encodes the ID and metadata that start every object.
void O5mEncode::EncodeObjectStart(const OsmObject &obj, std::string &out)
{
	AppendZigzag(WrapSub(obj.objId, this->lastObjId), out);
	this->lastObjId = obj.objId;

	const MetaData &metaData = obj.metaData;
	AppendVarint(metaData.version, out);
	if(metaData.version == 0)
		return; //The format has nowhere to put the rest without a version

	AppendZigzag(WrapSub(metaData.timestamp, this->lastTimeStamp), out);
	this->lastTimeStamp = metaData.timestamp;
	if(metaData.timestamp == 0)
		return;

	AppendZigzag(WrapSub(metaData.changeset, this->lastChangeSet), out);
	this->lastChangeSet = metaData.changeset;
	std::string encUid;
	if(metaData.uid != 0)
		encUid = EncodeVarint(metaData.uid);
	this->WriteStringPair(encUid, metaData.username, out);
}

void O5mEncode::EncodeTags(const TagMap &tags, std::string &out)
{
	for(TagMap::const_iterator it=tags.begin(); it != tags.end(); it++)
		this->WriteStringPair(it->first, it->second, out);
}

bool O5mEncode::FindStringPairsIndex(const std::string &needle, size_t &indexOut)
{
	map<std::string, int64_t>::iterator it = this->stringPairsDict.find(needle);
	if(it == this->stringPairsDict.end())
		return false;
	indexOut = this->runningRefOffset - it->second;
	return true;
}

void O5mEncode::WriteStringPair(const std::string &firstString, const std::string &secondString,
	std::string &out)
{
	if(firstString.find('\0') != std::string::npos || secondString.find('\0') != std::string::npos)
		throw std::invalid_argument("o5m strings cannot contain a zero byte");

	std::string encodedStrings = firstString;
	encodedStrings.append("\x00", 1);
	encodedStrings.append(secondString);
	encodedStrings.append("\x00", 1);
	bool useTable = firstString.size() + secondString.size() <= this->refTableLengthThreshold;
	if(useTable)
	{
		size_t existIndex = 0;
		if(this->FindStringPairsIndex(encodedStrings, existIndex))
		{
			AppendVarint(existIndex, out);
			return;
		}
	}

	out.append("\x00", 1);
	out.append(encodedStrings);
	if(useTable)
		this->AddToRefTable(encodedStrings);
}

void O5mEncode::AddToRefTable(const std::string &encodedStrings)
{
	assert(this->stringPairs.Size() == this->stringPairsDict.size());

	//Make sure it does not grow forever
	if(this->stringPairs.AvailableSpace() == 0)
	{
		const string st = this->stringPairs.PopFront();
		this->stringPairsDict.erase(st);
	}

	this->stringPairs.PushBack(encodedStrings);
	this->stringPairsDict[encodedStrings] = this->runningRefOffset;
	this->runningRefOffset ++;
}

void O5mEncode::StoreNode(const OsmNode &node)
{
	if(!this->writtenHeader)
		this->WriteStart(false);

	std::string data;
	this->EncodeObjectStart(node, data);

	int64_t lon = RoundCoord(node.lon * 1e7);
	AppendZigzag(WrapSub(lon, this->lastLon), data);
	this->lastLon = lon;
	int64_t lat = RoundCoord(node.lat * 1e7);
	AppendZigzag(WrapSub(lat, this->lastLat), data);
	this->lastLat = lat;

	this->EncodeTags(node.tags, data);
	this->WriteBlock((char)O5M_NODE, data);
}

void O5mEncode::StoreWay(const OsmWay &way)
{
	if(!this->writtenHeader)
		this->WriteStart(false);

	std::string data;
	this->EncodeObjectStart(way, data);

	std::string encRefs;
	for(size_t i=0; i<way.refs.size(); i++)
	{
		AppendZigzag(WrapSub(way.refs[i], this->lastRefNode), encRefs);
		this->lastRefNode = way.refs[i];
	}
	AppendVarint(encRefs.size(), data);
	data.append(encRefs);

	this->EncodeTags(way.tags, data);
	this->WriteBlock((char)O5M_WAY, data);
}

void O5mEncode::StoreRelation(const OsmRelation &relation)
{
	if(!this->writtenHeader)
		this->WriteStart(false);

	std::string data;
	this->EncodeObjectStart(relation, data);

	std::string encRefs;
	for(size_t i=0; i<relation.members.size(); i++)
	{
		const RelationMember &member = relation.members[i];
		if(member.role.find('\0') != std::string::npos)
			throw std::invalid_argument("o5m strings cannot contain a zero byte");

		char typeCode = '0';
		int64_t deltaRef = 0;
		switch(member.type)
		{
		case ObjectType::Node:
			typeCode = '0';
			deltaRef = WrapSub(member.ref, this->lastRefNode);
			this->lastRefNode = member.ref;
			break;
		case ObjectType::Way:
			typeCode = '1';
			deltaRef = WrapSub(member.ref, this->lastRefWay);
			this->lastRefWay = member.ref;
			break;
		case ObjectType::Relation:
			typeCode = '2';
			deltaRef = WrapSub(member.ref, this->lastRefRelation);
			this->lastRefRelation = member.ref;
			break;
		default:
			throw std::invalid_argument("Relation member has an invalid type");
		}
		AppendZigzag(deltaRef, encRefs);

		std::string typeCodeAndRole(1, typeCode);
		typeCodeAndRole.append(member.role);

		size_t refIndex = 0;
		if(this->FindStringPairsIndex(typeCodeAndRole, refIndex))
		{
			AppendVarint(refIndex, encRefs);
		}
		else
		{
			encRefs.append("\x00", 1); //String start byte
			encRefs.append(typeCodeAndRole);
			encRefs.append("\x00", 1); //String end byte
			if(typeCodeAndRole.size() <= this->refTableLengthThreshold)
				this->AddToRefTable(typeCodeAndRole);
		}
	}
	AppendVarint(encRefs.size(), data);
	data.append(encRefs);

	this->EncodeTags(relation.tags, data);
	this->WriteBlock((char)O5M_RELATION, data);
}

void O5mEncode::Sync()
{
	if(!this->writtenHeader)
		this->WriteStart(false);
	this->Write("\xee\x07\x00\x00\x00\x00\x00\x00\x00", 9);
}

void O5mEncode::Reset()
{
	if(!this->writtenHeader)
		this->WriteStart(false);
	const char code = (char)O5M_RESET;
	this->Write(&code, 1);
	this->ResetDeltaCoding();
}

void O5mEncode::Finish()
{
	if(!this->writtenHeader)
		this->WriteStart(false);
	const char code = (char)O5M_EOF;
	this->Write(&code, 1);
}
