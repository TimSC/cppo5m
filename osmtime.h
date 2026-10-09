#ifndef CPPO5M_OSMTIME_H
#define CPPO5M_OSMTIME_H

// Timestamps as the text formats write them. Internal to the library.

#include <cstdint>
#include <string>

///Parses an ISO 8601 date and time into seconds since the Unix epoch. Throws
///OsmDecodeError if the text is not a valid timestamp.
int64_t ParseOsmTimestamp(const char *text);

///Appends a timestamp as, for example, 2020-01-02T03:04:05Z. Returns false,
///appending nothing, if the value cannot be represented as a calendar date.
bool AppendOsmTimestamp(int64_t timestamp, std::string &out);

#endif //CPPO5M_OSMTIME_H
