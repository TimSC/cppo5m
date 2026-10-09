//Conversion utility, modelled after osmconvert
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "../cppo5m.h"
#include "../pbf/osmformat.pb.h"
using namespace std;

static const char *usage =
	"Convert between osm, o5m and pbf file formats\n"
	"Usage: o5mconvert INPUT [-o OUTPUT] [options]\n"
	"  INPUT             input file, or - for console stream\n"
	"  -o, --output FILE output file (console if omitted)\n"
	"  --in-osm          input file format is osm\n"
	"  --in-o5m          input file format is o5m\n"
	"  --in-pbf          input file format is pbf\n"
	"  --out-osm         output file format is osm\n"
	"  --out-o5m         output file format is o5m\n"
	"  --out-pbf         output file format is pbf\n"
	"  --out-null        do not write output\n"
	"  --sort            sort output by ID (memory intensive)\n"
	"  --help            show this message\n";

int main(int argc, char* argv[])
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	std::ios::sync_with_stdio(false);

	vector<string> inputFiles;
	string outputFile;
	bool formatInOsm = false, formatInO5m = false, formatInPbf = false;
	bool formatOutOsm = false, formatOutO5m = false, formatOutPbf = false;
	bool formatOutNull = false, sort = false;

	for(int i=1; i<argc; i++)
	{
		string arg = argv[i];
		if(arg == "--help" || arg == "-h") { cout << usage; return 0; }
		else if(arg == "--in-osm") formatInOsm = true;
		else if(arg == "--in-o5m") formatInO5m = true;
		else if(arg == "--in-pbf") formatInPbf = true;
		else if(arg == "--out-osm") formatOutOsm = true;
		else if(arg == "--out-o5m") formatOutO5m = true;
		else if(arg == "--out-pbf") formatOutPbf = true;
		else if(arg == "--out-null") formatOutNull = true;
		else if(arg == "--sort") sort = true;
		else if(arg == "-o" || arg == "--output")
		{
			if(i + 1 >= argc)
			{
				cerr << arg << " needs a file name" << endl;
				return 2;
			}
			outputFile = argv[++i];
		}
		else if(arg.compare(0, 9, "--output=") == 0)
			outputFile = arg.substr(9);
		else if(arg.compare(0, 2, "-o") == 0 && arg.size() > 2 && arg[1] == 'o')
			outputFile = arg.substr(arg[2] == '=' ? 3 : 2);
		else if(arg == "-" || arg[0] != '-')
			inputFiles.push_back(arg);
		else
		{
			cerr << "Unknown option: " << arg << endl << usage;
			return 2;
		}
	}

	if(inputFiles.size() != 1)
	{
		cerr << "Specify exactly one input file." << endl << usage;
		return 2;
	}
	if(formatInOsm + formatInO5m + formatInPbf > 1 || formatOutOsm + formatOutO5m + formatOutPbf + formatOutNull > 1)
	{
		cerr << "Specify at most one input format and one output format." << endl;
		return 2;
	}

	try
	{
		//Work out the formats; an explicit option beats the file extension
		const string &inputFile = inputFiles[0];
		bool consoleInput = inputFile == "-";
		OsmFormat inFormat = OsmFormat::OsmXml;
		if(formatInO5m) inFormat = OsmFormat::O5m;
		else if(formatInPbf) inFormat = OsmFormat::Pbf;
		else if(formatInOsm) inFormat = OsmFormat::OsmXml;
		else if(!consoleInput) inFormat = FormatFromFilename(inputFile);

		bool consoleOutput = outputFile.empty();
		OsmFormat outFormat = OsmFormat::OsmXml;
		if(formatOutO5m) outFormat = OsmFormat::O5m;
		else if(formatOutPbf) outFormat = OsmFormat::Pbf;
		else if(formatOutOsm) outFormat = OsmFormat::OsmXml;
		else if(!consoleOutput && !formatOutNull) outFormat = FormatFromFilename(outputFile);

		std::filebuf infb, outfb;
		std::streambuf *inbuff = std::cin.rdbuf();
		if(!consoleInput)
		{
			if(!infb.open(inputFile, std::ios::in | std::ios::binary))
				throw runtime_error("Error opening input file " + inputFile);
			inbuff = &infb;
		}
		std::streambuf *outbuff = std::cout.rdbuf();
		if(!consoleOutput && !formatOutNull)
		{
			if(!outfb.open(outputFile, std::ios::out | std::ios::binary))
				throw runtime_error("Error opening output file " + outputFile);
			outbuff = &outfb;
		}

		//Build the pipeline from the output end backwards
		IDataStreamHandler nullHandler;
		std::unique_ptr<OsmEncoder> encoder;
		IDataStreamHandler *pipeline = &nullHandler;
		if(!formatOutNull)
		{
			encoder = MakeEncoder(outFormat, *outbuff);
			pipeline = encoder.get();
		}
		std::unique_ptr<SortOsm> sorter;
		if(sort)
		{
			sorter.reset(new SortOsm(*pipeline));
			pipeline = sorter.get();
		}

		std::unique_ptr<OsmDecoder> decoder = MakeDecoder(inFormat, *inbuff, *pipeline);
		decoder->Decode();

		if(outbuff == &outfb && outfb.close() == nullptr)
			throw runtime_error("Error writing output file " + outputFile);
		std::cout.flush();
	}
	catch(const std::exception &err)
	{
		cerr << "Error: " << err.what() << endl;
		return 1;
	}
	return 0;
}
