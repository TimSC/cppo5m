
#include <iostream>
#include <fstream>
#include <sstream>
#include "pbf/fileformat.pb.h"
#include "pbf/osmformat.pb.h"
#include <arpa/inet.h>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include "OsmData.h"
#include "utils.h"
using namespace std;

// https://stackoverflow.com/questions/27529570/simple-zlib-c-string-compression-and-decompression
std::string DecompressData(const std::string &data)
{
    std::stringstream compressed;
    std::stringstream decompressed;
    compressed << data;
    boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
    in.push(boost::iostreams::zlib_decompressor());
    in.push(compressed);
    boost::iostreams::copy(in, decompressed);
    return decompressed.str();
}

void DecodeOsmHeader(std::string &decBlob,
	std::shared_ptr<class IDataStreamHandler> output)
{
	OSMPBF::HeaderBlock hb;
	std::istringstream iss(decBlob);
	bool ok = hb.ParseFromIstream(&iss);
	//cout << "hb ok " << ok << endl;

	if(hb.has_bbox())
	{
		const OSMPBF::HeaderBBox &bbox = hb.bbox();
		
		if(bbox.has_left() and bbox.has_right() and bbox.has_top() and bbox.has_bottom())
			output->StoreBounds(bbox.left()*1e-9, bbox.bottom()*1e-9, bbox.right()*1e-9, bbox.top()*1e-9);
	}
}

void DecodeOsmDenseNodes(const OSMPBF::DenseNodes &dense,
	int64_t lat_offset, int64_t lon_offset,
	int32_t granularity, int32_t date_granularity,
	const std::vector<std::string> &stringTab,
	std::shared_ptr<class IDataStreamHandler> output)
{
	//Decode tags
	vector<map<string, string> > tags;
	map<string, string> current;
	for(int j=0; j<dense.keys_vals_size(); j++)
	{
		int32_t sti = dense.keys_vals(j);
		if(sti > 0 and sti < stringTab.size() and j<dense.keys_vals_size()-1)
		{
			int32_t sti2 = dense.keys_vals(j+1);
			current[stringTab[sti]] = stringTab[sti2];
			j++;
		}
		else
		{
			tags.push_back(current);
			current.clear();
		}
	}

	int64_t idc = 0, latc = 0, lonc = 0, timestampc = 0, changesetc = 0;
	int32_t uidc = 0, user_sidc = 0;
	for(int j=0; j<dense.id_size() and j<dense.lat_size() and j<dense.lon_size(); j++)
	{
		idc += dense.id(j);
		latc += dense.lat(j);
		lonc += dense.lon(j);

		class MetaData metaData;
		TagMap empty;
		const TagMap *tagMapPtr = &empty;
		if(j < tags.size())
			tagMapPtr = &tags[j];
		
		if(dense.has_denseinfo())
		{
			const OSMPBF::DenseInfo &di = dense.denseinfo();
			if(j < di.version_size())
				metaData.version = di.version(j);

			if(j < di.timestamp_size())
			{
				timestampc += di.timestamp(j);
				metaData.timestamp = timestampc*date_granularity / 1000;
			}

			if(j < di.changeset_size())
			{
				changesetc += di.changeset(j);
				metaData.changeset = changesetc;
			}

			if(j < di.uid_size())
			{
				uidc += di.uid(j);
				metaData.uid = uidc;
			}

			if(j < di.user_sid_size())
			{
				user_sidc += di.user_sid(j);
				if(user_sidc > 0 and user_sidc <= stringTab.size())
					metaData.username = stringTab[user_sidc];
			}

			if(j < di.visible_size())
				metaData.visible = di.visible(j);

		}

		bool halt = output->StoreNode(idc, metaData, 
			*tagMapPtr, 
			1e-9 * (lat_offset + (granularity * latc)), 
			1e-9 * (lon_offset + (granularity * lonc)));
	}

}

void DecodeOsmWays(const OSMPBF::PrimitiveGroup& pg,
	int32_t date_granularity,
	const std::vector<std::string> &stringTab,
	std::shared_ptr<class IDataStreamHandler> output)
{
	for(int i=0; i<pg.ways_size(); i++)
	{
		const OSMPBF::Way &way = pg.ways(i);
		class MetaData metaData;
		TagMap tags;
		std::vector<int64_t> refs;
		
		for(int j=0; j<way.keys_size() and j<way.vals_size(); j++)
		{
			uint32_t keyIndex = way.keys(j);
			uint32_t valIndex = way.vals(j);
			if(keyIndex > 0 and keyIndex < stringTab.size() and valIndex > 0 and valIndex < stringTab.size())
				tags[stringTab[keyIndex]] = stringTab[valIndex];
		}

		if(way.has_info())
		{
			const OSMPBF::Info &info = way.info();

			if(info.has_version())
				metaData.version = info.version();
			if(info.has_timestamp())
				metaData.timestamp = info.timestamp()*date_granularity / 1000;
			if(info.has_changeset())
				metaData.changeset = info.changeset();
			if(info.has_uid())
				metaData.uid = info.uid();
			if(info.has_user_sid() and info.user_sid() > 0 and info.user_sid() < stringTab.size())
				metaData.username = stringTab[info.user_sid()];
			if(info.has_visible())
				metaData.visible = info.visible();
		}

		int64_t refsc = 0;
		for(int j=0; j<way.refs_size(); j++)
		{
			refsc += way.refs(j);
			refs.push_back(refsc);
		}
		
		bool halt = output->StoreWay(way.id(), metaData, 
			tags, refs);
	}

}

