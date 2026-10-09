#ifndef CPPO5M_O5M_H
#define CPPO5M_O5M_H

#include <cstdint>
#include <istream>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "decoder.h"
#include "encoder.h"
#include "fixeddeque.h"
#include "model.h"

///Decodes an o5m or o5c stream.
///
///The format marks a deleted object by writing nothing but its ID, optionally
///with version and author. Such an object is passed on with visible set to
///false, no position, no tags and no members.
class O5mDecode : public OsmDecoder
{
private:
	std::istream handle;
	bool headerRead;

	//Delta coding state
	int64_t lastObjId;
	int64_t lastTimeStamp;
	int64_t lastChangeSet;
	FixedDeque<std::string> stringPairs;
	int64_t lastLat;
	int64_t lastLon;
	int64_t lastRefNode;
	int64_t lastRefWay;
	int64_t lastRefRelation;
	unsigned refTableLengthThreshold;
	unsigned refTableMaxSize;

	//Buffers reused between objects to avoid reallocating
	std::string tmpBuff;
	std::string combinedRawTmpBuff;
	OsmNode tmpNode;
	OsmWay tmpWay;
	OsmRelation tmpRelation;

	void ResetDeltaCoding();
	void ReadHeader();
	void ReadBlock(std::string &out);
	void DecodeBoundingBox(Bounds &out);
	void DecodeAttributes();
	void DecodeSingleString(std::istream &stream, std::string &out);
	void ConsiderAddToStringRefTable(const std::string &firstStr, const std::string &secondStr);
	void AddBuffToStringRefTable(const std::string &buff);
	void DecodeObjectStart(std::istream &stream, OsmObject &out);
	void DecodeTags(std::istream &stream, TagMap &out);
	bool ReadStringPair(std::istream &stream, std::string &firstStr, std::string &secondStr);
	void DecodeNode();
	void DecodeWay();
	void DecodeRelation();
	bool DecodeBlock();

public:
	///Largest single object accepted, in bytes. Guards against a corrupt
	///length field asking for an enormous allocation.
	size_t maxObjectBytes;

	O5mDecode(std::streambuf &input, IDataStreamHandler &output);

	bool DecodeNext() override;
};

///Encodes a stream of map objects as o5m, or o5c if StoreIsDiff(true) comes first.
///
///An object with visible set to false is written as a delete: its ID and
///metadata only. The format has no other way to mark an object deleted, so its
///position, tags and members are not stored.
///
///o5m has no standard place for attributes of the document as a whole. Any
///given to the constructor are written in a dataset of this library's own
///(type 0xc0), straight after the header. The format tells readers to skip
///datasets they do not know, so other programs read the file as usual, though
///osmconvert prints a warning and drops the dataset if it rewrites the file.
///With no attributes the dataset is not written.
class O5mEncode : public OsmEncoder
{
private:
	int64_t lastObjId;
	int64_t lastTimeStamp;
	int64_t lastChangeSet;
	FixedDeque<std::string> stringPairs;
	std::map<std::string, int64_t> stringPairsDict;
	int64_t lastLat;
	int64_t lastLon;
	int64_t lastRefNode;
	int64_t lastRefWay;
	int64_t lastRefRelation;
	unsigned refTableLengthThreshold;
	unsigned refTableMaxSize;
	int64_t runningRefOffset;
	bool writtenHeader;
	TagMap customAttribs;

	void ResetDeltaCoding();
	void WriteStart(bool isDiff);
	void WriteBlock(char code, const std::string &data);
	void EncodeObjectStart(const OsmObject &obj, std::string &out);
	void EncodeTags(const TagMap &tags, std::string &out);
	void WriteStringPair(const std::string &firstString, const std::string &secondString,
		std::string &out);
	void AddToRefTable(const std::string &encodedStrings);
	bool FindStringPairsIndex(const std::string &needle, size_t &indexOut);

public:
	///customAttribs are written as described above; entries named version or
	///generator, or with an empty value, are skipped.
	explicit O5mEncode(std::shared_ptr<ByteSink> sink, const TagMap &customAttribs = TagMap());
	///Writes to a stream buffer, which must outlive the encoder.
	explicit O5mEncode(std::streambuf &output, const TagMap &customAttribs = TagMap());

	void StoreIsDiff(bool isDiff) override;
	void StoreBounds(const Bounds &bounds) override;
	void StoreNode(const OsmNode &node) override;
	void StoreWay(const OsmWay &way) override;
	void StoreRelation(const OsmRelation &relation) override;
	///Writes a reset marker and restarts delta coding.
	void Reset() override;
	void Finish() override;

	///Writes a sync marker, which lets a reader resynchronise mid-file. Follow
	///it with Reset.
	void Sync();
};

#endif //CPPO5M_O5M_H
