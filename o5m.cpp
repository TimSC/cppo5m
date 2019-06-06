
#include <assert.h>
#include <stdexcept>
#include <iostream>
#include <stdlib.h>
#include <cmath>
#include "varint.h"
#include "o5m.h"
#include <iostream>
using namespace std;

// ****** o5m utilities ******

void TestDecodeNumber()
{
	assert (DecodeVarint("\x05") == 5);
	assert (DecodeVarint("\x7f") == 127);
	assert (DecodeVarint("\xc3\x02") == 323);
	assert (DecodeVarint("\x80\x80\x01") == 16384);
	assert (DecodeZigzag("\x08") == 4);
	assert (DecodeZigzag("\x80\x01") == 64);
	assert (DecodeZigzag("\x03") == -2);
	assert (DecodeZigzag("\x05") == -3);
	assert (DecodeZigzag("\x81\x01") == -65);
}

void TestEncodeNumber()
{
	assert (EncodeVarint(5) == "\x05");
	assert (EncodeVarint(127) == "\x7f");
	assert (EncodeVarint(323) == "\xc3\x02");
	assert (EncodeVarint(16384) == "\x80\x80\x01");
	assert (EncodeZigzag(4) == "\x08");
	assert (EncodeZigzag(64) == "\x80\x01");
	assert (EncodeZigzag(-2) == "\x03");
	assert (EncodeZigzag(-3) == "\x05");
	assert (EncodeZigzag(-65) == "\x81\x01");
}

void ReadExactLength(std::istream &str, char *out, size_t len)
{
	size_t total = 0;
	while(total < len)
	{
		str.read(&out[total], len-total);
		total += str.gcount();
		//std::cout << "Read " << total << " of " << len << std::endl;
		if(str.fail())
			throw std::runtime_error("Input underflow");
	}
}

MetaData::MetaData()
{
	version=0;
	timestamp=0;
	changeset=0;
	uid=0;
	visible=true;
	current=true;
}

MetaData::~MetaData()
{

}

MetaData::MetaData( const MetaData &obj)
{
	*this = obj;
}

MetaData& MetaData::operator=(const MetaData &a)
{
	version = a.version;
	timestamp = a.timestamp;
	changeset = a.changeset;
	uid = a.uid;
	username = a.username;
	visible = a.visible;
	current = a.current;
	return *this;
}

void PrintTagMap(const TagMap &tagMap)
{
	for(TagMap::const_iterator it = tagMap.begin(); it != tagMap.end(); it++)
	{
		std::cout << it->first << "=" << it->second << std::endl;
	}
}

// ****** o5m decoder ******

O5mDecode::O5mDecode(std::streambuf &handleIn) : 
	handle(&handleIn),
	refTableLengthThreshold(250),
	refTableMaxSize(15000),
	output(NULL)
{
	finished = false;

	if(handle.fail())
		throw std::runtime_error("Stream handle indicating failure in o5m decode");

	this->stringPairs.SetBufferSize(this->refTableMaxSize);
	char tmp = handle.get();
	if(handle.fail())
		throw std::runtime_error("Error reading buffer to get o5m magic number");
	std::string tmp2(&tmp, 1);
	if(tmp2 != "\xff")
		throw std::runtime_error("First byte has wrong value");

	tmp = handle.get();
	if(handle.fail())
		throw std::runtime_error("Error reading buffer to get o5m header");
	std::string tmp3(&tmp, 1);
	if(tmp3 != "\xe0")
		throw std::runtime_error("Missing header");

	this->ResetDeltaCoding();
}

O5mDecode::~O5mDecode()
{
	if(!this->finished)
		this->DecodeFinish();
}

void O5mDecode::ResetDeltaCoding()
{
	this->lastObjId = 0; //Used in delta encoding
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
	if(finished)
		throw runtime_error("Decode already finished");

	unsigned char code = this->handle.get();
	if(this->handle.fail())
		throw std::runtime_error("Error reading type code");

	//std::cout << "found code " << (unsigned int)code << std::endl;
	switch(code)
	{
	case 0x10:
		this->DecodeNode();
		return true;
		break;
	case 0x11:
		this->DecodeWay();
		return true;
		break;
	case 0x12:
		this->DecodeRelation();
		return true;
		break;
	case 0xdb:
		this->DecodeBoundingBox();
		return true;
		break;
	case 0xff:
		//Used in delta encoding information
		this->ResetDeltaCoding();
		return true; //Reset code
		break;
	case 0xfe:
		return true; //End of file
		break;
	}
	if(code >= 0xF0 && code <= 0xFF)
		return false;

	//Default behavior to skip unknown data
	uint64_t length = DecodeVarint(this->handle);
	tmpBuff.resize(length);
	ReadExactLength(this->handle, &tmpBuff[0], length);
	return true;
}