void DecodeOsmData(std::string &decBlob, std::shared_ptr<class IDataStreamHandler> output)
{
	OSMPBF::PrimitiveBlock pb;
	std::istringstream iss(decBlob);
	bool ok = pb.ParseFromIstream(&iss);
	cout << "pb ok " << ok << endl;

	std::vector<std::string> stringTab;
	if(pb.has_stringtable())
	{
		const OSMPBF::StringTable &st = pb.stringtable();
		for(int i=0; i<st.s_size(); i++)
			stringTab.push_back(st.s(i));
	}

	int64_t lat_offset = 0, lon_offset = 0;
	int32_t granularity = 100, date_granularity=1000;
	if(pb.has_granularity())
		granularity = pb.granularity();
	if(pb.has_lat_offset())
		lat_offset = pb.lat_offset();
	if(pb.has_lon_offset())
		lon_offset = pb.lon_offset();
	if(pb.has_date_granularity())
		lon_offset = pb.date_granularity();

	int pbs = pb.primitivegroup_size();
	for(int i=0; i<pbs; i++)
	{
		const OSMPBF::PrimitiveGroup& pg = pb.primitivegroup(i);

		cout << "n " << pg.nodes_size() << endl;
		//cout << "dn " << pg.has_dense() << endl;
		if(pg.has_dense())
		{
			const OSMPBF::DenseNodes &dense = pg.dense();
			DecodeOsmDenseNodes(dense, lat_offset, lon_offset,
				granularity, date_granularity,
				stringTab, output);
		}

		cout << "w " << pg.ways_size() << endl;
		if(pg.ways_size() > 0)
			DecodeOsmWays(pg, date_granularity,
				stringTab, output);

		cout << "r " << pg.relations_size() << endl;
		cout << "cs " << pg.changesets_size() << endl;
	}
}

int main()
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	shared_ptr<class OsmData> osmData(new class OsmData());

	std::filebuf infi;
	infi.open("sample.pbf", std::ios::in);

	while(infi.in_avail()>0)
	{

		int32_t blobHeaderLenNbo;
		infi.sgetn ((char*)&blobHeaderLenNbo, sizeof(int32_t));
		int32_t blobHeaderLen = ntohl(blobHeaderLenNbo);
		cout << blobHeaderLen << endl;

		std::string refData;
		refData.resize(blobHeaderLen);
		infi.sgetn(&refData[0], blobHeaderLen);

		OSMPBF::BlobHeader header;
		std::istringstream iss(refData);
		bool ok = header.ParseFromIstream(&iss);

		std::string headerType = header.type();
		//cout << headerType << endl;
		//cout << header.indexdata() << endl;
		//cout << header.datasize() << endl;

		int32_t blobSize = header.datasize();
		std::string refData2;
		refData2.resize(blobSize);
		infi.sgetn(&refData2[0], blobSize);

		OSMPBF::Blob blob;
		std::istringstream iss2(refData2);
		ok = blob.ParseFromIstream(&iss2);

		/*cout << "has_raw" << blob.has_raw() << endl;
		cout << "has_raw_size" << blob.has_raw_size();
		if(blob.has_raw_size())
			cout << " of " << blob.raw_size();
		cout << endl;
		cout << "has_zlib_data" << blob.has_zlib_data() << endl;
		cout << "has_lzma_data" << blob.has_lzma_data() << endl;*/

		string decBlob;
		if(blob.has_raw())
			decBlob = blob.raw();
		else if(blob.has_zlib_data())
			decBlob = DecompressData(blob.zlib_data());
		//cout << decBlob.length() << endl;

		if(headerType == "OSMHeader")
			DecodeOsmHeader(decBlob, osmData);

		else if(headerType == "OSMData")
			DecodeOsmData(decBlob, osmData);

	}

	cout << "nodes " << osmData->nodes.size() << endl;
	cout << "ways " << osmData->ways.size() << endl;
	cout << "relations " << osmData->relations.size() << endl;

	std::filebuf outfi;
	outfi.open("pbftest.osm", std::ios::out);
	SaveToOsmXml(*osmData.get(), outfi);
	outfi.close();
}	

