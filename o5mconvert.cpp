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
	vector<string> inputFiles;
	string outputFile;
	po::options_description desc("Convert between osm, o5m file formats");
	desc.add_options()
		("help",                                                                 "show help message")
		("input",  po::value< vector<string> >(&inputFiles),                     "input file")
		("output,o", po::value< string >(&outputFile),                           "output file")
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
	
	std::filebuf infb;
	infb.open(inputFiles[0], std::ios::in | std::ios::binary);
	if(!infb.is_open())
	{
		cerr << "Error opening input file " << inputFiles[0] << endl;
		exit(0);
	}

	std::streambuf *outbuff;
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

	if(filePart > -1 and outFilenameSplit[filePart] == "o5m")
		enc.reset(new class O5mEncode(*outbuff));
	else if ((filePart > -1 and outFilenameSplit[filePart] == "osm") or consoleMode)
		enc.reset(new class OsmXmlEncode(*outbuff, customAttribs));
	else
		throw runtime_error("Output file extension not supported");

	//Run decoder
	vector<string> inFilenameSplit = split(inputFiles[0], '.');
	int64_t filePart2 = (int64_t)(inFilenameSplit.size()-1);

	if(filePart2 > -1)
	{
		if(inFilenameSplit[filePart2] == "o5m")
			LoadFromO5m(infb, enc);
		else if (inFilenameSplit[filePart2] == "osm")
			LoadFromOsmXml(infb, enc);
		else
			throw runtime_error("Input file extension not supported");
	}
	else
	{
		throw runtime_error("Input file extension not specified");
	}

	//Tidy up
	enc.reset();
	if(!consoleMode)
	{
		delete outbuff;
		outbuff = nullptr;
	}
}