void O5mDecode::DecodeHeader()
{
	if(finished)
		throw runtime_error("Decode already finished");

	uint64_t length = DecodeVarint(this->handle);
	std::string fileType;
	fileType.resize(length);
	ReadExactLength(this->handle, &fileType[0], length);
	if(this->output != NULL)
		this->output->StoreIsDiff("o5c2"==fileType);
}

void O5mDecode::DecodeBoundingBox()
{
	DecodeVarint(this->handle); //Value discarded

	//south-western corner 
	double x1 = DecodeZigzag(this->handle) / 1e7; //lon
	double y1 = DecodeZigzag(this->handle) / 1e7; //lat

	//north-eastern corner
	double x2 = DecodeZigzag(this->handle) / 1e7; //lon
	double y2 = DecodeZigzag(this->handle) / 1e7; //lat

	if(this->output != NULL)
		this->output->StoreBounds(x1, y1, x2, y2);
}

void O5mDecode::DecodeSingleString(std::istream &stream, std::string &out)
{
	char tmp[] = "a";
	out = "";
	int code = 0x01;
	while((char)code != 0x00)
	{
		code = stream.get();
		if(code == std::char_traits<char>::eof())
			throw std::runtime_error("End of file while reading string");
		if(stream.fail())
		{
			throw std::runtime_error("Error reading string");
		}
		if ((char)code != 0x00)
		{
			tmp[0] = (char)code;
			out.append(tmp, 1);
		}
	}
}

void O5mDecode::ConsiderAddToStringRefTable(const std::string &firstStr, const std::string &secondStr)
{
	//Consider adding pair to string reference table
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
	{
		this->stringPairs.PopFront();
	}

	this->stringPairs.PushBack(buff);
}

bool O5mDecode::ReadStringPair(std::istream &stream, std::string &firstStr, std::string &secondStr)
{
	uint64_t ref = 0;
	try {
		ref = DecodeVarint(stream);
	}
	catch (std::runtime_error &err)
	{
		if(stream.eof()) {
			firstStr = "";
			secondStr = "";
			return false;
		}
		throw err;
	}
	if(ref == 0x00)
	{
		//Found new pair of strings
		this->DecodeSingleString(stream, firstStr);
		this->DecodeSingleString(stream, secondStr);
		this->ConsiderAddToStringRefTable(firstStr, secondStr);
	}
	else
	{
		int64_t offset = this->stringPairs.Size()-ref;
		if(offset < 0 || offset >= (int64_t)this->stringPairs.Size())
		{
			stringstream ss;
			ss << "o5m reference " << offset << " out of range (should be in range 0-"<< (this->stringPairs.Size()-1) << ")";
			throw std::runtime_error(ss.str());
		}
		const std::string &prevPair = this->stringPairs[offset];
		std::istringstream ss(prevPair);
		this->DecodeSingleString(ss, firstStr);
		this->DecodeSingleString(ss, secondStr);
	}
	return true;
}

void O5mDecode::DecodeMetaData(std::istream &nodeDataStream, class MetaData &out)
{
	//Decode author and time stamp
	out.version = DecodeVarint(nodeDataStream);
	out.timestamp = 0;
	out.changeset = 0;
	out.uid = 0;
	std::string uidStr;
	out.username="";
	if(out.version != 0)
	{
		int64_t deltaTime = DecodeZigzag(nodeDataStream);
		this->lastTimeStamp += deltaTime;
		out.timestamp = this->lastTimeStamp;
		//print "timestamp", self.lastTimeStamp, deltaTime
		if(out.timestamp != 0)
		{
			int64_t deltaChangeSet = DecodeZigzag(nodeDataStream);
			this->lastChangeSet += deltaChangeSet;
			out.changeset = this->lastChangeSet;
			//print "changeset", self.lastChangeSet, deltaChangeSet

			this->ReadStringPair(nodeDataStream, uidStr, out.username);
			if (uidStr.size() > 0)
				out.uid = DecodeVarint(uidStr.c_str());
		}
	}
}

