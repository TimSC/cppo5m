//Convertion utility, modelled after osmconvert

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <boost/program_options.hpp>
#include "OsmData.h"
#include "osmxml.h"
#include "o5m.h"

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
	std::cin.sync_with_stdio(false);

	vector<string> inputFiles;
	string outputFile;
	bool formatInOsm = false, formatInO5m = false;
	bool formatOutNull = false;
	po::options_description desc("Convert between osm, o5m file formats");
	desc.add_options()
		("help",                                                                 "show help message")
		("input",  po::value< vector<string> >(&inputFiles),                     "input file (or '-' for console stream)")
		("output,o", po::value< string >(&outputFile),                           "output file")
		("in-osm", po::bool_switch(&formatInOsm),               "input file format is osm")
		("in-o5m", po::bool_switch(&formatInO5m),               "input file format is o5m")
		("out-null", po::bool_switch(&formatOutNull),           "do not write output")
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
	else if(filePart > -1 and outFilenameSplit[filePart] == "o5m")
		enc.reset(new class O5mEncode(*outbuff));
	else if ((filePart > -1 and outFilenameSplit[filePart] == "osm") or consoleMode)
		enc.reset(new class OsmXmlEncode(*outbuff, customAttribs));
	else
		throw runtime_error("Output file extension not supported");

	//Prepare input
	bool consoleInput = false;
	std::streambuf *inbuff = nullptr;
	string inFormat = "";
	if(inputFiles[0] != "-")
	{
		std::filebuf *infb = new std::filebuf;
		infb->open(inputFiles[0], std::ios::in | std::ios::binary);
		if(!infb->is_open())
		{
			cerr << "Error opening input file " << inputFiles[0] << endl;
			exit(0);
		}

		vector<string> inFilenameSplit = split(inputFiles[0], '.');
		int64_t filePart2 = (int64_t)(inFilenameSplit.size()-1);
		if(formatInO5m or inFilenameSplit[filePart2] == "o5m")
			inFormat = "o5m";
		else if (formatInOsm or inFilenameSplit[filePart2] == "osm")
			inFormat = "osm";
		else
			throw runtime_error("Input file extension not supported");
	}
	else
	{
		consoleInput = true;
		inbuff = std::cin.rdbuf();

		if(formatInO5m)
			inFormat = "o5m";
		else
			inFormat = "osm";
	}

	//Run decoder
	if(inFormat != "")
	{
		if(inFormat == "o5m")
			LoadFromO5m(*inbuff, enc);
		else if (inFormat == "osm")
			LoadFromOsmXml(*inbuff, enc);
		else
			throw runtime_error("Input file extension not supported");
	}
	else
	{
		throw runtime_error("Input file extension not specified");
	}

	//Tidy up. It is a good idea to delete the pipeline in order.
	if(!consoleInput)
	{
		delete inbuff;
		inbuff = nullptr;
	}
	enc.reset();
	if(!consoleMode)
	{
		delete outbuff;
		outbuff = nullptr;
	}
}

