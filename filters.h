#ifndef CPPO5M_FILTERS_H
#define CPPO5M_FILTERS_H

#include <set>
#include "model.h"

///Captures the bounds at the start of a stream without reading the rest.
///Bounds come before any object, so stop decoding once done is set:
///
///    FindBbox finder;
///    O5mDecode decoder(input, finder);
///    while(!finder.done && decoder.DecodeNext()) {}
class FindBbox : public IDataStreamHandler
{
public:
	bool bboxFound = false;
	Bounds bounds; ///<The first bounds in the stream, if bboxFound
	bool done = false; ///<True once bounds were found or cannot follow

	void StoreBounds(const Bounds &bounds) override;
	void StoreNode(const OsmNode &node) override;
	void StoreWay(const OsmWay &way) override;
	void StoreRelation(const OsmRelation &relation) override;
	void Finish() override;
};

///Passes a stream through, dropping any object whose type and ID have already
///been seen. The IDs seen are held in memory.
class DeduplicateOsm : public IDataStreamHandler
{
private:
	IDataStreamHandler &out;
	std::set<int64_t> nodeIds, wayIds, relationIds;

public:
	///out must outlive the filter.
	explicit DeduplicateOsm(IDataStreamHandler &out);

	///Forgets the IDs seen so far.
	void ResetExisting();

	void StoreIsDiff(bool isDiff) override;
	void StoreAttributes(const TagMap &attribs) override;
	void StoreBounds(const Bounds &bounds) override;
	void StoreNode(const OsmNode &node) override;
	void StoreWay(const OsmWay &way) override;
	void StoreRelation(const OsmRelation &relation) override;
	void Reset() override;
	void Finish() override;
};

///Collects a whole stream, then on Finish sends it on with nodes, ways and
///relations each sorted by ID and then version. The stream is held in memory.
class SortOsm : public OsmData
{
private:
	IDataStreamHandler &out;

public:
	///out must outlive the filter.
	explicit SortOsm(IDataStreamHandler &out);

	void Finish() override;
};

#endif //CPPO5M_FILTERS_H
