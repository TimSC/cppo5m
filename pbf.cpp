
#include <iostream>
#include <fstream>
#include <sstream>
#include "pbf/fileformat.pb.h"
#include "pbf/osmformat.pb.h"
#include <arpa/inet.h>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/zlib.hpp>
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

void DecodeOsmHeader(std::string &decBlob)
{
	OSMPBF::HeaderBlock hb;
	std::istringstream iss(decBlob);
	bool ok = hb.ParseFromIstream(&iss);
	cout << "hb ok " << ok << endl;

	if(hb.has_bbox())
	{
		const OSMPBF::HeaderBBox &bbox = hb.bbox();
		
		if(bbox.has_left() and bbox.has_right() and bbox.has_top() and bbox.has_bottom())
		{
			cout << "bbox " << bbox.left()*1e-9 << "," << bbox.bottom()*1e-9 << "," 
				<< bbox.right()*1e-9 << "," << bbox.top()*1e-9 << endl;
		}
	}
}

void DecodeOsmDenseNodes(const OSMPBF::DenseNodes &dense,
	int64_t lat_offset, int64_t lon_offset,
	int32_t granularity, int32_t date_granularity,
	const std::vector<std::string> &stringTab)
{
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

	for(size_t j=0; j<tags.size(); j++)
	{
		cout << "tags {";
		for(auto it=tags[j].begin(); it!=tags[j].end(); it++)
		{
			cout << it->first << "=" << it->second << ",";
		}
		cout << "}" << endl;
	} 

	int64_t idc = 0, latc = 0, lonc = 0;
	for(int j=0; j<dense.id_size() and j<dense.lat_size() and j<dense.lon_size(); j++)
	{
		idc += dense.id(j);
		latc += dense.lat(j);
		lonc += dense.lon(j);
		cout << "id " << idc << "," << (1e-9 * (lat_offset + (granularity * latc))) 
			<< "," << (1e-9 * (lon_offset + (granularity * lonc))) << endl;
	}

}

void DecodeOsmData(std::string &decBlob)
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
		cout << "dn " << pg.has_dense() << endl;
		if(pg.has_dense())
		{
			const OSMPBF::DenseNodes &dense = pg.dense();
			DecodeOsmDenseNodes(dense, lat_offset, lon_offset,
				granularity, date_granularity,
				stringTab);
		}

		cout << "w " << pg.ways_size() << endl;
		cout << "r " << pg.relations_size() << endl;
		cout << "cs " << pg.changesets_size() << endl;
	}
}

int main()
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;

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
			DecodeOsmHeader(decBlob);

		else if(headerType == "OSMData")
			DecodeOsmData(decBlob);

	}	
}	

