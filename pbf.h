#ifndef CPPO5M_PBF_H
#define CPPO5M_PBF_H

#include <cstdint>
#include <istream>
#include <memory>
#include <string>
#include <vector>
#include "decoder.h"
#include "encoder.h"
#include "model.h"

///Decodes an OSM PBF stream. Each call to DecodeNext handles one blob.
class PbfDecode : public OsmDecoder
{
private:
	std::istream handle;
	bool anyObject;
	ObjectType prevObjType;

	void DecodeOsmData(const std::string &decBlob);
	void CheckOutputType(ObjectType objType);

public:
	PbfDecode(std::streambuf &input, IDataStreamHandler &output);

	bool DecodeNext() override;
};

///Encodes a stream of map objects as OSM PBF. Objects are collected into
///blocks of one type, so output appears when the object type changes, when a
///block fills, and on Finish.
class PbfEncode : public OsmEncoder
{
private:
	OsmData buffer;
	bool anyObject;
	ObjectType prevObjType;
	bool headerWritten;
	uint32_t maxPayloadSize, maxHeaderSize;

	void BeforeStore(ObjectType objType, size_t buffered);
	void EncodeBuffer();
	void EncodeHeaderBlock(std::string &out);
	void EncodePbfDenseNodes(const std::vector<OsmNode> &nodes, size_t &nodec, size_t maxNodesToProcess,
		std::string &out);
	void EncodePbfWays(const std::vector<OsmWay> &ways, size_t &wayc, size_t maxWaysToProcess,
		std::string &out);
	void EncodePbfRelations(const std::vector<OsmRelation> &relations, size_t &relationc, size_t maxRelsToProcess,
		std::string &out);
	void EncodePbfDenseNodesSizeLimited(const std::vector<OsmNode> &nodes, size_t &nodec, std::string &out);
	void EncodePbfWaysSizeLimited(const std::vector<OsmWay> &ways, size_t &wayc, std::string &out);
	void EncodePbfRelationsSizeLimited(const std::vector<OsmRelation> &relations, size_t &relc, std::string &out);
	void WriteBlobPayload(const std::string &blobPayload, const char *type);

public:
	explicit PbfEncode(std::shared_ptr<ByteSink> sink);
	///Writes to a stream buffer, which must outlive the encoder.
	explicit PbfEncode(std::streambuf &output);

	void StoreBounds(const Bounds &bounds) override;
	void StoreNode(const OsmNode &node) override;
	void StoreWay(const OsmWay &way) override;
	void StoreRelation(const OsmRelation &relation) override;
	void Finish() override;

	//Settings; change them before the first object is stored
	bool encodeMetaData, encodeHistorical, compressUsingZLib;
	///Objects per block. The format recommends no more than 8000, and this is
	///also how many objects the encoder holds in memory at once.
	size_t maxBlockObjects;
	std::string writingProgram;
	int32_t granularity, date_granularity;
	int64_t lat_offset, lon_offset;
};

#endif //CPPO5M_PBF_H
