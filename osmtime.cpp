#include "osmtime.h"
#include <cstring>
#include <ctime>
#include "decoder.h"
extern "C" {
#include "iso8601lib/iso8601.h"
}

int64_t ParseOsmTimestamp(const char *text)
{
	struct tm dt;
	memset(&dt, 0, sizeof(dt));
	int timezoneOffsetMin = 0;
	if(!ParseIso8601Datetime(text, &dt, &timezoneOffsetMin))
		throw OsmDecodeError(std::string("Invalid timestamp: ") + text);
	TmToUtc(&dt, timezoneOffsetMin);
	return (int64_t)timegm(&dt);
}

bool AppendOsmTimestamp(int64_t timestamp, std::string &out)
{
	time_t tt = timestamp;
	char buf[50];
	struct tm tmbuf;
	if(gmtime_r(&tt, &tmbuf) == nullptr)
		return false;
	//Only four digit years can be written in a form every reader, including
	//this library's, will parse. Binary formats can hold any number.
	int year = tmbuf.tm_year + 1900;
	if(year < 1000 || year > 9999)
		return false;
	if(strftime(buf, sizeof(buf), "%FT%TZ", &tmbuf) == 0)
		return false;
	out.append(buf);
	return true;
}
