#ifndef CPPO5M_HANDLER_H
#define CPPO5M_HANDLER_H

#include <map>
#include <string>

struct Bounds;
class OsmNode;
class OsmWay;
class OsmRelation;
struct OsmChangeBlock;

///Receives a stream of map objects. Decoders call it, encoders and filters
///implement it, and OsmData both implements it and can replay itself into one.
///Every method has an empty default, so a handler overrides only what it needs.
///A handler that cannot continue throws; the exception reaches whoever is
///driving the stream.
class IDataStreamHandler
{
public:
	virtual ~IDataStreamHandler() {}

	///Whether the stream is a diff (o5c) rather than a snapshot. Sent before any object.
	virtual void StoreIsDiff(bool isDiff) {}
	///Attributes of the document as a whole that are not part of the map, such
	///as the edit IDs a dump was taken at. Sent before any object, and only if
	///the document has some. The version and generator are never included.
	virtual void StoreAttributes(const std::map<std::string, std::string> &attribs) {}
	virtual void StoreBounds(const Bounds &bounds) {}
	virtual void StoreNode(const OsmNode &node) {}
	virtual void StoreWay(const OsmWay &way) {}
	virtual void StoreRelation(const OsmRelation &relation) {}

	///Marks a boundary in the stream, normally a change of object type. The
	///o5m encoder restarts its delta coding here; most handlers ignore it.
	virtual void Reset() {}

	///The stream is complete. Sent exactly once, after the last object.
	virtual void Finish() {}
};

///Receives the action blocks of an osmChange document in order.
class IOsmChangeHandler
{
public:
	virtual ~IOsmChangeHandler() {}

	virtual void StoreChangeBlock(const OsmChangeBlock &block) {}
	virtual void Finish() {}
};

#endif //CPPO5M_HANDLER_H
