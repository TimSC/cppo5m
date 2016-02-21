
#include <assert.h>
#include <stdexcept>
#include <iostream>
#include <stdlib.h>
#include "varint.h"
#include "o5m.h"

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

MetaData::MetaData()
{
	version=0;
	timestamp=0;
	changeset=0;
	uid=0;
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
	return *this;
}

// ****** o5m decoder ******

O5mDecode::O5mDecode(std::istream &handleIn) : handle(handleIn),
	funcStoreNode(NULL),
	funcStoreWay(NULL),
	funcStoreRelation(NULL),
	funcStoreBounds(NULL),
	funcStoreIsDiff(NULL),
	refTableLengthThreshold(250),
	refTableMaxSize(15000),
	userData(NULL)
{
	char tmp = handle.get();
	if(handle.fail())
		throw std::runtime_error("Error reading input");
	std::string tmp2(&tmp, 1);
	if(tmp2 != "\xff")
		throw std::runtime_error("First byte has wrong value");

	tmp = handle.get();
	if(handle.fail())
		throw std::runtime_error("Error reading input");
	std::string tmp3(&tmp, 1);
	if(tmp3 != "\xe0")
		throw std::runtime_error("Missing header");

	this->ResetDeltaCoding();
}

O5mDecode::~O5mDecode()
{

}

void O5mDecode::ResetDeltaCoding()
{
	this->lastObjId = 0; //Used in delta encoding
	this->lastTimeStamp = 0;
	this->lastChangeSet = 0;
	this->stringPairs.clear();
	this->lastLat = 0;
	this->lastLon = 0;
	this->lastRefNode = 0;
	this->lastRefWay = 0;
	this->lastRefRelation = 0;
}

bool O5mDecode::DecodeNext()
{
	unsigned char code = this->handle.get();
	//std::cout << "found " << (unsigned int)code << std::endl;
	switch(code)
	{
	case 0x10:
		this->DecodeNode();
		return false;
		break;
	case 0x11:
		this->DecodeWay();
		return false;
		break;
	case 0x12:
		this->DecodeRelation();
		return false;
		break;
	case 0xdb:
		this->DecodeBoundingBox();
		return false;
		break;
	case 0xff:
		//Used in delta encoding information
		this->ResetDeltaCoding();
		return false; //Reset code
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
	this->handle.read(&tmpBuff[0], length);
	return false;
}

void O5mDecode::DecodeHeader()
{
	uint64_t length = DecodeVarint(this->handle);
	std::string fileType;
	fileType.resize(length);
	this->handle.read(&fileType[0], length);
	if(this->funcStoreIsDiff != NULL)
		this->funcStoreIsDiff("o5c2"==fileType, this->userData);
}

void O5mDecode::DecodeBoundingBox()
{
	uint64_t length = DecodeVarint(this->handle);
	
	//south-western corner 
	double x1 = DecodeZigzag(this->handle) / 1e7; //lon
	double y1 = DecodeZigzag(this->handle) / 1e7; //lat

	//north-eastern corner
	double x2 = DecodeZigzag(this->handle) / 1e7; //lon
	double y2 = DecodeZigzag(this->handle) / 1e7; //lat

	if(this->funcStoreBounds != NULL)
		this->funcStoreBounds(x1, y1, x2, y2, this->userData);
}

void O5mDecode::DecodeSingleString(std::istream &stream, std::string &out)
{
	char tmp[] = "a";
	out = "";
	char code = 0x01;
	while(code != 0x00 && !stream.eof())
	{
		code = stream.get();
		if (code != 0x00)
		{
			tmp[0] = code;
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
	this->stringPairs.push_back(buff);

	//Make sure it does not grow forever
	while(this->stringPairs.size() > this->refTableMaxSize)
		this->stringPairs.pop_front();
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
		//print "new pair"
		this->DecodeSingleString(stream, firstStr);
		this->DecodeSingleString(stream, secondStr);
		this->ConsiderAddToStringRefTable(firstStr, secondStr);
	}
	else
	{
		int64_t offset = this->stringPairs.size()-ref;
		if(offset < 0 || offset >= this->stringPairs.size())
			throw std::runtime_error("o5m reference out of range");
		std::string &prevPair = this->stringPairs[offset];
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
			out.uid = atoi(uidStr.c_str());
		}
	}
}

void O5mDecode::DecodeNode()
{
	uint64_t length = DecodeVarint(this->handle);
	std::string &nodeData = tmpBuff;
	nodeData.resize(length);
	this->handle.read(&nodeData[0], length);

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

	if(this->funcStoreNode != NULL)
		this->funcStoreNode(objectId, this->tmpMetaData, this->tmpTagsBuff, lat, lon, this->userData);
}

void O5mDecode::DecodeWay()
{
	uint64_t length = DecodeVarint(this->handle);
	std::string &objData = tmpBuff;
	objData.resize(length);
	this->handle.read(&objData[0], length);

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

	if (this->funcStoreWay != NULL)
		this->funcStoreWay(objectId, this->tmpMetaData, this->tmpTagsBuff, this->tmpRefsBuff, this->userData);
}

void O5mDecode::DecodeRelation()
{
	uint64_t length = DecodeVarint(this->handle);
	std::string &objData = tmpBuff;
	objData.resize(length);
	this->handle.read(&objData[0], length);

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
			int64_t offset = this->stringPairs.size()-refIndex;
			if(offset < 0 || offset >= this->stringPairs.size())
				throw std::runtime_error("o5m reference out of range");
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

	if(this->funcStoreRelation != NULL)
		this->funcStoreRelation(objectId, this->tmpMetaData, this->tmpTagsBuff, 
			this->tmpRefTypeStrBuff, this->tmpRefsBuff, this->tmpRefRolesBuff, this->userData);

}