void O5mDecode::DecodeNode()
{
	uint64_t length = DecodeVarint(this->handle);
	std::string &nodeData = tmpBuff;
	nodeData.resize(length);
	ReadExactLength(this->handle, &nodeData[0], length);

	//Decode object ID
	std::istringstream nodeDataStream(nodeData);
	int64_t deltaId = DecodeZigzag(nodeDataStream);
	this->lastObjId += deltaId;
	int64_t objectId = this->lastObjId; 

	this->DecodeMetaData(nodeDataStream, this->tmpMetaData);

	this->lastLon += DecodeZigzag(nodeDataStream);
	this->lastLat += DecodeZigzag(nodeDataStream);
	double lon = this->lastLon / 1e7;
	double lat = this->lastLat / 1e7;

	//Extract tags
	std::string firstString, secondString;
	this->tmpTagsBuff.clear();
	while(!nodeDataStream.eof())
	{
		bool ok = this->ReadStringPair(nodeDataStream, firstString, secondString);
		if(ok) this->tmpTagsBuff[firstString] = secondString;
	}

	if(this->output != NULL)
		this->output->StoreNode(objectId, this->tmpMetaData, this->tmpTagsBuff, lat, lon);
}

void O5mDecode::DecodeWay()
{
	uint64_t length = DecodeVarint(this->handle);
	std::string &objData = tmpBuff;
	objData.resize(length);
	ReadExactLength(this->handle, &objData[0], length);

	//Decode object ID
	std::istringstream objDataStream(objData);
	int64_t deltaId = DecodeZigzag(objDataStream);
	this->lastObjId += deltaId;
	int64_t objectId = this->lastObjId;
	//print "objectId", objectId

	this->DecodeMetaData(objDataStream, this->tmpMetaData);

	uint64_t refLen = DecodeVarint(objDataStream);
	//print "len ref", refLen

	std::string refData;
	refData.resize(refLen);
	objDataStream.read(&refData[0], refLen);
	std::istringstream refDataStream(refData);
	this->tmpRefsBuff.clear();
	while(!refDataStream.eof())
	{
		try {
			this->lastRefNode += DecodeZigzag(refDataStream);
		}
		catch (std::runtime_error &err)
		{
			if(refDataStream.eof())
				continue;
			throw err;
		}
		this->tmpRefsBuff.push_back(this->lastRefNode);
	}

	//Extract tags
	std::string firstString, secondString;
	this->tmpTagsBuff.clear();
	while(!objDataStream.eof())
	{
		bool ok = this->ReadStringPair(objDataStream, firstString, secondString);
		if(ok) this->tmpTagsBuff[firstString] = secondString;
	}

	if (this->output != NULL)
		this->output->StoreWay(objectId, this->tmpMetaData, this->tmpTagsBuff, this->tmpRefsBuff);
}

