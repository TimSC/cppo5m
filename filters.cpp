#include "filters.h"
#include <algorithm>
using namespace std;

void FindBbox::StoreBounds(const Bounds &boundsIn)
{
	if(!this->bboxFound)
		this->bounds = boundsIn;
	this->bboxFound = true;
	this->done = true;
}

void FindBbox::StoreNode(const OsmNode &)
{
	this->done = true;
}

void FindBbox::StoreWay(const OsmWay &)
{
	this->done = true;
}

void FindBbox::StoreRelation(const OsmRelation &)
{
	this->done = true;
}

void FindBbox::Finish()
{
	this->done = true;
}

// ******************************************************

DeduplicateOsm::DeduplicateOsm(IDataStreamHandler &out) : out(out)
{

}

void DeduplicateOsm::ResetExisting()
{
	nodeIds.clear();
	wayIds.clear();
	relationIds.clear();
}

void DeduplicateOsm::StoreIsDiff(bool isDiff)
{
	out.StoreIsDiff(isDiff);
}

void DeduplicateOsm::StoreAttributes(const TagMap &attribs)
{
	out.StoreAttributes(attribs);
}

void DeduplicateOsm::StoreBounds(const Bounds &bounds)
{
	out.StoreBounds(bounds);
}

void DeduplicateOsm::StoreNode(const OsmNode &node)
{
	if(nodeIds.insert(node.objId).second)
		out.StoreNode(node);
}

void DeduplicateOsm::StoreWay(const OsmWay &way)
{
	if(wayIds.insert(way.objId).second)
		out.StoreWay(way);
}

void DeduplicateOsm::StoreRelation(const OsmRelation &relation)
{
	if(relationIds.insert(relation.objId).second)
		out.StoreRelation(relation);
}

void DeduplicateOsm::Reset()
{
	out.Reset();
}

void DeduplicateOsm::Finish()
{
	out.Finish();
}

// *******************************************************************

static bool CompareObjs(const OsmObject &a, const OsmObject &b)
{
	if(a.objId != b.objId)
		return a.objId < b.objId;
	return a.metaData.version < b.metaData.version;
}

SortOsm::SortOsm(IDataStreamHandler &out) : out(out)
{

}

void SortOsm::Finish()
{
	std::stable_sort(nodes.begin(), nodes.end(), CompareObjs);
	std::stable_sort(ways.begin(), ways.end(), CompareObjs);
	std::stable_sort(relations.begin(), relations.end(), CompareObjs);
	this->StreamTo(this->out);
	this->Clear();
}
