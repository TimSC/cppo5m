
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
	refTableMaxSize(15000)
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
	std::cout << "found " << (unsigned int)code << std::endl;
	switch(code)
	{
	case 0x10:
		this->DecodeNode();
		return false;
	/*if code == 0x11:
		self.DecodeWay()
		return false
	if code == 0x12:
		self.DecodeRelation()
		return false*/
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
		this->funcStoreIsDiff("o5c2"==fileType);
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
		this->funcStoreBounds(x1, y1, x2, y2);
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

	std::string firstString, secondString;
	TagMap tags;
	while(!nodeDataStream.eof())
	{
		bool ok = this->ReadStringPair(nodeDataStream, firstString, secondString);
		if(ok) tags[firstString] = secondString;
	}

	if(this->funcStoreNode != NULL)
		this->funcStoreNode(objectId, this->tmpMetaData, tags, lat, lon);
}

/*
	def DecodeWay(self):
		length = Encoding.DecodeVarint(self.handle)
		objData = self.handle.read(length)

		#Decode object ID
		objDataStream = BytesIO(objData)
		deltaId = Encoding.DecodeZigzag(objDataStream)
		self.lastObjId += deltaId
		objectId = self.lastObjId 
		#print "objectId", objectId

		metaData = self.DecodeMetaData(objDataStream)

		refLen = Encoding.DecodeVarint(objDataStream)
		#print "len ref", refLen

		refStart = objDataStream.tell()
		refs = []
		while objDataStream.tell() < refStart + refLen:
			self.lastRefNode += Encoding.DecodeZigzag(objDataStream)
			refs.append(self.lastRefNode)
			#print "ref", self.lastRefNode

		tags = {}
		while objDataStream.tell() < len(objData):
			firstString, secondString = self.ReadStringPair(objDataStream)
			#print "strlen", len(firstString), len(secondString)
			#print "str", firstString.decode("utf-8"), secondString.decode("utf-8")
			tags[firstString.decode("utf-8")] = secondString.decode("utf-8")
		#print tags

		if self.funcStoreWay is not None:
			self.funcStoreWay(objectId, metaData, tags, refs)

	def DecodeRelation(self):
		length = Encoding.DecodeVarint(self.handle)
		objData = self.handle.read(length)

		#Decode object ID
		objDataStream = BytesIO(objData)
		deltaId = Encoding.DecodeZigzag(objDataStream)
		self.lastObjId += deltaId
		objectId = self.lastObjId 
		#print "objectId", objectId

		metaData = self.DecodeMetaData(objDataStream)

		refLen = Encoding.DecodeVarint(objDataStream)
		#print "len ref", refLen

		refStart = objDataStream.tell()
		refs = []

		while objDataStream.tell() < refStart + refLen:
			deltaRef = Encoding.DecodeZigzag(objDataStream)
			refIndex = Encoding.DecodeVarint(objDataStream) #Index into reference table
			if refIndex == 0:
				typeAndRoleRaw = self.DecodeSingleString(objDataStream)
				typeAndRole = typeAndRoleRaw.decode("utf-8")
				if len(typeAndRoleRaw) <= self.refTableLengthThreshold:
					self.AddBuffToStringRefTable(typeAndRoleRaw)
			else:
				typeAndRole = self.stringPairs[-refIndex].decode("utf-8")

			typeCode = int(typeAndRole[0])
			role = typeAndRole[1:]
			refId = None
			if typeCode == 0:
				self.lastRefNode += deltaRef
				refId = self.lastRefNode
			if typeCode == 1:
				self.lastRefWay += deltaRef
				refId = self.lastRefWay
			if typeCode == 2:
				self.lastRefRelation += deltaRef
				refId = self.lastRefRelation
			typeStr = None
			if typeCode == 0:
				typeStr = "node"
			if typeCode == 1:
				typeStr = "way"
			if typeCode == 2:
				typeStr = "relation"
			refs.append((typeStr, refId, role))
			#print "rref", refId, typeCode, role

		tags = {}
		while objDataStream.tell() < len(objData):
			firstString, secondString = self.ReadStringPair(objDataStream)
			#print "strlen", len(firstString), len(secondString)
			#print "str", firstString.decode("utf-8"), secondString.decode("utf-8")
			tags[firstString.decode("utf-8")] = secondString.decode("utf-8")
		#print tags

		if self.funcStoreRelation is not None:
			self.funcStoreRelation(objectId, metaData, tags, refs)

	*/


