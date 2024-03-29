//Convertion utility, modelled after osmconvert

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <boost/program_options.hpp>
#include "OsmData.h"
#include "osmxml.h"
#include "o5m.h"
#include "pbf.h"
#include "utils.h"
#include "pbf/osmformat.pb.h"

using namespace std;
namespace po = boost::program_options;

//http://stackoverflow.com/a/236803/4288232
void split2(const string &s, char delim, vector<string> &elems) {
	stringstream ss;
	ss.str(s);
	string item;
	while (getline(ss, item, delim)) {
		elems.push_back(item);
	}
}

vector<string> split(const string &s, char delim) {
	vector<string> elems;
	split2(s, delim, elems);
	return elems;
}

int main(int argc, char* argv[])
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	std::cin.sync_with_stdio(false);

	vector<string> inputFiles;
	string outputFile;
	bool formatInOsm = false, formatInO5m = false, formatInPbf = false;
	bool formatOutOsm = false, formatOutO5m = false, formatOutPbf = false;
	bool formatOutNull = false, sort = false;
	po::options_description desc("Convert between osm, o5m, pbf file formats");
	desc.add_options()
		("help",																 "show help message")
		("input",  po::value< vector<string> >(&inputFiles),					 "input file (or '-' for console stream)")
		("output,o", po::value< string >(&outputFile),						   "output file")
		("in-osm", po::bool_switch(&formatInOsm),			   "input file format is osm")
		("in-o5m", po::bool_switch(&formatInO5m),			   "input file format is o5m")
		("in-pbf", po::bool_switch(&formatInPbf),			   "input file format is pbf")
		("out-osm", po::bool_switch(&formatOutOsm),			   "output file format is osm")
		("out-o5m", po::bool_switch(&formatOutO5m),			   "output file format is o5m")
		("out-pbf", po::bool_switch(&formatOutPbf),			   "output file format is pbf")
		("out-null", po::bool_switch(&formatOutNull),		   "do not write output")
		("sort", po::bool_switch(&sort),		   "sort output by ID (memory intensive)")
	;
	po::positional_options_description p;
	p.add("input", -1);
	po::variables_map vm;
	try {
		po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
	}
	catch (const po::unknown_option& ex) {
		cerr << "Unknown option: " << ex.get_option_name() << endl;
		return -1;
	}
	po::notify(vm);
	
	if (vm.count("help")) { cout << desc << endl; return 1; }
	if (vm.count("input")==0) { cerr << "You must specify at least one input file." << endl; cout << desc << endl; return -1; }
	
	std::streambuf *outbuff = nullptr;
	bool consoleMode = false;
	if(outputFile.size() > 0)
	{
		std::filebuf *outfb = new std::filebuf;
		outfb->open(outputFile, std::ios::out | std::ios::binary);
		if(!outfb->is_open())
		{
			cerr << "Error opening output file " << outputFile << endl;
			exit(0);
		}
		outbuff = outfb;
	}
	else
	{
		consoleMode = true;
		outbuff = std::cout.rdbuf();
	}

	//Prepare encoder
	vector<string> outFilenameSplit = split(outputFile, '.');
	int64_t filePart = (int64_t)(outFilenameSplit.size()-1);
	shared_ptr<class IDataStreamHandler> enc;
	TagMap customAttribs;

	if(formatOutNull)
		enc.reset(new class IDataStreamHandler());
	else if(formatOutO5m or (filePart > -1 and outFilenameSplit[filePart] == "o5m"))
		enc.reset(new class O5mEncode(*outbuff));
	else if(formatOutPbf or (filePart > -1 and outFilenameSplit[filePart] == "pbf"))
		enc.reset(new class PbfEncode(*outbuff));
	else if (formatOutOsm or (filePart > -1 and outFilenameSplit[filePart] == "osm") or consoleMode)
		enc.reset(new class OsmXmlEncode(*outbuff, customAttribs));
	else
		throw runtime_error("Output file extension not supported");

	if(sort)
		enc.reset(new OsmFilterRenumber(enc));

	//Prepare input
	bool consoleInput = false;
	std::streambuf *inbuff = nullptr;
	string inFormat = "";
	std::shared_ptr<class OsmDecoder> inDecoder;
	if(inputFiles[0] != "-")
	{
		std::filebuf *infb = new std::filebuf;
		infb->open(inputFiles[0], std::ios::in | std::ios::binary);
		if(!infb->is_open())
		{
			cerr << "Error opening input file " << inputFiles[0] << endl;
			exit(0);
		}

		inbuff = infb;
		vector<string> inFilenameSplit = split(inputFiles[0], '.');
		int64_t filePart2 = (int64_t)(inFilenameSplit.size()-1);
		if(formatInO5m or inFilenameSplit[filePart2] == "o5m")
		{
			inFormat = "o5m";
			inDecoder = make_shared<O5mDecode>(*inbuff);
		}
		else if (formatInOsm or inFilenameSplit[filePart2] == "osm")
		{
			inFormat = "osm";
			inDecoder = make_shared<OsmXmlDecode>(*inbuff);
		}
		else if (formatInPbf or inFilenameSplit[filePart2] == "pbf")
		{
			inFormat = "pbf";
			inDecoder = make_shared<PbfDecode>(*inbuff);
		}
		else
			throw runtime_error("Input file extension not supported");
	}
	else
	{
		consoleInput = true;
		inbuff = std::cin.rdbuf();

		if(formatInO5m)
		{
			inFormat = "o5m";
			inDecoder = make_shared<O5mDecode>(*inbuff);
		}
		if(formatInPbf)
		{
			inFormat = "pbf";
			inDecoder = make_shared<PbfDecode>(*inbuff);
		}
		else
		{
			inFormat = "osm";
			inDecoder = make_shared<OsmXmlDecode>(*inbuff);
		}
	}


	if(!inDecoder)
		throw runtime_error("Input file extension not specified/supported");

	//Run decoder
	LoadFromDecoder(*inbuff, inDecoder.get(), enc.get());

	//Tidy up. It is a good idea to delete the pipeline in order.
	if(!consoleInput)
	{
		delete inbuff;
		inbuff = nullptr;
	}
	inDecoder.reset();
	enc.reset();
	if(!consoleMode)
	{
		delete outbuff;
		outbuff = nullptr;
	}
}

