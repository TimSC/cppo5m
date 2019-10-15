
#include <iostream>
#include <fstream>
#include <sstream>
#include "pbf/fileformat.pb.h"
#include "pbf/osmformat.pb.h"
#include <arpa/inet.h>
using namespace std;

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
		cout << header.indexdata() << endl;
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
		cout << "has_raw_size" << blob.has_raw_size() << endl;
		cout << "has_zlib_data" << blob.has_zlib_data() << endl;
		cout << "has_lzma_data" << blob.has_lzma_data() << endl;

	}	
}	

