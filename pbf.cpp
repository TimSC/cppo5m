
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

void DecodeOsmData(std::string &decBlob)
{
	OSMPBF::PrimitiveBlock pb;
	std::istringstream iss(decBlob);
	bool ok = pb.ParseFromIstream(&iss);
	cout << "pb ok " << ok << endl;

	int pbs = pb.primitivegroup_size();
	for(int i=0; i<pbs; i++)
	{
		const OSMPBF::PrimitiveGroup& pg = pb.primitivegroup(i);

		cout << "n " << pg.nodes_size() << endl;
		cout << "dn " << pg.has_dense() << endl;
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
		bool a = header.ParseFromIstream(&iss);
		cout << a << endl;

		std::string headerType = header.type();
		cout << headerType << endl;
		//cout << header.indexdata() << endl;
		cout << header.datasize() << endl;

		int32_t blobSize = header.datasize();
		std::string refData2;
		refData2.resize(blobSize);
		infi.sgetn(&refData2[0], blobSize);

		OSMPBF::Blob blob;
		std::istringstream iss2(refData2);
		bool a2 = blob.ParseFromIstream(&iss2);
		cout << a2 << endl;

		cout << "has_raw" << blob.has_raw() << endl;
		cout << "has_raw_size" << blob.has_raw_size();
		if(blob.has_raw_size())
			cout << " of " << blob.raw_size();
		cout << endl;
		cout << "has_zlib_data" << blob.has_zlib_data() << endl;
		cout << "has_lzma_data" << blob.has_lzma_data() << endl;

		string decBlob;
		if(blob.has_raw())
			decBlob = blob.raw();
		else if(blob.has_zlib_data())
			decBlob = DecompressData(blob.zlib_data());
		cout << decBlob.length() << endl;

		if(headerType == "OSMHeader")
		{
			DecodeOsmHeader(decBlob);
		}
		else if(headerType == "OSMData")
		{
			DecodeOsmData(decBlob);
		} 

	}	
}	