/*
# ****** encode o5m ******

class O5mEncode(object):
	def __init__(self, handle):
		self.handle = handle
		self.handle.write(b"\xff")
		
		self.ResetDeltaCoding()
		self.funcStoreNode = None
		self.funcStoreWay = None
		self.funcStoreRelation = None
		self.funcStoreBounds = None
		self.funcStoreIsDiff = None
		self.refTableLengthThreshold = 250
		self.refTableMaxSize = 15000

	def ResetDeltaCoding(self):
		self.lastObjId = 0 #Used in delta encoding
		self.lastTimeStamp = 0
		self.lastChangeSet = 0
		self.stringPairs = []
		self.lastLat = 0
		self.lastLon = 0
		self.lastRefNode = 0
		self.lastRefWay = 0
		self.lastRefRelation = 0

	def StoreIsDiff(self, isDiff):
		self.handle.write(b"\xe0")
		if isDiff:
			headerData = "o5c2".encode("utf-8")
		else:
			headerData = "o5m2".encode("utf-8")
		self.handle.write(Encoding.EncodeVarint(len(headerData)))
		self.handle.write(headerData)

	def StoreBounds(self, bbox):

		#south-western corner 
		bboxData = []
		bboxData.append(Encoding.EncodeZigzag(round(bbox[0] * 1e7))) #lon
		bboxData.append(Encoding.EncodeZigzag(round(bbox[1] * 1e7))) #lat

		#north-eastern corner
		bboxData.append(Encoding.EncodeZigzag(round(bbox[2] * 1e7))) #lon
		bboxData.append(Encoding.EncodeZigzag(round(bbox[3] * 1e7))) #lat
		
		combinedData = b''.join(bboxData)
		self.handle.write(b'\xdb')
		self.handle.write(Encoding.EncodeVarint(len(combinedData)))
		self.handle.write(combinedData)

	def EncodeMetaData(self, version, timestamp, changeset, uid, username, outStream):
		#Decode author and time stamp
		if version != 0 and version != None:
			outStream.write(Encoding.EncodeVarint(version))
			if timestamp != None:
				timestamp = calendar.timegm(timestamp.utctimetuple())
			else:
				timestamp = 0
			deltaTime = timestamp - self.lastTimeStamp
			outStream.write(Encoding.EncodeZigzag(deltaTime))
			self.lastTimeStamp = timestamp
			#print "timestamp", self.lastTimeStamp, deltaTime
			if timestamp != 0:
				#print changeset
				deltaChangeSet = changeset - self.lastChangeSet
				outStream.write(Encoding.EncodeZigzag(deltaChangeSet))
				self.lastChangeSet = changeset
				encUid = b""
				if uid is not None:
					encUid = Encoding.EncodeVarint(uid)
				encUsername = b""
				if username is not None:
					encUsername = username.encode("utf-8")
				self.WriteStringPair(encUid, encUsername, outStream)
		else:
			outStream.write(Encoding.EncodeVarint(0))

	def EncodeSingleString(self, strIn):
		return strIn + b'\x00'

	def WriteStringPair(self, firstString, secondString, tmpStream):
		encodedStrings = firstString + b"\x00" + secondString + b"\x00"
		if len(firstString) + len(secondString) <= self.refTableLengthThreshold:
			try:
				existIndex = self.stringPairs.index(encodedStrings)
				tmpStream.write(Encoding.EncodeVarint(len(self.stringPairs) - existIndex))
				return
			except ValueError:
				pass #Key value pair not currently in reference table

		tmpStream.write(b"\x00")
		tmpStream.write(encodedStrings)
		if len(firstString) + len(secondString) <= self.refTableLengthThreshold:
			self.AddToRefTable(encodedStrings)

	def AddToRefTable(self, encodedStrings):
		self.stringPairs.append(encodedStrings)

		#Limit size of reference table
		if len(self.stringPairs) > self.refTableMaxSize:
			self.stringPairs = self.stringPairs[-self.refTableMaxSize:]

	def StoreNode(self, objectId, metaData, tags, pos):
		self.handle.write(b"\x10")

		#Object ID
		tmpStream = BytesIO()
		deltaId = objectId - self.lastObjId
		tmpStream.write(Encoding.EncodeZigzag(deltaId))
		self.lastObjId = objectId

		version, timestamp, changeset, uid, username = metaData
		self.EncodeMetaData(version, timestamp, changeset, uid, username, tmpStream)

		#Position
		lon = round(pos[1] * 1e7)
		deltaLon = lon - self.lastLon
		tmpStream.write(Encoding.EncodeZigzag(deltaLon))
		self.lastLon = lon
		lat = round(pos[0] * 1e7)
		deltaLat = lat - self.lastLat
		tmpStream.write(Encoding.EncodeZigzag(deltaLat))
		self.lastLat = lat

		for key in tags:
			val = tags[key]
			self.WriteStringPair(key.encode("utf-8"), val.encode("utf-8"), tmpStream)

		binData = tmpStream.getvalue()
		self.handle.write(Encoding.EncodeVarint(len(binData)))
		self.handle.write(binData)

	def StoreWay(self, objectId, metaData, tags, refs):
		self.handle.write(b"\x11")

		#Object ID
		tmpStream = BytesIO()
		deltaId = objectId - self.lastObjId
		tmpStream.write(Encoding.EncodeZigzag(deltaId))
		self.lastObjId = objectId

		#Store meta data
		version, timestamp, changeset, uid, username = metaData
		self.EncodeMetaData(version, timestamp, changeset, uid, username, tmpStream)

		#Store nodes
		refStream = BytesIO()
		for ref in refs:
			deltaRef = ref - self.lastRefNode
			refStream.write(Encoding.EncodeZigzag(deltaRef))
			self.lastRefNode = ref

		encRefs = refStream.getvalue()
		tmpStream.write(Encoding.EncodeVarint(len(encRefs)))
		tmpStream.write(encRefs)

		#Write tags
		for key in tags:
			val = tags[key]
			self.WriteStringPair(key.encode("utf-8"), val.encode("utf-8"), tmpStream)

		binData = tmpStream.getvalue()
		self.handle.write(Encoding.EncodeVarint(len(binData)))
		self.handle.write(binData)
		
	def StoreRelation(self, objectId, metaData, tags, refs):
		self.handle.write(b"\x12")

		#Object ID
		tmpStream = BytesIO()
		deltaId = objectId - self.lastObjId
		tmpStream.write(Encoding.EncodeZigzag(deltaId))
		self.lastObjId = objectId

		#Store meta data
		version, timestamp, changeset, uid, username = metaData
		self.EncodeMetaData(version, timestamp, changeset, uid, username, tmpStream)

		#Store referenced children
		refStream = BytesIO()
		for typeStr, refId, role in refs:
			typeCode = None
			deltaRef = None
			if typeStr == "node":
				typeCode = 0
				deltaRef = refId - self.lastRefNode
				self.lastRefNode = refId
			if typeStr == "way":
				typeCode = 1
				deltaRef = refId - self.lastRefWay
				self.lastRefWay = refId
			if typeStr == "relation":
				typeCode = 2
				deltaRef = refId - self.lastRefRelation
				self.lastRefRelation = refId

			refStream.write(Encoding.EncodeZigzag(deltaRef))

			typeCodeAndRole = (str(typeCode) + role).encode("utf-8")
			try:
				refIndex = self.stringPairs.index(typeCodeAndRole)
				refStream.write(Encoding.EncodeVarint(len(self.stringPairs) - refIndex))
			except ValueError:
				refStream.write(b'\x00') #String start byte
				refStream.write(self.EncodeSingleString(typeCodeAndRole))
				if len(typeCodeAndRole) <= self.refTableLengthThreshold:
					self.AddToRefTable(typeCodeAndRole)

		encRefs = refStream.getvalue()
		tmpStream.write(Encoding.EncodeVarint(len(encRefs)))
		tmpStream.write(encRefs)

		#Write tags
		for key in tags:
			val = tags[key]
			self.WriteStringPair(key.encode("utf-8"), val.encode("utf-8"), tmpStream)

		binData = tmpStream.getvalue()
		self.handle.write(Encoding.EncodeVarint(len(binData)))
		self.handle.write(binData)

	def Sync(self):
		self.handle.write(b"\xee\x07\x00\x00\x00\x00\x00\x00\x00")

	def Reset(self):
		self.handle.write(b"\xff")
		self.ResetDeltaCoding()

	def Finish(self):
		self.handle.write(b"\xfe")
*/



