#ifndef _UTILS_H
#define _UTILS_H

#include "OsmData.h"

// Convenience functions: load and save from std::streambuf

void LoadFromO5m(std::streambuf &fi, std::shared_ptr<class IDataStreamHandler> output);
void LoadFromOsmXml(std::streambuf &fi, std::shared_ptr<class IDataStreamHandler> output);
void LoadFromPbf(std::streambuf &fi, std::shared_ptr<class IDataStreamHandler> output);
void LoadFromOsmChangeXml(std::streambuf &fi, std::shared_ptr<class IOsmChangeBlock> output);

void SaveToO5m(const class OsmData &osmData, std::streambuf &fi);
void SaveToOsmXml(const class OsmData &osmData, std::streambuf &fi);
void SaveToOsmChangeXml(const class OsmChange &osmChange, bool separateActions, std::streambuf &fi);

// Convenience functions: load from std::string

void LoadFromO5m(const std::string &fi, std::shared_ptr<class IDataStreamHandler> output);
void LoadFromOsmXml(const std::string &fi, std::shared_ptr<class IDataStreamHandler> output);
void LoadFromOsmChangeXml(const std::string &fi, std::shared_ptr<class IOsmChangeBlock> output);

#endif //_UTILS_H