void O5mDecode::DecodeRelation()
{
	uint64_t length = DecodeVarint(this->handle);
	std::string &objData = tmpBuff;
	objData.resize(length);
	ReadExactLength(this->handle, &objData[0], length);

	//Decode object ID
	std::istringstream objDataStream(objData);
	int64_t deltaId = DecodeZigzag(objDataStream);
	this->lastObjId += deltaId;
	int64_t objectId = this->lastObjId;
	//print "objectId", objectId

	this->DecodeMetaData(objDataStream, this->tmpMetaData);

	uint64_t refLen = DecodeVarint(objDataStream);
	//print "len ref", refLen

	std::string refData;
	refData.resize(refLen);
	objDataStream.read(&refData[0], refLen);
	std::istringstream refDataStream(refData);

	this->tmpRefsBuff.clear();
	this->tmpRefRolesBuff.clear();
	this->tmpRefTypeStrBuff.clear();

	while (!refDataStream.eof())
	{
		int64_t deltaRef = 0;
		try {
			deltaRef += DecodeZigzag(refDataStream);
		}
		catch (std::runtime_error &err)
		{
			if(refDataStream.eof())
				continue;
			throw err;
		}

		uint64_t refIndex = DecodeVarint(refDataStream); //Index into reference table
		std::string typeAndRole;
		if(refIndex == 0)
		{
			this->DecodeSingleString(refDataStream, typeAndRole);
			if(typeAndRole.size() <= this->refTableLengthThreshold)
				this->AddBuffToStringRefTable(typeAndRole);
		}
		else
		{
			int64_t offset = this->stringPairs.Size()-refIndex;
			if(offset < 0 || offset >= (int64_t)this->stringPairs.Size())
			{
				stringstream ss;
				ss << "o5m reference " << offset << " out of range (should be in range 0-"<< (this->stringPairs.Size()-1) << ")";
				throw std::runtime_error(ss.str());
			}
			typeAndRole = this->stringPairs[offset];
		}

		char typeCodeStr[] = "a";
		typeCodeStr[0] = typeAndRole[0];
		int typeCode = atoi(typeCodeStr);
		std::string role(&typeAndRole[1], typeAndRole.size()-1);
		int64_t refId = 0;
		std::string typeStr;
		switch(typeCode)
		{
		case 0:
			this->lastRefNode += deltaRef;
			refId = this->lastRefNode;
			typeStr = "node";
			break;
		case 1:
			this->lastRefWay += deltaRef;
			refId = this->lastRefWay;
			typeStr = "way";
			break;
		case 2:
			this->lastRefRelation += deltaRef;
			refId = this->lastRefRelation;
			typeStr = "relation";
			break;
		}

		this->tmpRefsBuff.push_back(refId);
		this->tmpRefRolesBuff.push_back(role);
		this->tmpRefTypeStrBuff.push_back(typeStr);
	}

	//Extract tags
	std::string firstString, secondString;
	this->tmpTagsBuff.clear();
	while(!objDataStream.eof())
	{
		bool ok = this->ReadStringPair(objDataStream, firstString, secondString);
		if(ok) this->tmpTagsBuff[firstString] = secondString;
	}

	if(this->output != NULL)
		this->output->StoreRelation(objectId, this->tmpMetaData, this->tmpTagsBuff, 
			this->tmpRefTypeStrBuff, this->tmpRefsBuff, this->tmpRefRolesBuff);

}

void O5mDecode::DecodeFinish()
{
	if(finished)
		throw runtime_error("Decode already finished");
	if(this->output != nullptr)
		this->output->Finish();
	finished = true;
}

// ************** o5m encoder ****************
O5mEncodeBase::O5mEncodeBase():	refTableLengthThreshold(250),
	refTableMaxSize(15000),
	runningRefOffset(0)
{

}

O5mEncodeBase::~O5mEncodeBase()
{

}

void O5mEncodeBase::WriteStart()
{
	this->stringPairs.SetBufferSize(this->refTableMaxSize);
	this->write("\xff", 1);
	this->ResetDeltaCoding();
}

void O5mEncodeBase::ResetDeltaCoding()
{
	this->lastObjId = 0;
	this->lastTimeStamp = 0;
	this->lastChangeSet = 0;
	this->stringPairs.Clear();
	this->stringPairsDict.clear();
	this->runningRefOffset = 0;
	this->lastLat = 0.0;
	this->lastLon = 0.0;
	this->lastRefNode = 0;
	this->lastRefWay = 0;
	this->lastRefRelation = 0;
}

bool O5mEncodeBase::StoreIsDiff(bool isDiff)
{
	this->write("\xe0", 1);
	std::string headerData;
	if(isDiff)
		headerData = "o5c2";
	else
		headerData = "o5m2";
	std::string len = EncodeVarint(headerData.size());
	*this << len;
	*this << headerData;
	return false;
}

bool O5mEncodeBase::StoreBounds(double x1, double y1, double x2, double y2)
{
	//south-western corner 
	std::string bboxData;
	bboxData.append(EncodeZigzag(round(x1 * 1e7))); //lon
	bboxData.append(EncodeZigzag(round(y1 * 1e7))); //lat

	//north-eastern corner
	bboxData.append(EncodeZigzag(round(x2 * 1e7))); //lon
	bboxData.append(EncodeZigzag(round(y2 * 1e7))); //lat
	
	this->write("\xdb", 1);
	std::string len = EncodeVarint(bboxData.size());
	*this << len;
	*this << bboxData;
	return false;
}

