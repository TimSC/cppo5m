// Tests for cppo5m. Run from the repository root: ./tests/test_cppo5m
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include "../cppo5m.h"
#include "../varint.h"
#include "../pbf/osmformat.pb.h"
#include "../pbf/fileformat.pb.h"
#include <arpa/inet.h>
#include <zlib.h>
using namespace std;

static int checks = 0, failures = 0;
static string currentTest;

#define CHECK(cond) do { checks++; if(!(cond)) { failures++; \
	cerr << "FAIL " << currentTest << " line " << __LINE__ << ": " << #cond << endl; } } while(0)

template <class E> static bool Throws(const function<void()> &fn)
{
	try { fn(); }
	catch(const E &) { return true; }
	catch(...) { return false; }
	return false;
}

///True if fn throws OsmLimitError naming the given limit.
static bool HitsLimit(const string &limit, const function<void()> &fn)
{
	try { fn(); }
	catch(const OsmLimitError &err) { return err.limit == limit && err.actual > err.maximum; }
	catch(...) { return false; }
	return false;
}

static string ReadFile(const string &path)
{
	ifstream in(path, ios::binary);
	if(!in)
		throw runtime_error("Cannot open " + path + "; run the tests from the repository root");
	stringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

// ************* Sample data *************

static MetaData Meta(uint64_t version, int64_t timestamp, int64_t changeset, uint64_t uid, const string &user)
{
	MetaData m;
	m.version = version;
	m.timestamp = timestamp;
	m.changeset = changeset;
	m.uid = uid;
	m.username = user;
	return m;
}

static OsmNode Node(int64_t id, double lat, double lon)
{
	OsmNode n;
	n.objId = id;
	n.lat = lat;
	n.lon = lon;
	n.metaData = Meta(1, 1300000000 + id, 100 + id, 7, "mapper");
	return n;
}

///Data that every format can represent exactly, with awkward values throughout.
static OsmData SampleData()
{
	OsmData data;
	data.bounds.push_back(Bounds(-1.5, 50.25, 1.75, 51.5));

	OsmNode n = Node(1, 50.7, -1.1);
	n.tags["name"] = "Caf\xc3\xa9 <\"Quote\" & 'apostrophe'>";
	n.tags["note"] = "line one\nline two\ttabbed";
	n.tags["empty"] = "";
	data.nodes.push_back(n);
	n = Node(2, -89.9999999, 179.9999999);
	n.metaData = Meta(250, 1700000000, 3000000000LL, 2000000000ULL, "\xe6\x97\xa5\xe6\x9c\xac");
	data.nodes.push_back(n);
	n = Node(3, 0.0, 0.0);
	n.metaData = Meta(2, 1, 1, 1, "");
	data.nodes.push_back(n);
	n = Node(5000000000LL, 12.3456789, -98.7654321);
	data.nodes.push_back(n);
	//IDs need not ascend, and deltas may be negative
	n = Node(4, 50.7000001, -1.1000001);
	n.tags["name"] = "Caf\xc3\xa9 <\"Quote\" & 'apostrophe'>";
	data.nodes.push_back(n);

	OsmWay w;
	w.objId = 10;
	w.metaData = Meta(3, 1400000000, 55, 7, "mapper");
	w.refs = {1, 2, 5000000000LL, 3, 1};
	w.tags["highway"] = "residential";
	data.ways.push_back(w);
	w = OsmWay();
	w.objId = 11;
	w.metaData = Meta(1, 1400000001, 56, 8, "other");
	data.ways.push_back(w); //No nodes or tags

	OsmRelation r;
	r.objId = 20;
	r.metaData = Meta(4, 1500000000, 77, 7, "mapper");
	r.members.push_back(RelationMember(ObjectType::Node, 1, "stop"));
	r.members.push_back(RelationMember(ObjectType::Way, 10, ""));
	r.members.push_back(RelationMember(ObjectType::Relation, 21, "sub area"));
	r.members.push_back(RelationMember(ObjectType::Node, 5000000000LL, "stop"));
	r.members.push_back(RelationMember(ObjectType::Way, 11, "outer"));
	r.tags["type"] = "route";
	data.relations.push_back(r);
	r = OsmRelation();
	r.objId = 21;
	r.metaData = Meta(1, 1500000001, 78, 9, "third");
	data.relations.push_back(r); //No members or tags
	return data;
}

static string Encode(OsmFormat format, const OsmData &data)
{
	auto sink = make_shared<StringSink>();
	auto encoder = MakeEncoder(format, sink);
	data.StreamTo(*encoder);
	return sink->data;
}

static OsmData Decode(OsmFormat format, const string &encoded)
{
	OsmData out;
	istringstream in(encoded);
	auto decoder = MakeDecoder(format, *in.rdbuf(), out);
	decoder->Decode();
	return out;
}

static bool Near(double a, double b)
{
	return fabs(a - b) < 1.5e-7;
}

///Equality allowing for the coordinate precision of the formats.
static bool Equivalent(const OsmData &a, const OsmData &b)
{
	if(a.nodes.size() != b.nodes.size() || a.ways != b.ways || a.relations != b.relations)
		return false;
	if(a.bounds.size() != b.bounds.size())
		return false;
	for(size_t i=0; i<a.bounds.size(); i++)
		if(!Near(a.bounds[i].minLon, b.bounds[i].minLon) || !Near(a.bounds[i].minLat, b.bounds[i].minLat) ||
			!Near(a.bounds[i].maxLon, b.bounds[i].maxLon) || !Near(a.bounds[i].maxLat, b.bounds[i].maxLat))
			return false;
	for(size_t i=0; i<a.nodes.size(); i++)
	{
		const OsmNode &x = a.nodes[i], &y = b.nodes[i];
		if(x.objId != y.objId || x.metaData != y.metaData || x.tags != y.tags ||
			!Near(x.lat, y.lat) || !Near(x.lon, y.lon))
			return false;
	}
	return true;
}

static const OsmFormat allFormats[] = {OsmFormat::O5m, OsmFormat::OsmXml, OsmFormat::Pbf, OsmFormat::OsmJson};

// ************* Tests *************

static void TestVarint()
{
	CHECK(DecodeVarint(string("\x05")) == 5);
	CHECK(DecodeVarint(string("\x7f")) == 127);
	CHECK(DecodeVarint(string("\xc3\x02")) == 323);
	CHECK(DecodeVarint(string("\x80\x80\x01")) == 16384);
	CHECK(DecodeZigzag(string("\x08")) == 4);
	CHECK(DecodeZigzag(string("\x80\x01")) == 64);
	CHECK(DecodeZigzag(string("\x03")) == -2);
	CHECK(DecodeZigzag(string("\x05")) == -3);
	CHECK(DecodeZigzag(string("\x81\x01")) == -65);
	CHECK(EncodeVarint(5) == "\x05");
	CHECK(EncodeVarint(323) == "\xc3\x02");
	CHECK(EncodeVarint(16384) == "\x80\x80\x01");
	CHECK(EncodeZigzag(4) == "\x08");
	CHECK(EncodeZigzag(-2) == "\x03");
	CHECK(EncodeZigzag(-65) == "\x81\x01");
	CHECK(EncodeVarint(0) == string("\x00", 1));

	const uint64_t unsignedValues[] = {0, 1, 127, 128, 16383, 16384, 1ULL << 32, (1ULL << 63) - 1, 1ULL << 63, UINT64_MAX};
	for(uint64_t v : unsignedValues)
		CHECK(DecodeVarint(EncodeVarint(v)) == v);
	const int64_t signedValues[] = {0, 1, -1, 63, -64, 64, -65, INT32_MAX, INT32_MIN, INT64_MAX, INT64_MIN};
	for(int64_t v : signedValues)
		CHECK(DecodeZigzag(EncodeZigzag(v)) == v);

	//Appending leaves existing content alone
	string out = "x";
	AppendVarint(323, out);
	AppendZigzag(-2, out);
	CHECK(out == "x\xc3\x02\x03");

	CHECK(Throws<runtime_error>([]{ DecodeVarint(string("")); }));
	CHECK(Throws<runtime_error>([]{ DecodeVarint(string("\x80\x80")); }));
	CHECK(Throws<runtime_error>([]{ DecodeVarint(string(11, '\x80') + "\x01"); }));
	CHECK(Throws<runtime_error>([]{ DecodeVarint(string(9, '\x80') + "\x02"); })); //65 bits
}

static void TestModel()
{
	CHECK(string(ObjectTypeName(ObjectType::Node)) == "node");
	CHECK(string(ObjectTypeName(ObjectType::Way)) == "way");
	CHECK(string(ObjectTypeName(ObjectType::Relation)) == "relation");
	CHECK(ObjectTypeFromName("relation") == ObjectType::Relation);
	ObjectType type = ObjectType::Way;
	CHECK(!ObjectTypeFromName("Node", type) && type == ObjectType::Way);
	CHECK(Throws<invalid_argument>([]{ ObjectTypeFromName(""); }));

	OsmData data = SampleData();
	CHECK(!data.IsEmpty());
	CHECK(data.GetNodeIds() == (set<int64_t>{1, 2, 3, 4, 5000000000LL}));
	CHECK(data.GetWayIds() == (set<int64_t>{10, 11}));
	CHECK(data.GetRelationIds() == (set<int64_t>{20, 21}));

	//Replaying into another store copies everything, through the base class too
	OsmData copy;
	data.StreamTo(copy);
	CHECK(copy == data);
	OsmData viaObjects;
	const OsmObject &asObject = data.ways[0];
	viaObjects.StoreObject(asObject);
	viaObjects.StoreObject(data.nodes[0]);
	viaObjects.StoreObject(data.relations[0]);
	CHECK(viaObjects.ways.size() == 1 && viaObjects.ways[0] == data.ways[0]);
	CHECK(viaObjects.nodes.size() == 1 && viaObjects.relations.size() == 1);
	CHECK(asObject.Type() == ObjectType::Way);

	copy.nodes[0].tags["extra"] = "1";
	CHECK(copy != data);
	copy.Clear();
	CHECK(copy.IsEmpty() && copy == OsmData());

	//A handler sees the calls in order, with a reset between object types
	struct Recorder : public IDataStreamHandler
	{
		string calls;
		void StoreIsDiff(bool) override { calls += "d"; }
		void StoreBounds(const Bounds &) override { calls += "b"; }
		void StoreNode(const OsmNode &) override { calls += "n"; }
		void StoreWay(const OsmWay &) override { calls += "w"; }
		void StoreRelation(const OsmRelation &) override { calls += "r"; }
		void Reset() override { calls += "|"; }
		void Finish() override { calls += "."; }
	};
	Recorder recorder;
	data.StreamTo(recorder);
	CHECK(recorder.calls == "dbnnnnn|ww|rr.");
	Recorder unfinished;
	data.StreamTo(unfinished, false);
	CHECK(unfinished.calls == "dbnnnnn|ww|rr");
}

static void TestOsmChangeGrouping()
{
	OsmChange change;
	OsmNode created = Node(1, 1, 1);
	OsmNode modified = Node(2, 2, 2);
	modified.metaData.version = 2;
	OsmWay deleted;
	deleted.objId = 3;
	deleted.metaData.version = 3;
	deleted.metaData.visible = false;

	change.StoreObject(created);
	change.StoreObject(Node(5, 1, 1));
	change.StoreObject(modified);
	change.StoreObject(deleted);
	change.StoreObject(deleted, true);
	change.StoreObject(deleted, true);
	change.StoreObject(created, true); //if-unused means nothing for a create
	CHECK(change.blocks.size() == 5);
	if(change.blocks.size() == 5)
	{
		CHECK(change.blocks[0].action == "create" && change.blocks[0].data.nodes.size() == 2);
		CHECK(change.blocks[1].action == "modify" && change.blocks[1].data.nodes.size() == 1);
		CHECK(change.blocks[2].action == "delete" && !change.blocks[2].ifUnused && change.blocks[2].data.ways.size() == 1);
		CHECK(change.blocks[3].action == "delete" && change.blocks[3].ifUnused && change.blocks[3].data.ways.size() == 2);
		CHECK(change.blocks[4].action == "create" && !change.blocks[4].ifUnused);
	}
}

static void TestRoundTrips()
{
	OsmData data = SampleData();
	for(OsmFormat format : allFormats)
	{
		string encoded = Encode(format, data);
		CHECK(encoded.size() > 0);
		OsmData decoded = Decode(format, encoded);
		CHECK(Equivalent(decoded, data));
		//Encoding what was decoded gives the same bytes: nothing drifts
		CHECK(Encode(format, decoded) == encoded);

		//An empty document is still a valid document
		string emptyDoc = Encode(format, OsmData());
		CHECK(emptyDoc.size() > 0);
		CHECK(Decode(format, emptyDoc).IsEmpty());
	}

	//Every format to every other format agrees
	OsmData viaO5m = Decode(OsmFormat::O5m, Encode(OsmFormat::O5m, data));
	for(OsmFormat format : allFormats)
		CHECK(Equivalent(Decode(format, Encode(format, viaO5m)), viaO5m));

	//o5m keeps its diff flag
	OsmData diff = data;
	diff.isDiff = true;
	string o5c = Encode(OsmFormat::O5m, diff);
	CHECK(o5c.substr(0, 7) == string("\xff\xe0\x04o5c2", 7));
	CHECK(Decode(OsmFormat::O5m, o5c).isDiff);
	CHECK(!Decode(OsmFormat::O5m, Encode(OsmFormat::O5m, data)).isDiff);

	//Values beyond 32 bits: o5m and XML carry them, PBF cannot and says so
	OsmData wide;
	OsmNode wideNode = Node(1, 1, 1);
	wideNode.metaData.uid = 5000000000ULL;
	wide.nodes.push_back(wideNode);
	CHECK(Decode(OsmFormat::O5m, Encode(OsmFormat::O5m, wide)) == wide);
	CHECK(Decode(OsmFormat::OsmXml, Encode(OsmFormat::OsmXml, wide)) == wide);
	CHECK(Throws<range_error>([&]{ Encode(OsmFormat::Pbf, wide); }));

	//Deleted objects survive in XML
	OsmData deleted;
	OsmNode gone = Node(9, 1, 2);
	gone.metaData.visible = false;
	gone.metaData.version = 4;
	deleted.nodes.push_back(gone);
	OsmData back = Decode(OsmFormat::OsmXml, Encode(OsmFormat::OsmXml, deleted));
	CHECK(back.nodes.size() == 1 && !back.nodes[0].metaData.visible);
}

static void TestLargeData()
{
	//Enough distinct and repeated strings to fill and recycle the o5m string
	//table, strings too long for it, and enough objects to span several PBF groups
	OsmData data;
	for(int i=0; i<40000; i++)
	{
		OsmNode n = Node(i * 3 + 1, 50 + (i % 1000) * 0.0001, -1 - (i % 777) * 0.0001);
		n.metaData = Meta(1 + i % 5, 1200000000 + i * 7 % 100000, 500 + i / 100, 1 + i % 300, "user" + to_string(i % 300));
		n.tags["ref"] = "value " + to_string(i % 20000);
		if(i % 7 == 0)
			n.tags["highway"] = "bus_stop";
		if(i % 1000 == 0)
			n.tags["description"] = string(300 + i % 50, 'a' + i % 26);
		data.nodes.push_back(n);
	}
	for(int i=0; i<12000; i++)
	{
		OsmWay w;
		w.objId = i + 1;
		w.metaData = Meta(1, 1300000000 + i, 900 + i, 1 + i % 50, "user" + to_string(i % 50));
		for(int j=0; j<2 + i % 9; j++)
			w.refs.push_back(((i * 31 + j * 17) % 40000) * 3 + 1);
		w.tags["name"] = "Street " + to_string(i % 500);
		data.ways.push_back(w);
	}
	for(int i=0; i<9000; i++)
	{
		OsmRelation r;
		r.objId = i + 1;
		r.metaData = Meta(2, 1400000000 + i, 2000 + i, 3, "relater");
		for(int j=0; j<1 + i % 6; j++)
			r.members.push_back(RelationMember((ObjectType)((i + j) % 3), (i * 13 + j) % 12000 + 1,
				j % 2 ? "role" + to_string(i % 40) : ""));
		r.tags["type"] = i % 2 ? "multipolygon" : "route";
		data.relations.push_back(r);
	}

	for(OsmFormat format : allFormats)
	{
		string encoded = Encode(format, data);
		OsmData decoded = Decode(format, encoded);
		CHECK(Equivalent(decoded, data));
		CHECK(Encode(format, decoded) == encoded);
	}
}

static void TestSampleFiles()
{
	string o5m = ReadFile("tests/data/o5mtest.o5m");
	OsmData fromO5m = Decode(OsmFormat::O5m, o5m);
	CHECK(fromO5m.nodes.size() == 982);
	CHECK(fromO5m.ways.size() == 102);
	CHECK(fromO5m.relations.size() == 2);
	//The sample was written by another program. Decoding straight into an
	//encoder reproduces it byte for byte.
	{
		istringstream in(o5m);
		auto sink = make_shared<StringSink>();
		O5mEncode encoder(sink);
		O5mDecode decoder(*in.rdbuf(), encoder);
		decoder.Decode();
		CHECK(sink->data == o5m);
	}
	for(OsmFormat format : allFormats)
		CHECK(Equivalent(Decode(format, Encode(format, fromO5m)), fromO5m));

	OsmData fromXml = Decode(OsmFormat::OsmXml, ReadFile("tests/data/o5mtest.osm"));
	CHECK(fromXml.nodes.size() > 0 && fromXml.ways.size() > 0);
	CHECK(fromXml.bounds.size() == 1);
	for(OsmFormat format : allFormats)
		CHECK(Equivalent(Decode(format, Encode(format, fromXml)), fromXml));

	OsmData relations = Decode(OsmFormat::OsmXml, ReadFile("tests/data/o5mrel.osm"));
	CHECK(relations.relations.size() > 0);
	CHECK(relations.bounds.size() == 2);
	CHECK(Equivalent(Decode(OsmFormat::O5m, Encode(OsmFormat::O5m, relations)), relations));

	CHECK(FormatFromFilename("a/b.o5m") == OsmFormat::O5m);
	CHECK(FormatFromFilename("planet.osm.gz") == OsmFormat::OsmXml);
	CHECK(FormatFromFilename("x.osm.pbf") == OsmFormat::Pbf);
	CHECK(Throws<invalid_argument>([]{ FormatFromFilename("data.txt"); }));
	CHECK(Throws<invalid_argument>([]{ FormatFromFilename(""); }));
}

static void TestDecoderContract()
{
	struct Counter : public IDataStreamHandler
	{
		int objects = 0, finishes = 0;
		void StoreNode(const OsmNode &) override { objects++; }
		void StoreWay(const OsmWay &) override { objects++; }
		void StoreRelation(const OsmRelation &) override { objects++; }
		void Finish() override { finishes++; }
	};

	OsmData data = SampleData();
	for(OsmFormat format : allFormats)
	{
		string encoded = Encode(format, data);
		istringstream in(encoded);
		Counter counter;
		auto decoder = MakeDecoder(format, *in.rdbuf(), counter);
		CHECK(!decoder->IsFinished());
		int calls = 0;
		while(decoder->DecodeNext())
			calls++;
		CHECK(calls > 0);
		CHECK(decoder->IsFinished());
		CHECK(counter.objects == 9);
		//Finish arrives once, however often the decoder is asked for more
		CHECK(!decoder->DecodeNext());
		CHECK(!decoder->DecodeNext());
		CHECK(counter.finishes == 1);

		//Stopping early is allowed and sends no Finish
		istringstream in2(encoded);
		Counter partial;
		{
			auto abandoned = MakeDecoder(format, *in2.rdbuf(), partial);
			abandoned->DecodeNext();
		}
		CHECK(partial.finishes == 0);

		//Exceptions from the handler reach the caller as they are
		struct Refuser : public IDataStreamHandler
		{
			void StoreWay(const OsmWay &) override { throw std::domain_error("no ways"); }
		};
		istringstream in3(encoded);
		Refuser refuser;
		auto refused = MakeDecoder(format, *in3.rdbuf(), refuser);
		CHECK(Throws<std::domain_error>([&]{ refused->Decode(); }));
	}
}

static void TestBadInput()
{
	OsmData data = SampleData();
	for(OsmFormat format : allFormats)
	{
		string encoded = Encode(format, data);
		CHECK(Throws<OsmDecodeError>([&]{ Decode(format, "this is not map data at all"); }));
		//Cut off anywhere, the result is an error or a shorter document, never a crash
		for(size_t len=0; len<encoded.size(); len += 1 + encoded.size() / 400)
		{
			try { Decode(format, encoded.substr(0, len)); }
			catch(const OsmDecodeError &) {}
			checks++;
		}
		//The same for corrupted bytes
		for(size_t pos=0; pos<encoded.size(); pos += 1 + encoded.size() / 400)
		{
			for(unsigned char value : {0x00, 0xff, 0x7f, 0x80})
			{
				string corrupt = encoded;
				corrupt[pos] = (char)value;
				try { Decode(format, corrupt); }
				catch(const OsmDecodeError &) {}
				catch(const std::bad_alloc &) { CHECK(false); }
				checks++;
			}
		}
	}

	CHECK(Throws<OsmDecodeError>([]{ Decode(OsmFormat::O5m, ""); }));
	CHECK(Throws<OsmDecodeError>([]{ Decode(OsmFormat::O5m, string("\xff\xe0\x04o5x2", 7)); }));
	//A length field claiming far more data than could exist
	CHECK(Throws<OsmDecodeError>([]{ Decode(OsmFormat::O5m, string("\xff\xe0\x04o5m2\x10\xff\xff\xff\xff\xff\xff\x7f", 15)); }));
	//A string reference to an entry that was never defined
	CHECK(Throws<OsmDecodeError>([]{ Decode(OsmFormat::O5m, string("\xff\xe0\x04o5m2\x10\x05\x02\x00\x00\x00\x09", 14)); }));
	//PBF header length beyond what the format permits
	CHECK(Throws<OsmDecodeError>([]{ Decode(OsmFormat::Pbf, string("\x7f\xff\xff\xff", 4)); }));
	CHECK(Decode(OsmFormat::Pbf, "").IsEmpty());

	//A relation that cannot be written is refused rather than written wrongly
	OsmData bad;
	OsmRelation relation;
	relation.objId = 1;
	relation.members.push_back(RelationMember(ObjectType::Node, 1, string("a\0b", 3)));
	bad.relations.push_back(relation);
	CHECK(Throws<invalid_argument>([&]{ Encode(OsmFormat::O5m, bad); }));
}

///Wraps a payload as one PBF blob, compressed, claiming rawSize bytes when
///that is not negative.
static string PbfBlob(const string &type, const string &payload, int64_t rawSize)
{
	uLongf len = compressBound(payload.size());
	string compressed(len, '\0');
	compress2((Bytef *)&compressed[0], &len, (const Bytef *)payload.data(), payload.size(), 9);
	compressed.resize(len);

	OSMPBF::Blob blob;
	blob.set_zlib_data(compressed);
	if(rawSize >= 0)
		blob.set_raw_size(rawSize);
	string packedBlob;
	blob.SerializeToString(&packedBlob);

	OSMPBF::BlobHeader header;
	header.set_type(type);
	header.set_datasize(packedBlob.size());
	string packedHeader;
	header.SerializeToString(&packedHeader);

	uint32_t headerSize = htonl(packedHeader.size());
	return string((const char *)&headerSize, 4) + packedHeader + packedBlob;
}

static void TestHostileInput()
{
	//A small PBF blob that inflates far beyond what the format allows is
	//refused part way through, whether or not it admits its size
	string zeros(48 * 1024 * 1024, '\0');
	for(int64_t claimed : {(int64_t)-1, (int64_t)1000, (int64_t)zeros.size()})
	{
		string bomb = PbfBlob("OSMData", zeros, claimed);
		CHECK(bomb.size() < 1024 * 1024);
		CHECK(Throws<OsmDecodeError>([&]{ Decode(OsmFormat::Pbf, bomb); }));
	}
	//A blob must inflate to exactly the size it states
	OSMPBF::HeaderBlock hb;
	hb.add_required_features("OsmSchema-V0.6");
	string header;
	hb.SerializeToString(&header);
	CHECK(Decode(OsmFormat::Pbf, PbfBlob("OSMHeader", header, header.size())).IsEmpty());
	CHECK(Decode(OsmFormat::Pbf, PbfBlob("OSMHeader", header, -1)).IsEmpty());
	CHECK(Throws<OsmDecodeError>([&]{ Decode(OsmFormat::Pbf, PbfBlob("OSMHeader", header, header.size() + 1)); }));
	CHECK(Throws<OsmDecodeError>([&]{ Decode(OsmFormat::Pbf, PbfBlob("OSMHeader", header, header.size() - 1)); }));
	//Features this library does not implement are refused, unknown blob types skipped
	hb.add_required_features("SomethingNew");
	hb.SerializeToString(&header);
	CHECK(Throws<OsmDecodeError>([&]{ Decode(OsmFormat::Pbf, PbfBlob("OSMHeader", header, -1)); }));
	CHECK(Decode(OsmFormat::Pbf, PbfBlob("Mystery", "anything", -1)).IsEmpty());

	//Strings that are not valid UTF-8, or hold characters XML forbids, still
	//give a well formed document; the bad parts become replacement characters
	OsmData odd;
	OsmNode n = Node(1, 1, 2);
	n.tags["bad utf8"] = string("a\xff" "b\xc3" "c\xed\xa0\x80" "d\xc0\xaf", 11);
	n.tags["control"] = string("x\x01y\x1fz");
	n.tags["good"] = "\xf0\x9f\x97\xba \xe2\x82\xac \xc3\xa9";
	n.metaData.username = "user\x80name";
	odd.nodes.push_back(n);
	OsmRelation r;
	r.objId = 2;
	r.members.push_back(RelationMember(ObjectType::Node, 1, "role\xfe"));
	odd.relations.push_back(r);
	OsmData readBack = Decode(OsmFormat::OsmXml, Encode(OsmFormat::OsmXml, odd));
	CHECK(readBack.nodes.size() == 1 && readBack.relations.size() == 1);
	if(readBack.nodes.size() == 1 && readBack.relations.size() == 1)
	{
		const string rep = "\xef\xbf\xbd";
		const TagMap &tags = readBack.nodes[0].tags;
		CHECK(tags.at("bad utf8") == "a" + rep + "b" + rep + "c" + rep + rep + rep + "d" + rep + rep);
		CHECK(tags.at("control") == "x" + rep + "y" + rep + "z");
		CHECK(tags.at("good") == n.tags["good"]);
		CHECK(readBack.nodes[0].metaData.username == "user" + rep + "name");
		CHECK(readBack.relations[0].members[0].role == "role" + rep);
	}
	//o5m and PBF carry the bytes unchanged
	CHECK(Decode(OsmFormat::O5m, Encode(OsmFormat::O5m, odd)) == odd);
	CHECK(Decode(OsmFormat::Pbf, Encode(OsmFormat::Pbf, odd)).nodes[0].tags == n.tags);

	//Extreme IDs: delta coding wraps instead of overflowing
	OsmData extreme;
	for(int64_t id : {INT64_MAX, INT64_MIN, (int64_t)0, INT64_MIN + 1, INT64_MAX - 1})
	{
		OsmNode e = Node(0, 0, 0);
		e.objId = id;
		e.metaData = Meta(1, 1000, id, 1, "u");
		extreme.nodes.push_back(e);
		OsmWay w;
		w.objId = id;
		w.metaData = Meta(1, 1000, 1, 1, "u");
		w.refs = {INT64_MAX, INT64_MIN, 0, INT64_MIN};
		extreme.ways.push_back(w);
		OsmRelation rel;
		rel.objId = id;
		rel.metaData = Meta(1, 1000, 1, 1, "u");
		rel.members.push_back(RelationMember(ObjectType::Way, INT64_MIN, ""));
		rel.members.push_back(RelationMember(ObjectType::Way, INT64_MAX, ""));
		extreme.relations.push_back(rel);
	}
	for(OsmFormat format : allFormats)
		CHECK(Decode(format, Encode(format, extreme)) == extreme);

	//Coordinates that are not numbers are refused on the way in and the way out
	for(const char *value : {"nan", "inf", "-infinity", "1e999"})
	{
		OsmData ignored;
		string doc = string("<osm><node id='1' lat='") + value + "' lon='0'/></osm>";
		CHECK(Throws<OsmDecodeError>([&]{ LoadFromOsmXml(doc, ignored); }));
	}
	OsmData unreal;
	unreal.nodes.push_back(Node(1, std::nan(""), 0));
	CHECK(Throws<invalid_argument>([&]{ Encode(OsmFormat::O5m, unreal); }));
	CHECK(Throws<invalid_argument>([&]{ Encode(OsmFormat::Pbf, unreal); }));
	unreal.nodes[0].lat = 1e300;
	CHECK(Throws<invalid_argument>([&]{ Encode(OsmFormat::O5m, unreal); }));
	CHECK(Throws<invalid_argument>([&]{ Encode(OsmFormat::Pbf, unreal); }));
	CHECK(Encode(OsmFormat::OsmXml, unreal).size() > 300);

	//A timestamp too large to scale for PBF
	OsmData late;
	OsmNode lateNode = Node(1, 0, 0);
	lateNode.metaData.timestamp = INT64_MAX / 10;
	late.nodes.push_back(lateNode);
	CHECK(Throws<range_error>([&]{ Encode(OsmFormat::Pbf, late); }));

	//Timestamps outside the years text can hold are left out of XML and JSON,
	//so the document still reads back; o5m keeps the number
	OsmData ancient;
	OsmNode old = Node(1, 1, 2);
	old.metaData.timestamp = -229000000000LL; //Around the year -5287
	ancient.nodes.push_back(old);
	OsmNode distant = Node(2, 1, 2);
	distant.metaData.timestamp = 400000000000LL; //Around the year 14645
	ancient.nodes.push_back(distant);
	CHECK(Decode(OsmFormat::O5m, Encode(OsmFormat::O5m, ancient)) == ancient);
	for(OsmFormat format : {OsmFormat::OsmXml, OsmFormat::OsmJson})
	{
		string text = Encode(format, ancient);
		CHECK(text.find("timestamp") == string::npos);
		OsmData readBack2 = Decode(format, text);
		CHECK(readBack2.nodes.size() == 2 && readBack2.nodes[0].metaData.timestamp == 0);
	}

	//Root attribute names are checked
	auto sink = make_shared<StringSink>();
	OsmXmlEncode badName(sink, TagMap{{"not a name", "x"}});
	CHECK(Throws<invalid_argument>([&]{ badName.Finish(); }));
}

static void TestPbfBlocks()
{
	//Blocks hold a limited number of objects, however many are written
	OsmData data;
	for(int i=0; i<25000; i++)
		data.nodes.push_back(Node(i + 1, 1 + i * 1e-5, 2 + i * 1e-5));
	for(int i=0; i<300; i++)
	{
		OsmWay w;
		w.objId = i + 1;
		w.metaData = Meta(1, 1000, 1, 1, "u");
		w.refs = {i + 1, i + 2};
		data.ways.push_back(w);
	}

	auto countBlobs = [](const string &encoded) {
		size_t blobs = 0, pos = 0;
		while(pos + 4 <= encoded.size())
		{
			uint32_t headerLen = ntohl(*(const uint32_t *)&encoded[pos]);
			OSMPBF::BlobHeader header;
			header.ParseFromString(encoded.substr(pos + 4, headerLen));
			pos += 4 + headerLen + header.datasize();
			blobs ++;
		}
		return blobs;
	};

	string standard = Encode(OsmFormat::Pbf, data);
	CHECK(countBlobs(standard) == 1 + 4 + 1); //Header, 25000 nodes in 8000s, ways
	CHECK(Equivalent(Decode(OsmFormat::Pbf, standard), data));

	for(size_t limit : {(size_t)1000, (size_t)100000, (size_t)0})
	{
		auto sink = make_shared<StringSink>();
		PbfEncode encoder(sink);
		encoder.maxBlockObjects = limit;
		data.StreamTo(encoder);
		CHECK(Equivalent(Decode(OsmFormat::Pbf, sink->data), data));
		if(limit == 1000)
			CHECK(countBlobs(sink->data) == 1 + 25 + 1);
		if(limit == 100000)
			CHECK(countBlobs(sink->data) == 1 + 1 + 1);
	}

	//Uncompressed output is also readable
	auto raw = make_shared<StringSink>();
	PbfEncode rawEncoder(raw);
	rawEncoder.compressUsingZLib = false;
	data.StreamTo(rawEncoder);
	CHECK(raw->data.size() > standard.size());
	CHECK(Equivalent(Decode(OsmFormat::Pbf, raw->data), data));
}

static void TestXml()
{
	//Text is escaped on the way out and restored on the way in
	OsmData data = SampleData();
	string xml = Encode(OsmFormat::OsmXml, data);
	CHECK(xml.find("&lt;&quot;Quote&quot; &amp; &apos;apostrophe&apos;&gt;") != string::npos);
	CHECK(xml.find("line one&#10;line two&#9;tabbed") != string::npos);
	CHECK(xml.find("<osm version=\"0.6\" generator=\"cppo5m\">") != string::npos);
	CHECK(xml.substr(xml.size() - 6) == "</osm>");

	//Root attributes
	TagMap attribs;
	attribs["generator"] = "test <suite>";
	attribs["copyright"] = "nobody";
	attribs["skipped"] = "";
	ostringstream custom;
	{
		OsmXmlEncode encoder(*custom.rdbuf(), attribs);
		OsmData().StreamTo(encoder);
	}
	CHECK(custom.str().find("<osm version=\"0.6\" generator=\"test &lt;suite&gt;\" copyright=\"nobody\">") != string::npos);
	CHECK(custom.str().find("skipped") == string::npos);

	//Nothing is written until there is something to write
	auto sink = make_shared<StringSink>();
	OsmXmlEncode lazy(sink);
	CHECK(sink->data.empty());
	//The output can be collected in pieces
	lazy.StoreNode(data.nodes[0]);
	string firstPiece = sink->data;
	auto second = make_shared<StringSink>();
	lazy.SetSink(second);
	lazy.StoreNode(data.nodes[1]);
	lazy.Finish();
	OsmData joined = Decode(OsmFormat::OsmXml, firstPiece + second->data);
	CHECK(joined.nodes.size() == 2 && firstPiece.size() > 0 && second->data.size() > 0);

	//A document parsed a byte at a time, or as one string, gives the same result
	OsmData whole;
	LoadFromOsmXml(xml, whole);
	OsmData bytewise;
	OsmXmlParser parser(bytewise);
	for(size_t i=0; i<xml.size(); i++)
		parser.Feed(&xml[i], 1, false);
	CHECK(!parser.IsComplete());
	parser.Feed(nullptr, 0, true);
	CHECK(parser.IsComplete());
	CHECK(bytewise == whole && Equivalent(whole, data));
	CHECK(Throws<OsmDecodeError>([&]{ parser.Feed("x", 1, false); }));

	//Details of what is read
	OsmData parsed;
	LoadFromOsmXml(string("<osm version='0.6'>"
		"<note>ignored</note><meta osm_base='x'><tag k='a' v='b'/></meta>"
		"<bounds minlat='1' minlon='2' maxlat='3' maxlon='4'/>"
		"<node id='-5' lat='1.5' lon='-2.5' version='3' changeset='9' uid='4' user='A &amp; B' "
		"timestamp='2020-01-02T03:04:05Z' visible='false'><tag k='k' v='v'/></node>"
		"<way id='6'><nd ref='-5'/><nd ref='7'/><tag k='w' v='1'/><unknown/></way>"
		"<relation id='8'><member type='way' ref='6' role='outer'/><member type='node' ref='-5'/></relation>"
		"</osm>"), parsed);
	CHECK(parsed.bounds.size() == 1 && parsed.bounds[0] == Bounds(2, 1, 4, 3));
	CHECK(parsed.nodes.size() == 1 && parsed.ways.size() == 1 && parsed.relations.size() == 1);
	if(parsed.nodes.size() == 1 && parsed.ways.size() == 1 && parsed.relations.size() == 1)
	{
		const OsmNode &n = parsed.nodes[0];
		CHECK(n.objId == -5 && n.lat == 1.5 && n.lon == -2.5);
		CHECK(n.metaData.version == 3 && n.metaData.changeset == 9 && n.metaData.uid == 4);
		CHECK(n.metaData.username == "A & B" && !n.metaData.visible);
		CHECK(n.metaData.timestamp == 1577934245);
		CHECK(n.tags == (TagMap{{"k", "v"}}));
		CHECK(parsed.ways[0].refs == (vector<int64_t>{-5, 7}));
		CHECK(parsed.ways[0].tags == (TagMap{{"w", "1"}}));
		CHECK(parsed.ways[0].metaData.visible);
		CHECK(parsed.relations[0].members == (vector<RelationMember>{
			RelationMember(ObjectType::Way, 6, "outer"), RelationMember(ObjectType::Node, -5, "")}));
	}

	//Malformed documents
	const char *bad[] = {
		"",
		"<osm>",
		"<osm><node id='1'></osm>",
		"<osm><node id='1' id='2'/></osm>",
		"<notosm><node id='1'/></notosm>",
		"<osmChange><create><node id='1'/></create></osmChange>",
		"<osm><relation id='1'><member type='area' ref='1'/></relation></osm>",
		"<osm><relation id='1'><member ref='1'/></relation></osm>",
		"<osm><node id='1' timestamp='yesterday'/></osm>",
		"<osm></osm><osm></osm>",
	};
	for(const char *doc : bad)
	{
		OsmData ignored;
		CHECK(Throws<OsmDecodeError>([&]{ LoadFromOsmXml(string(doc), ignored); }));
	}
	//The error names the line
	try
	{
		OsmData ignored;
		LoadFromOsmXml(string("<osm>\n<node id='1'/>\n<<</osm>"), ignored);
		CHECK(false);
	}
	catch(const OsmDecodeError &err)
	{
		CHECK(string(err.what()).find("line 3") != string::npos);
	}
}

static string EncodeJson(const OsmData &data, const TagMap &attribs = TagMap())
{
	auto sink = make_shared<StringSink>();
	OsmJsonEncode encoder(sink, attribs);
	data.StreamTo(encoder);
	return sink->data;
}

static void TestJson()
{
	//An empty document
	CHECK(EncodeJson(OsmData()) == "{\"version\":\"0.6\",\"generator\":\"cppo5m\",\"elements\":[]}");

	//One object of each kind, exactly as written
	OsmData data;
	data.bounds.push_back(Bounds(-1.5, 50.25, 1.75, 51));
	data.bounds.push_back(Bounds(1, 2, 3, 4)); //Only the first can be written
	OsmNode n;
	n.objId = 5;
	n.lat = 50.7;
	n.lon = -1.1000001;
	n.metaData = Meta(3, 1577934245, 9, 4, "A \"B\"");
	n.tags["name"] = "Caf\xc3\xa9\nline\\two";
	n.tags["empty"] = "";
	data.nodes.push_back(n);
	OsmWay w;
	w.objId = 6;
	w.refs = {5, -7};
	data.ways.push_back(w);
	OsmRelation r;
	r.objId = 8;
	r.metaData.visible = false;
	r.metaData.version = 2;
	r.members.push_back(RelationMember(ObjectType::Way, 6, "outer"));
	r.members.push_back(RelationMember(ObjectType::Node, 5, ""));
	data.relations.push_back(r);

	TagMap attribs;
	attribs["generator"] = "test";
	attribs["license"] = "odbl";
	attribs["skipped"] = "";
	string expected =
		"{\"version\":\"0.6\",\"generator\":\"test\",\"license\":\"odbl\","
		"\"bounds\":{\"minlat\":50.25,\"minlon\":-1.5,\"maxlat\":51,\"maxlon\":1.75},\"elements\":[\n"
		"{\"type\":\"node\",\"id\":5,\"lat\":50.7,\"lon\":-1.1000001,\"timestamp\":\"2020-01-02T03:04:05Z\","
		"\"version\":3,\"changeset\":9,\"user\":\"A \\\"B\\\"\",\"uid\":4,"
		"\"tags\":{\"empty\":\"\",\"name\":\"Caf\xc3\xa9\\nline\\\\two\"}},\n"
		"{\"type\":\"way\",\"id\":6,\"nodes\":[5,-7]},\n"
		"{\"type\":\"relation\",\"id\":8,\"version\":2,\"visible\":false,"
		"\"members\":[{\"type\":\"way\",\"ref\":6,\"role\":\"outer\"},{\"type\":\"node\",\"ref\":5,\"role\":\"\"}]}"
		"\n]}";
	CHECK(EncodeJson(data, attribs) == expected);

	//A deleted node has no position; whole numbers and zero are written plainly
	OsmData positions;
	OsmNode gone;
	gone.objId = 1;
	gone.lat = 12;
	gone.lon = 34;
	gone.metaData.visible = false;
	positions.nodes.push_back(gone);
	OsmNode whole;
	whole.objId = 2;
	whole.lat = -0.0;
	whole.lon = 180;
	positions.nodes.push_back(whole);
	string json = EncodeJson(positions);
	CHECK(json.find("{\"type\":\"node\",\"id\":1,\"visible\":false}") != string::npos);
	CHECK(json.find("{\"type\":\"node\",\"id\":2,\"lat\":0,\"lon\":180}") != string::npos);

	//Empty lists are left out, as the OSM API does for deleted ways and relations
	OsmData hollow;
	OsmWay noNodes;
	noNodes.objId = 3;
	noNodes.metaData.visible = false;
	hollow.ways.push_back(noNodes);
	OsmRelation noMembers;
	noMembers.objId = 4;
	hollow.relations.push_back(noMembers);
	json = EncodeJson(hollow);
	CHECK(json.find("{\"type\":\"way\",\"id\":3,\"visible\":false}") != string::npos);
	CHECK(json.find("{\"type\":\"relation\",\"id\":4}") != string::npos);
	OsmData hollowBack;
	LoadFromOsmJson(json, hollowBack);
	CHECK(hollowBack == hollow);

	//Strings: control characters are escaped and invalid UTF-8 replaced
	string out;
	AppendJsonString(string("a\x01" "b\xff" "c\x1f\t\r", 8), out);
	CHECK(out == "\"a\\u0001b\xef\xbf\xbd" "c\\u001f\\t\\r\"");
	out.clear();
	AppendJsonString("\xf0\x9f\x97\xba \xed\xa0\x80", out);
	CHECK(out == "\"\xf0\x9f\x97\xba \xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd\"");

	//Numbers JSON cannot hold are refused
	OsmData unreal;
	unreal.nodes.push_back(Node(1, std::nan(""), 0));
	CHECK(Throws<invalid_argument>([&]{ EncodeJson(unreal); }));

	//Bounds after the first object are left out, and the output can be collected in pieces
	auto first = make_shared<StringSink>();
	OsmJsonEncode pieces(first);
	CHECK(first->data.empty());
	pieces.StoreNode(data.nodes[0]);
	pieces.StoreBounds(Bounds(1, 2, 3, 4));
	auto second = make_shared<StringSink>();
	pieces.SetSink(second);
	pieces.StoreWay(data.ways[0]);
	pieces.Finish();
	string joined = first->data + second->data;
	CHECK(joined.find("bounds") == string::npos);
	CHECK(joined.substr(joined.size() - 3) == "\n]}");
	CHECK(first->data.size() > 0 && second->data.find("\"type\":\"way\"") != string::npos);

	//Large data: brackets and quotes balance, so the document is well formed
	OsmData sample = SampleData();
	string big = EncodeJson(sample);
	int depth = 0, minDepth = 0;
	bool inString = false;
	for(size_t i=0; i<big.size(); i++)
	{
		char c = big[i];
		if(inString)
		{
			if(c == '\\') i++;
			else if(c == '"') inString = false;
			continue;
		}
		if(c == '"') inString = true;
		else if(c == '{' || c == '[') depth++;
		else if(c == '}' || c == ']') depth--;
		if(depth < minDepth) minDepth = depth;
	}
	CHECK(depth == 0 && minDepth == 0 && !inString);
}

static void TestJsonDecode()
{
	//What the encoder writes reads back, including through the helper functions
	OsmData data = SampleData();
	string json = EncodeJson(data);
	OsmData back;
	LoadFromOsmJson(json, back);
	CHECK(Equivalent(back, data));
	istringstream in(json);
	OsmData fromStream;
	LoadFromOsmJson(*in.rdbuf(), fromStream);
	CHECK(fromStream == back);
	ostringstream saved;
	SaveToOsmJson(back, *saved.rdbuf());
	CHECK(saved.str() == json);
	CHECK(FormatFromFilename("map.json") == OsmFormat::OsmJson);

	//A document as the OSM API or Overpass would send it: members in any order,
	//whitespace, escapes, and extras this library has no place for
	OsmData parsed;
	LoadFromOsmJson(string(R"({
		"version": 0.6, "generator": "Overpass API", "osm3s": {"timestamp_osm_base": "x", "nested": [1, {"a": []}]},
		"bounds": {"minlat": 1, "minlon": 2.5, "maxlat": 3, "maxlon": 4, "extra": {"x": 1}},
		"elements": [
			{"tags": {"name": "Caf\u00e9 \"A\"\n", "empty": ""}, "lon": -2.5, "lat": 1.5, "id": -5, "type": "node",
			 "timestamp": "2020-01-02T03:04:05Z", "version": 3, "changeset": 9, "uid": 4, "user": "A & B"},
			{"type": "node", "id": 6, "visible": false, "version": 2},
			{"type": "way", "id": 7, "nodes": [-5, 6, 9007199254740993], "center": {"lat": 1, "lon": 2},
			 "geometry": [{"lat": 1, "lon": 2}, null], "bounds": {"minlat": 0}},
			{"type": "relation", "id": 8, "members": [
				{"type": "way", "ref": 7, "role": "outer", "geometry": [[1, 2]]},
				{"ref": -5, "type": "node"}], "tags": {}}
		],
		"remark": null
	})"), parsed);
	CHECK(parsed.bounds.size() == 1 && parsed.bounds[0] == Bounds(2.5, 1, 4, 3));
	CHECK(parsed.nodes.size() == 2 && parsed.ways.size() == 1 && parsed.relations.size() == 1);
	if(parsed.nodes.size() == 2 && parsed.ways.size() == 1 && parsed.relations.size() == 1)
	{
		const OsmNode &n = parsed.nodes[0];
		CHECK(n.objId == -5 && n.lat == 1.5 && n.lon == -2.5);
		CHECK(n.metaData.version == 3 && n.metaData.changeset == 9 && n.metaData.uid == 4);
		CHECK(n.metaData.username == "A & B" && n.metaData.visible);
		CHECK(n.metaData.timestamp == 1577934245);
		CHECK(n.tags == (TagMap{{"name", "Caf\xc3\xa9 \"A\"\n"}, {"empty", ""}}));
		CHECK(parsed.nodes[1].objId == 6 && !parsed.nodes[1].metaData.visible && parsed.nodes[1].lat == 0);
		//IDs beyond what a double can hold exactly are kept
		CHECK(parsed.ways[0].refs == (vector<int64_t>{-5, 6, 9007199254740993LL}));
		CHECK(parsed.ways[0].tags.empty());
		CHECK(parsed.relations[0].members == (vector<RelationMember>{
			RelationMember(ObjectType::Way, 7, "outer"), RelationMember(ObjectType::Node, -5, "")}));
	}

	//A reset marks each change of object type
	struct Recorder : public IDataStreamHandler
	{
		string calls;
		void StoreBounds(const Bounds &) override { calls += "b"; }
		void StoreNode(const OsmNode &) override { calls += "n"; }
		void StoreWay(const OsmWay &) override { calls += "w"; }
		void StoreRelation(const OsmRelation &) override { calls += "r"; }
		void Reset() override { calls += "|"; }
		void Finish() override { calls += "."; }
	};
	Recorder recorder;
	LoadFromOsmJson(json, recorder);
	CHECK(recorder.calls == "bnnnnn|ww|rr.");

	//Malformed documents, and well formed ones that are not OSM JSON
	const char *bad[] = {
		"",
		"[]",
		"42",
		"{",
		"{\"elements\":[{\"type\":\"node\",\"id\":1}",
		"{\"elements\":[]} {}",
		"{\"elements\":{}}",
		"{\"elements\":7}",
		"{\"elements\":[5]}",
		"{\"elements\":[[]]}",
		"{\"elements\":[{\"id\":1}]}",
		"{\"elements\":[{\"type\":\"area\",\"id\":1}]}",
		"{\"elements\":[{\"type\":7,\"id\":1}]}",
		"{\"elements\":[{\"type\":\"node\",\"id\":\"1\"}]}",
		"{\"elements\":[{\"type\":\"node\",\"id\":1.5}]}",
		"{\"elements\":[{\"type\":\"node\",\"id\":99999999999999999999}]}",
		"{\"elements\":[{\"type\":\"node\",\"id\":1,\"lat\":\"north\"}]}",
		"{\"elements\":[{\"type\":\"node\",\"id\":1,\"lat\":NaN}]}",
		"{\"elements\":[{\"type\":\"node\",\"id\":1,\"version\":-1}]}",
		"{\"elements\":[{\"type\":\"node\",\"id\":1,\"visible\":\"no\"}]}",
		"{\"elements\":[{\"type\":\"node\",\"id\":1,\"timestamp\":\"yesterday\"}]}",
		"{\"elements\":[{\"type\":\"node\",\"id\":1,\"tags\":{\"k\":5}}]}",
		"{\"elements\":[{\"type\":\"node\",\"id\":1,\"tags\":[]}]}",
		"{\"elements\":[{\"type\":\"way\",\"id\":1,\"nodes\":[\"2\"]}]}",
		"{\"elements\":[{\"type\":\"way\",\"id\":1,\"nodes\":{}}]}",
		"{\"elements\":[{\"type\":\"relation\",\"id\":1,\"members\":[{\"ref\":1}]}]}",
		"{\"elements\":[{\"type\":\"relation\",\"id\":1,\"members\":[{\"type\":\"area\",\"ref\":1}]}]}",
		"{\"elements\":[{\"type\":\"relation\",\"id\":1,\"members\":[3]}]}",
		"{\"bounds\":[],\"elements\":[]}",
		//The OSM API marks an incomplete response this way
		"{\"elements\":[{\"type\":\"node\",\"id\":1},{\"error\":\"Mismatch in tags key and value size\"}]}",
	};
	for(const char *doc : bad)
	{
		OsmData ignored;
		bool refused = Throws<OsmDecodeError>([&]{ LoadFromOsmJson(string(doc), ignored); });
		if(!refused)
			cerr << "accepted: " << doc << endl;
		CHECK(refused);
	}
	//The error says where
	try
	{
		OsmData ignored;
		LoadFromOsmJson(string("{\"elements\":[{\"type\":\"node\",,}]}"), ignored);
		CHECK(false);
	}
	catch(const OsmDecodeError &err)
	{
		CHECK(string(err.what()).find("JSON error") == 0 && string(err.what()).find(" at offset ") != string::npos);
	}

	//A document with no elements member is an empty map
	OsmData none;
	LoadFromOsmJson(string("{\"version\":\"0.6\"}"), none);
	CHECK(none.IsEmpty());

	//Limits
	const string doc = "{\"elements\":["
		"{\"type\":\"node\",\"id\":1,\"lat\":0,\"lon\":0,\"tags\":{\"a\":\"1\",\"b\":\"2\"}},"
		"{\"type\":\"way\",\"id\":2,\"nodes\":[1,1,1]},"
		"{\"type\":\"relation\",\"id\":3,\"members\":[{\"type\":\"node\",\"ref\":1,\"role\":\"\"},"
		"{\"type\":\"way\",\"ref\":2,\"role\":\"\"}]}]}";
	auto load = [&](const OsmXmlLimits &limits) { OsmData out; LoadFromOsmJson(doc, out, limits); return out; };
	OsmXmlLimits exact;
	exact.maxBytes = doc.size();
	exact.maxDepth = 5;
	exact.maxObjects = 3;
	exact.maxTagsPerObject = 2;
	exact.maxWayNodesPerObject = 3;
	exact.maxRelationMembersPerObject = 2;
	CHECK(load(exact).relations.size() == 1);
	struct Case { const char *name; size_t OsmXmlLimits::*field; size_t value; };
	const Case cases[] = {
		{"maxBytes", &OsmXmlLimits::maxBytes, doc.size() - 1},
		{"maxDepth", &OsmXmlLimits::maxDepth, 4},
		{"maxObjects", &OsmXmlLimits::maxObjects, 2},
		{"maxTagsPerObject", &OsmXmlLimits::maxTagsPerObject, 1},
		{"maxWayNodesPerObject", &OsmXmlLimits::maxWayNodesPerObject, 2},
		{"maxRelationMembersPerObject", &OsmXmlLimits::maxRelationMembersPerObject, 1},
	};
	for(const Case &c : cases)
	{
		OsmXmlLimits limits;
		limits.*(c.field) = c.value;
		CHECK(HitsLimit(c.name, [&]{ load(limits); }));
	}
	//Depth is counted inside values that are being skipped too
	OsmXmlLimits shallow;
	shallow.maxDepth = 10;
	string nested = "{\"junk\":" + string(50, '[') + string(50, ']') + ",\"elements\":[]}";
	OsmData ignored;
	CHECK(HitsLimit("maxDepth", [&]{ LoadFromOsmJson(nested, ignored, shallow); }));
	//Very deep nesting does not exhaust the stack when there is no limit
	string deep = "{\"junk\":" + string(200000, '[') + string(200000, ']') + ",\"elements\":[]}";
	LoadFromOsmJson(deep, ignored);
	CHECK(ignored.IsEmpty());
}

static void TestO5mLongitudeWrap()
{
	//o5m delta codes node coordinates in 32 bit arithmetic. Going from +179 to
	//-179 degrees is stored as the positive delta 714967296, which reaches the
	//right value only by wrapping round. This is the example in the format's
	//description, and what osmconvert writes.
	OsmData data;
	const double lons[] = {179.0, -179.0, 179.5, -180.0, 180.0, 0.0, -179.9999999, 179.9999999};
	int64_t id = 1;
	for(double lon : lons)
	{
		OsmNode n;
		n.objId = id++;
		n.lat = (id % 2) ? 89.5 : -89.5;
		n.lon = lon;
		data.nodes.push_back(n);
	}
	string encoded = Encode(OsmFormat::O5m, data);
	CHECK(Decode(OsmFormat::O5m, encoded) == data);

	//The second node's longitude delta, exactly as the description gives it
	string wrapped = EncodeZigzag(714967296);
	CHECK(encoded.find(wrapped) != string::npos);
	CHECK(encoded.find(EncodeZigzag(-3580000000LL)) == string::npos);

	//A file from a program using 64 bit deltas reads the same way
	string header("\xff\xe0\x04o5m2", 7);
	auto node = [](int64_t idDelta, int64_t lonDelta, int64_t latDelta) {
		string body = EncodeZigzag(idDelta) + string("\0", 1) + EncodeZigzag(lonDelta) + EncodeZigzag(latDelta);
		return string("\x10") + EncodeVarint(body.size()) + body;
	};
	for(int64_t delta : {(int64_t)714967296, (int64_t)-3580000000LL})
	{
		OsmData decoded = Decode(OsmFormat::O5m, header + node(1, 1790000000, 100000000) + node(1, delta, 0) + "\xfe");
		CHECK(decoded.nodes.size() == 2);
		if(decoded.nodes.size() == 2)
		{
			CHECK(decoded.nodes[0].lon == 179.0 && decoded.nodes[0].lat == 10.0);
			CHECK(decoded.nodes[1].lon == -179.0 && decoded.nodes[1].lat == 10.0);
		}
	}

	//Coordinates the format's 32 bits cannot hold are refused, not wrapped
	OsmData outside;
	outside.nodes.push_back(Node(1, 0, 215));
	CHECK(Throws<invalid_argument>([&]{ Encode(OsmFormat::O5m, outside); }));
	outside.nodes[0].lon = -215;
	CHECK(Throws<invalid_argument>([&]{ Encode(OsmFormat::O5m, outside); }));
	outside.nodes[0].lon = 214;
	CHECK(Decode(OsmFormat::O5m, Encode(OsmFormat::O5m, outside)) == outside);
}

static void TestO5mDeletes()
{
	//o5m marks a deleted object by cutting its dataset down to the ID, with or
	//without version and author. Live objects around it must be unaffected.
	OsmData data;
	data.isDiff = true;
	OsmNode before = Node(10, 50.5, -1.5);
	before.tags["name"] = "kept";
	OsmNode gone = Node(11, 51.5, -2.5);
	gone.metaData = Meta(3, 1600000000, 77, 5, "deleter");
	gone.metaData.visible = false;
	gone.tags["name"] = "dropped";
	OsmNode after = Node(12, 50.6, -1.6);
	after.tags["name"] = "kept";
	data.nodes = {before, gone, after};

	OsmWay liveWay;
	liveWay.objId = 20;
	liveWay.metaData = Meta(1, 1600000001, 78, 5, "deleter");
	liveWay.refs = {10, 12};
	OsmWay goneWay = liveWay;
	goneWay.objId = 21;
	goneWay.metaData.version = 2;
	goneWay.metaData.visible = false;
	OsmWay laterWay = liveWay;
	laterWay.objId = 22;
	laterWay.refs = {12, 10};
	data.ways = {liveWay, goneWay, laterWay};

	OsmRelation liveRel;
	liveRel.objId = 30;
	liveRel.metaData = Meta(1, 1600000002, 79, 5, "deleter");
	liveRel.members.push_back(RelationMember(ObjectType::Way, 20, "outer"));
	OsmRelation goneRel = liveRel;
	goneRel.objId = 31;
	goneRel.metaData.visible = false;
	OsmRelation laterRel = liveRel;
	laterRel.objId = 32;
	laterRel.members.push_back(RelationMember(ObjectType::Node, 10, ""));
	data.relations = {liveRel, goneRel, laterRel};

	//What should come back: deleted objects keep their identity and metadata only
	OsmData expected = data;
	expected.nodes[1].lat = 0.0;
	expected.nodes[1].lon = 0.0;
	expected.nodes[1].tags.clear();
	expected.ways[1].refs.clear();
	expected.relations[1].members.clear();

	string encoded = Encode(OsmFormat::O5m, data);
	OsmData decoded = Decode(OsmFormat::O5m, encoded);
	CHECK(Equivalent(decoded, expected));
	CHECK(decoded.isDiff);
	CHECK(!decoded.nodes[1].metaData.visible && decoded.nodes[0].metaData.visible && decoded.nodes[2].metaData.visible);
	CHECK(!decoded.ways[1].metaData.visible && !decoded.relations[1].metaData.visible);
	CHECK(decoded.nodes[1].metaData == gone.metaData);
	//Writing it again gives the same bytes
	CHECK(Encode(OsmFormat::O5m, decoded) == encoded);
	//In a plain o5m file too
	data.isDiff = false;
	expected.isDiff = false;
	CHECK(Equivalent(Decode(OsmFormat::O5m, Encode(OsmFormat::O5m, data)), expected));

	//A change file as osmconvert writes it: delete node 5 version 3, delete
	//way 6 version 2, then node 7 version 2 at lon 2 lat 1
	const string change("\xff\xe0\x04o5c2\xff\x10\x03\x0a\x03\x00\xff\x11\x03\x0c\x02\x00"
		"\xff\x10\x0b\x0e\x02\x00\x80\xb4\x89\x13\x80\xda\xc4\x09\xfe", 34);
	OsmData fromChange = Decode(OsmFormat::O5m, change);
	CHECK(fromChange.isDiff && fromChange.nodes.size() == 2 && fromChange.ways.size() == 1);
	if(fromChange.nodes.size() == 2 && fromChange.ways.size() == 1)
	{
		CHECK(fromChange.nodes[0].objId == 5 && fromChange.nodes[0].metaData.version == 3);
		CHECK(!fromChange.nodes[0].metaData.visible);
		CHECK(fromChange.ways[0].objId == 6 && fromChange.ways[0].metaData.version == 2);
		CHECK(!fromChange.ways[0].metaData.visible && fromChange.ways[0].refs.empty());
		CHECK(fromChange.nodes[1].objId == 7 && fromChange.nodes[1].metaData.visible);
		CHECK(fromChange.nodes[1].lon == 2.0 && fromChange.nodes[1].lat == 1.0);
	}

	//Cut down further still: the ID alone, and the ID with a version but no timestamp
	string header("\xff\xe0\x04o5c2", 7);
	OsmData idOnly = Decode(OsmFormat::O5m, header + string("\x10\x01\x0a", 3) + string("\x12\x02\x0c\x04", 4) + "\xfe");
	CHECK(idOnly.nodes.size() == 1 && idOnly.relations.size() == 1);
	if(idOnly.nodes.size() == 1 && idOnly.relations.size() == 1)
	{
		CHECK(idOnly.nodes[0].objId == 5 && !idOnly.nodes[0].metaData.visible && idOnly.nodes[0].metaData.version == 0);
		//The relation ID is a delta from the node's, in the same counter
		CHECK(idOnly.relations[0].objId == 11 && idOnly.relations[0].metaData.version == 4);
		CHECK(!idOnly.relations[0].metaData.visible);
	}

	//A way with no nodes is not a delete: it still has its (empty) node list
	OsmData hollow;
	OsmWay empty;
	empty.objId = 1;
	hollow.ways.push_back(empty);
	OsmData hollowBack = Decode(OsmFormat::O5m, Encode(OsmFormat::O5m, hollow));
	CHECK(hollowBack.ways.size() == 1 && hollowBack.ways[0].metaData.visible);

	//A node with a longitude but no latitude is damage, not a delete
	CHECK(Throws<OsmDecodeError>([&]{ Decode(OsmFormat::O5m, header + string("\x10\x03\x0a\x00\x02", 5) + "\xfe"); }));

	//Deletes survive conversion to and from XML
	OsmData viaXml = Decode(OsmFormat::OsmXml, Encode(OsmFormat::OsmXml, decoded));
	CHECK(Equivalent(Decode(OsmFormat::O5m, Encode(OsmFormat::O5m, viaXml)), decoded));
}

static void TestAttributes()
{
	//Attributes of the document itself: written by o5m, XML and JSON, and read back
	TagMap attribs;
	attribs["edit_activity_id"] = "123456789012";
	attribs["atomic_edit_id"] = "42";
	attribs["note"] = "Caf\xc3\xa9 <&> \"quoted\"";
	attribs["skipped"] = "";
	attribs["generator"] = "test";
	TagMap expected = attribs;
	expected.erase("skipped");
	expected.erase("generator");

	OsmData data = SampleData();
	const OsmFormat withAttribs[] = {OsmFormat::O5m, OsmFormat::OsmXml, OsmFormat::OsmJson};
	for(OsmFormat format : withAttribs)
	{
		auto sink = make_shared<StringSink>();
		auto encoder = MakeEncoder(format, sink, attribs);
		data.StreamTo(*encoder);
		OsmData decoded = Decode(format, sink->data);
		CHECK(decoded.attributes == expected);
		//The map itself is unaffected
		decoded.attributes.clear();
		CHECK(Equivalent(decoded, data));
		//A document with attributes and nothing else
		auto emptySink = make_shared<StringSink>();
		auto emptyEncoder = MakeEncoder(format, emptySink, attribs);
		OsmData().StreamTo(*emptyEncoder);
		CHECK(Decode(format, emptySink->data).attributes == expected);
		//Without attributes none are reported
		CHECK(Decode(format, Encode(format, data)).attributes.empty());
	}
	//PBF has nowhere to put them and writes the map as usual
	auto pbfSink = make_shared<StringSink>();
	auto pbfEncoder = MakeEncoder(OsmFormat::Pbf, pbfSink, attribs);
	data.StreamTo(*pbfEncoder);
	CHECK(pbfSink->data == Encode(OsmFormat::Pbf, data));

	//They reach a handler before the bounds and any object
	struct Recorder : public IDataStreamHandler
	{
		string calls;
		void StoreAttributes(const TagMap &) override { calls += "a"; }
		void StoreBounds(const Bounds &) override { calls += "b"; }
		void StoreNode(const OsmNode &) override { calls += "n"; }
		void StoreWay(const OsmWay &) override { calls += "w"; }
		void StoreRelation(const OsmRelation &) override { calls += "r"; }
	};
	for(OsmFormat format : withAttribs)
	{
		auto sink = make_shared<StringSink>();
		auto encoder = MakeEncoder(format, sink, attribs);
		data.StreamTo(*encoder);
		Recorder recorder;
		istringstream in(sink->data);
		MakeDecoder(format, *in.rdbuf(), recorder)->Decode();
		CHECK(recorder.calls == "abnnnnnwwrr");
	}
	//A store replays its attributes, and a filter passes them on
	OsmData withAttributes = data;
	withAttributes.attributes = expected;
	OsmData copy;
	DeduplicateOsm dedup(copy);
	withAttributes.StreamTo(dedup);
	CHECK(copy == withAttributes);
	copy.Clear();
	CHECK(copy.attributes.empty());

	//In o5m the attributes are a dataset of our own, straight after the header,
	//and a file without attributes is byte for byte what it was before
	string plain = Encode(OsmFormat::O5m, data);
	auto sink = make_shared<StringSink>();
	O5mEncode encoder(sink, TagMap{{"k", "v"}});
	data.StreamTo(encoder);
	const string dataset = string("\xc0\x16" "cppo5m-attributes\0k\0v\0", 24);
	CHECK(sink->data == plain.substr(0, 7) + dataset + plain.substr(7));
	//Only names this format can hold
	CHECK(Throws<invalid_argument>([&]{
		O5mEncode bad(make_shared<StringSink>(), TagMap{{string("a\0b", 3), "v"}});
		bad.Finish();
	}));

	//Another program's dataset of the same type is skipped, as any unknown one is
	string foreign = plain.substr(0, 7) + string("\xc0\x05hello", 7) + plain.substr(7);
	OsmData fromForeign = Decode(OsmFormat::O5m, foreign);
	CHECK(fromForeign.attributes.empty() && Equivalent(fromForeign, data));
	string unknown = plain.substr(0, 7) + string("\x20\x03" "abc", 5) + plain.substr(7);
	CHECK(Equivalent(Decode(OsmFormat::O5m, unknown), data));
	//Ours, but cut short
	string broken = plain.substr(0, 7) + string("\xc0\x14" "cppo5m-attributes\0k\0", 22) + plain.substr(7);
	CHECK(Throws<OsmDecodeError>([&]{ Decode(OsmFormat::O5m, broken); }));

	//XML and JSON written elsewhere: whatever the header holds besides version and generator
	OsmData xml;
	LoadFromOsmXml(string("<osm version='0.6' generator='x' copyright='OSM' upload='true'><node id='1'/></osm>"), xml);
	CHECK(xml.attributes == (TagMap{{"copyright", "OSM"}, {"upload", "true"}}));
	OsmData json;
	LoadFromOsmJson(string("{\"version\":0.6,\"generator\":\"x\",\"copyright\":\"OSM\",\"count\":5,"
		"\"osm3s\":{\"copyright\":\"inner\"},\"elements\":[],\"late\":\"ignored\"}"), json);
	CHECK(json.attributes == (TagMap{{"copyright", "OSM"}}));
}

static void TestXmlLimits()
{
	const string doc = "<osm><node id='1' lat='0' lon='0'><tag k='a' v='1'/><tag k='b' v='2'/></node>"
		"<way id='2'><nd ref='1'/><nd ref='1'/><nd ref='1'/></way>"
		"<relation id='3'><member type='node' ref='1' role=''/><member type='way' ref='2' role=''/></relation></osm>";

	auto load = [&](const OsmXmlLimits &limits) { OsmData out; LoadFromOsmXml(doc, out, limits); return out; };

	//Limits set exactly at the document's size allow it
	OsmXmlLimits exact;
	exact.maxBytes = doc.size();
	exact.maxDepth = 3;
	exact.maxObjects = 3;
	exact.maxTagsPerObject = 2;
	exact.maxWayNodesPerObject = 3;
	exact.maxRelationMembersPerObject = 2;
	exact.maxAttributesPerElement = 3;
	exact.maxAttributeBytes = 100;
	CHECK(load(exact).relations.size() == 1);
	CHECK(load(OsmXmlLimits()).relations.size() == 1);

	//One less than needed, for each limit in turn
	struct Case { const char *name; size_t OsmXmlLimits::*field; size_t value; };
	const Case cases[] = {
		{"maxBytes", &OsmXmlLimits::maxBytes, doc.size() - 1},
		{"maxDepth", &OsmXmlLimits::maxDepth, 2},
		{"maxObjects", &OsmXmlLimits::maxObjects, 2},
		{"maxTagsPerObject", &OsmXmlLimits::maxTagsPerObject, 1},
		{"maxWayNodesPerObject", &OsmXmlLimits::maxWayNodesPerObject, 2},
		{"maxRelationMembersPerObject", &OsmXmlLimits::maxRelationMembersPerObject, 1},
		{"maxAttributesPerElement", &OsmXmlLimits::maxAttributesPerElement, 2},
		{"maxAttributeBytes", &OsmXmlLimits::maxAttributeBytes, 10},
	};
	for(const Case &c : cases)
	{
		OsmXmlLimits limits;
		limits.*(c.field) = c.value;
		CHECK(HitsLimit(c.name, [&]{ load(limits); }));
		//osmChange documents obey the same limits
		string change = "<osmChange><create>" + doc.substr(5, doc.size() - 11) + "</create></osmChange>";
		OsmXmlLimits changeLimits = limits;
		if(string(c.name) == "maxBytes") changeLimits.maxBytes = change.size() - 1;
		if(string(c.name) == "maxDepth") changeLimits.maxDepth = 3;
		OsmChange out;
		CHECK(HitsLimit(c.name, [&]{ LoadFromOsmChangeXml(change, out, changeLimits); }));
	}

	//A limit error is also a decode error, and says what happened
	OsmXmlLimits small;
	small.maxObjects = 1;
	try { load(small); CHECK(false); }
	catch(const OsmDecodeError &err)
	{
		CHECK(string(err.what()) == "maxObjects limit exceeded; maximum is 1, got 2");
	}

	//Limits by name
	OsmXmlLimits named;
	named.Apply({{"max_bytes", 10}, {"max_members_per_object", 5}, {"max_depth", 4}});
	CHECK(named.maxBytes == 10 && named.maxDepth == 4);
	CHECK(named.maxWayNodesPerObject == 5 && named.maxRelationMembersPerObject == 5);
	CHECK(Throws<invalid_argument>([&]{ named.Apply({{"max_nonsense", 1}}); }));
	CHECK(Throws<invalid_argument>([&]{ named.Apply({{"max_bytes", -1}}); }));

	//The byte limit counts across pieces
	OsmXmlLimits bytes;
	bytes.maxBytes = 20;
	OsmData out;
	OsmXmlParser parser(out, bytes);
	parser.Feed(doc.substr(0, 15), false);
	CHECK(HitsLimit("maxBytes", [&]{ parser.Feed(doc.substr(15, 15), false); }));
}

static void TestOsmChange()
{
	OsmChange change;
	LoadFromOsmChangeXml(ReadFile("tests/data/examplechange.osm"), change);
	CHECK(change.blocks.size() == 1);
	if(change.blocks.size() == 1)
	{
		CHECK(change.blocks[0].action == "create" && !change.blocks[0].ifUnused);
		CHECK(change.blocks[0].data.nodes.size() == 1 && change.blocks[0].data.nodes[0].objId == -5393);
		CHECK(change.blocks[0].data.isDiff);
	}

	OsmData sample = SampleData();
	OsmChange built;
	built.blocks.push_back(OsmChangeBlock("create"));
	built.blocks.back().data.nodes = sample.nodes;
	built.blocks.back().data.ways = sample.ways;
	built.blocks.push_back(OsmChangeBlock("modify"));
	built.blocks.back().data.relations = sample.relations;
	built.blocks.push_back(OsmChangeBlock("delete", true));
	built.blocks.back().data.nodes.push_back(sample.nodes[0]);
	built.blocks.back().data.ways.push_back(sample.ways[0]);
	built.blocks.back().data.relations.push_back(sample.relations[0]);
	built.blocks.push_back(OsmChangeBlock("delete"));
	built.blocks.back().data.nodes.push_back(sample.nodes[1]);
	for(OsmChangeBlock &block : built.blocks)
		block.data.isDiff = true;

	ostringstream encoded;
	SaveToOsmChangeXml(built, *encoded.rdbuf());
	string xml = encoded.str();
	CHECK(xml.find("<osmChange version=\"0.6\" generator=\"cppo5m\">") != string::npos);
	CHECK(xml.find("<delete if-unused=\"true\">") != string::npos);
	CHECK(xml.find("<delete>") != string::npos);
	//Deletes list relations before ways before nodes
	size_t del = xml.find("<delete if-unused");
	CHECK(xml.find("<relation", del) < xml.find("<way", del) && xml.find("<way", del) < xml.find("<node", del));

	OsmChange decoded;
	LoadFromOsmChangeXml(xml, decoded);
	CHECK(decoded.blocks.size() == 4);
	bool same = decoded.blocks.size() == built.blocks.size();
	for(size_t i=0; same && i<built.blocks.size(); i++)
		same = decoded.blocks[i].action == built.blocks[i].action &&
			decoded.blocks[i].ifUnused == built.blocks[i].ifUnused &&
			Equivalent(decoded.blocks[i].data, built.blocks[i].data);
	CHECK(same);

	//From a stream, and with each object in its own action element
	istringstream in(xml);
	OsmChange fromStream;
	LoadFromOsmChangeXml(*in.rdbuf(), fromStream);
	CHECK(fromStream == decoded);

	ostringstream separate;
	SaveToOsmChangeXml(built, *separate.rdbuf(), true);
	OsmChange separated;
	LoadFromOsmChangeXml(separate.str(), separated);
	CHECK(separated.blocks.size() == 5 + 2 + 2 + 3 + 1);
	for(const OsmChangeBlock &block : separated.blocks)
		CHECK(block.data.nodes.size() + block.data.ways.size() + block.data.relations.size() == 1);

	//Decoder to encoder as a stream, with a finish at the end
	auto sink = make_shared<StringSink>();
	OsmChangeXmlEncode streaming(sink);
	LoadFromOsmChangeXml(xml, streaming);
	CHECK(sink->data == xml);

	const char *bad[] = {
		"<osm><node id='1'/></osm>",
		"<osmChange><replace><node id='1'/></replace></osmChange>",
		"<osmChange><create><node id='1'></create></osmChange>",
	};
	for(const char *doc : bad)
	{
		OsmChange ignored;
		CHECK(Throws<OsmDecodeError>([&]{ LoadFromOsmChangeXml(string(doc), ignored); }));
	}
	OsmChange invalid;
	invalid.blocks.push_back(OsmChangeBlock("replace"));
	ostringstream ignoredOut;
	CHECK(Throws<invalid_argument>([&]{ SaveToOsmChangeXml(invalid, *ignoredOut.rdbuf()); }));
}

static void TestFilters()
{
	OsmData data = SampleData();

	//Duplicates are dropped by type and ID
	OsmData doubled = data;
	doubled.nodes.insert(doubled.nodes.end(), data.nodes.begin(), data.nodes.end());
	doubled.ways.push_back(data.ways[0]);
	doubled.relations.push_back(data.relations[1]);
	OsmData unique;
	DeduplicateOsm dedup(unique);
	doubled.StreamTo(dedup);
	CHECK(unique == data);
	//A way may share an ID with a node
	OsmData mixed;
	OsmWay sameId;
	sameId.objId = 1;
	mixed.nodes.push_back(data.nodes[0]);
	mixed.ways.push_back(sameId);
	OsmData kept;
	DeduplicateOsm dedup2(kept);
	mixed.StreamTo(dedup2, false);
	mixed.StreamTo(dedup2, false);
	CHECK(kept.nodes.size() == 1 && kept.ways.size() == 1);
	dedup2.ResetExisting();
	mixed.StreamTo(dedup2);
	CHECK(kept.nodes.size() == 2 && kept.ways.size() == 2);

	//Sorting orders each type by ID
	OsmData sorted;
	SortOsm sorter(sorted);
	data.StreamTo(sorter);
	vector<int64_t> ids;
	for(const OsmNode &n : sorted.nodes)
		ids.push_back(n.objId);
	CHECK(ids == (vector<int64_t>{1, 2, 3, 4, 5000000000LL}));
	CHECK(sorted.ways.size() == 2 && sorted.relations.size() == 2 && sorted.bounds == data.bounds);

	//Bounds are found without reading the rest of the file
	for(OsmFormat format : allFormats)
	{
		OsmData noBounds = data;
		noBounds.bounds.clear();
		string encoded = Encode(format, noBounds);
		string withBounds = Encode(format, data);
		for(const string *doc : {&encoded, &withBounds})
		{
			istringstream in(*doc);
			FindBbox finder;
			auto decoder = MakeDecoder(format, *in.rdbuf(), finder);
			while(!finder.done && decoder->DecodeNext()) {}
			CHECK(finder.done);
			CHECK(finder.bboxFound == (doc == &withBounds));
			if(doc == &withBounds)
				CHECK(Near(finder.bounds.minLon, -1.5) && Near(finder.bounds.maxLat, 51.5));
		}
	}
}

static void TestSinks()
{
	StringSink text;
	text.Write("ab", 2);
	text.Write(string("cd"));
	CHECK(text.data == "abcd");

	//A stream that cannot be written to is reported, not ignored
	std::filebuf closed;
	CHECK(Throws<runtime_error>([&]{
		O5mEncode encoder(closed);
		SampleData().StreamTo(encoder);
	}));
	shared_ptr<ByteSink> none;
	CHECK(Throws<invalid_argument>([&]{ O5mEncode nullSink(none); }));
	CHECK(Throws<invalid_argument>([&]{ OsmChangeXmlEncode nullSink(none); }));
	O5mEncode encoder(make_shared<StringSink>());
	CHECK(Throws<invalid_argument>([&]{ encoder.SetSink(none); }));
}

int main()
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
#if GOOGLE_PROTOBUF_VERSION < 4022000
	//Decoding deliberately corrupt PBF data makes protobuf log to stderr
	google::protobuf::LogSilencer quietProtobuf;
#endif

	const pair<const char *, void (*)()> tests[] = {
		{"varint", TestVarint},
		{"model", TestModel},
		{"osmchange grouping", TestOsmChangeGrouping},
		{"round trips", TestRoundTrips},
		{"large data", TestLargeData},
		{"sample files", TestSampleFiles},
		{"decoder contract", TestDecoderContract},
		{"bad input", TestBadInput},
		{"hostile input", TestHostileInput},
		{"pbf blocks", TestPbfBlocks},
		{"xml", TestXml},
		{"json", TestJson},
		{"json decode", TestJsonDecode},
		{"o5m longitude wrap", TestO5mLongitudeWrap},
		{"o5m deletes", TestO5mDeletes},
		{"attributes", TestAttributes},
		{"xml limits", TestXmlLimits},
		{"osmchange", TestOsmChange},
		{"filters", TestFilters},
		{"sinks", TestSinks},
	};
	for(const auto &test : tests)
	{
		currentTest = test.first;
		try
		{
			test.second();
		}
		catch(const std::exception &err)
		{
			failures++;
			cerr << "FAIL " << currentTest << ": unexpected exception: " << err.what() << endl;
		}
	}

	cout << checks << " checks, " << failures << " failures" << endl;
	return failures ? 1 : 0;
}