void O5mEncodeBase::EncodeMetaData(const class MetaData &metaData, std::ostream &outStream)
{
	//Decode author and time stamp
	if(metaData.version != 0)
	{
		std::string verStr = EncodeVarint(metaData.version);
		outStream << verStr;
		int64_t deltaTime = metaData.timestamp - this->lastTimeStamp;
		outStream << EncodeZigzag(deltaTime);
		this->lastTimeStamp = metaData.timestamp;
		//print "timestamp", self.lastTimeStamp, deltaTime
		if(metaData.timestamp != 0)
		{
			//print changeset
			int64_t deltaChangeSet = metaData.changeset - this->lastChangeSet;
			outStream << EncodeZigzag(deltaChangeSet);
			this->lastChangeSet = metaData.changeset;
			if (metaData.uid != 0)
			{
				std::string encUid = EncodeVarint(metaData.uid);
				this->WriteStringPair(encUid, metaData.username, outStream);
			}
			else
			{
				this->WriteStringPair("", metaData.username, outStream);
			}
		}
	}
	else
	{
		outStream << EncodeVarint(0);
	}
}

size_t O5mEncodeBase::FindStringPairsIndex(std::string needle, bool &indexFound)
{
	map<std::string, int>::iterator it = this->stringPairsDict.find(needle);
	if (it == this->stringPairsDict.end())
	{
		indexFound = false;
		return 0;
	}
	indexFound = true;
	return this->runningRefOffset - it->second;
}

void O5mEncodeBase::WriteStringPair(const std::string &firstString, const std::string &secondString, 
	std::ostream &tmpStream)
{
	std::string encodedStrings = firstString;
	encodedStrings.append("\x00",1);
	encodedStrings.append(secondString);
	encodedStrings.append("\x00",1);
	if(firstString.size() + secondString.size() <= this->refTableLengthThreshold)
	{
		bool indexFound = false;
		size_t existIndex = FindStringPairsIndex(encodedStrings, indexFound);
		if(indexFound) {
			tmpStream << EncodeVarint(existIndex);
			return;
		}
	}

	tmpStream.write("\x00", 1);
	tmpStream << encodedStrings;
	if(firstString.size() + secondString.size() <= this->refTableLengthThreshold)
		this->AddToRefTable(encodedStrings);
}

void O5mEncodeBase::AddToRefTable(const std::string &encodedStrings)
{
	size_t refTableSize = this->stringPairs.Size();
	assert(refTableSize == this->stringPairsDict.size());
	size_t availableSpace = refTableMaxSize - refTableSize;

	//Make sure it does not grow forever
	if(availableSpace == 0)
	{
		const string &st = this->stringPairs.PopFront();
		this->stringPairsDict.erase(st);
	}

	this->stringPairs.PushBack(encodedStrings);
	this->stringPairsDict[encodedStrings] = this->runningRefOffset;
	this->runningRefOffset ++;
}

bool O5mEncodeBase::StoreNode(int64_t objId, const class MetaData &metaData, 
		const TagMap &tags, double latIn, double lonIn)
{
	this->write("\x10",1);

	//Object ID
	std::stringstream tmpStream;
	int64_t deltaId = objId - this->lastObjId;
	tmpStream << EncodeZigzag(deltaId);
	this->lastObjId = objId;

	this->EncodeMetaData(metaData, tmpStream);

	//Position
	int64_t lon = round(lonIn * 1e7);
	int64_t deltaLon = lon - this->lastLon;
	tmpStream << EncodeZigzag(deltaLon);
	this->lastLon = lon;
	int64_t lat = round(latIn * 1e7);
	int64_t deltaLat = lat - this->lastLat;
	tmpStream << EncodeZigzag(deltaLat);
	this->lastLat = lat;

	for (TagMap::const_iterator it=tags.begin(); it != tags.end(); it++)
		this->WriteStringPair(it->first, it->second, tmpStream);

	std::string binData = tmpStream.str();
	std::string len = EncodeVarint(binData.size());
	*this << len;
	*this << binData;
	return false;
}

bool O5mEncodeBase::StoreWay(int64_t objId, const class MetaData &metaData, 
		const TagMap &tags, const std::vector<int64_t> &refs)
{
	this->write("\x11", 1);

	//Object ID
	std::stringstream tmpStream;
	int64_t deltaId = objId - this->lastObjId;
	tmpStream << EncodeZigzag(deltaId);
	this->lastObjId = objId;

	//Store meta data
	this->EncodeMetaData(metaData, tmpStream);

	//Store nodes
	std::stringstream refStream;
	for(size_t i=0; i< refs.size(); i++)
	{
		int64_t ref = refs[i]; 
		int64_t deltaRef = ref - this->lastRefNode;
		refStream << EncodeZigzag(deltaRef);
		this->lastRefNode = ref;
	}

	std::string encRefs = refStream.str();
	tmpStream << EncodeVarint(encRefs.size());
	tmpStream << encRefs;

	//Write tags
	for (TagMap::const_iterator it=tags.begin(); it != tags.end(); it++)
		this->WriteStringPair(it->first, it->second, tmpStream);

	std::string binData = tmpStream.str();
	*this << EncodeVarint(binData.size());
	*this << binData;
	return false;
}
	
bool O5mEncodeBase::StoreRelation(int64_t objId, const class MetaData &metaData, const TagMap &tags, 
		const std::vector<std::string> &refTypeStrs, const std::vector<int64_t> &refIds, 
		const std::vector<std::string> &refRoles)
{
	if(refTypeStrs.size() != refIds.size() || refTypeStrs.size() != refRoles.size())
		throw std::invalid_argument("Length of ref vectors must be equal");

	this->write("\x12", 1);

	//Object ID
	std::stringstream tmpStream;
	int64_t deltaId = objId - this->lastObjId;
	tmpStream << EncodeZigzag(deltaId);
	this->lastObjId = objId;

	//Store meta data
	this->EncodeMetaData(metaData, tmpStream);

	//Store referenced children
	std::stringstream refStream;
	for(size_t i=0; i<refTypeStrs.size(); i++)
	{
		const std::string &typeStr = refTypeStrs[i];
		int64_t refId = refIds[i];
		const std::string &role = refRoles[i];
		char typeCode[2] = "0";
		int64_t deltaRef = 0;
		if(typeStr == "node")
		{
			typeCode[0] = '0';
			deltaRef = refId - this->lastRefNode;
			this->lastRefNode = refId;
		}
		if(typeStr == "way")
		{
			typeCode[0] = '1';
			deltaRef = refId - this->lastRefWay;
			this->lastRefWay = refId;
		}
		if(typeStr == "relation")
		{
			typeCode[0] = '2';
			deltaRef = refId - this->lastRefRelation;
			this->lastRefRelation = refId;
		}

		refStream << EncodeZigzag(deltaRef);

		std::string typeCodeAndRole(typeCode);
		typeCodeAndRole.append(role);

		bool indexFound = false;
		size_t refIndex = this->FindStringPairsIndex(typeCodeAndRole, indexFound);
		if(indexFound)
		{
			refStream << EncodeVarint(refIndex);
		}
		else
		{
			refStream.write("\x00", 1); //String start byte
			refStream << typeCodeAndRole;
			refStream.write("\x00", 1); //String end byte
			if(typeCodeAndRole.size() <= this->refTableLengthThreshold)
				this->AddToRefTable(typeCodeAndRole);
		}
	}
	
	std::string encRefs = refStream.str();
	tmpStream << EncodeVarint(encRefs.size());
	tmpStream << encRefs;

	//Write tags
	for (TagMap::const_iterator it=tags.begin(); it != tags.end(); it++)
		this->WriteStringPair(it->first, it->second, tmpStream);

	std::string binData = tmpStream.str();
	*this << EncodeVarint(binData.size());
	*this << binData;
	return false;
}

bool O5mEncodeBase::Sync()
{
	this->write("\xee\x07\x00\x00\x00\x00\x00\x00\x00", 9);
	return false;
}

bool O5mEncodeBase::Reset()
{
	this->write("\xff", 1);
	this->ResetDeltaCoding();
	return false;
}

bool O5mEncodeBase::Finish()
{
	this->write("\xfe", 1);
	return false;
}

void O5mEncodeBase::write (const char* s, std::streamsize n)
{

}

void O5mEncodeBase::operator<< (const std::string &val)
{

}

// **** Output specific encoders

O5mEncode::O5mEncode(std::streambuf &handleIn): O5mEncodeBase(), handle(&handleIn)
{
	this->WriteStart();
}

O5mEncode::~O5mEncode()
{

}

#ifdef PYTHON_AWARE
PyO5mEncode::PyO5mEncode(PyObject* obj): O5mEncodeBase()
{
	m_PyObj = obj;
	Py_INCREF(m_PyObj);
	m_Write = PyObject_GetAttrString(m_PyObj, "write");
	this->WriteStart();
}

PyO5mEncode::~PyO5mEncode()
{
	Py_XDECREF(m_Write);
	Py_XDECREF(m_PyObj);
}

void PyO5mEncode::write (const char* s, std::streamsize n)
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

void PyO5mEncode::operator<< (const std::string &val)
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

#endif //PYTHON_AWARE

