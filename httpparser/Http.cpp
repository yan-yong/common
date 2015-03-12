#include "Http.hpp"

#include <string.h>
#include <memory.h>
#include <zlib.h>
#include <stdio.h>

//////////////////////////////////////////////////////////////////////////
// Http common utilities

const char* GetReasonPhrase(int status_code, const char* unmatched_text)
{
	switch (status_code)
	{
	// Informational 1xx
	case 100: return "Continue";
	case 101: return "Switching Protocols";

	//Successful 2xx
	case 200: return "OK";
	case 201: return "Created";
	case 202: return "Accepted";
	case 203: return "Non-Authoritative Information";
	case 204: return "No Content";
	case 205: return "Reset Content";
	case 206: return "Partial Content";

	// Redirection 3xx
	case 300: return "Multiple Choices";
	case 301: return "Moved Permanently";
	case 302: return "Found";
	case 303: return "See Other";
	case 304: return "Not Modified";
	case 305: return "Use Proxy";
	case 306: return "(Unused)";
	case 307: return "Temporary Redirect";

	// Client Error 4xx
	case 400: return "Bad Request";
	case 401: return "Unauthorized";
	case 402: return "Payment Required";
	case 403: return "Forbidden";
	case 404: return "Not Found";
	case 405: return "Method Not Allowed";
	case 406: return "Not Acceptable";
	case 407: return "Proxy Authentication Required";
	case 408: return "Request Timeout";
	case 409: return "Conflict";
	case 410: return "Gone";
	case 411: return "Length Required";
	case 412: return "Precondition Failed";
	case 413: return "Request Entity Too Large";
	case 414: return "Request-URI Too Long";
	case 415: return "Unsupported Media Type";
	case 416: return "Requested Range Not Satisfiable";
	case 417: return "Expectation Failed";

	// Server Error 5xx
	case 500: return "Internal Server Error";
	case 501: return "Not Implemented";
	case 502: return "Bad Gateway";
	case 503: return "Service Unavailable";
	case 504: return "Gateway Timeout";
	case 505: return "HTTP Version Not Supported";
	}

	return unmatched_text;
}

static locale_t GetCLocale()
{
	static locale_t locate = newlocale(LC_ALL, "C", 0);
	return locate;
}

bool ParseTime(const char* str, time_t& time)
{
	locale_t c_locate = GetCLocale();
	struct tm tm;
	if (strptime_l(str, "%c", &tm, c_locate))
	{
		time = mktime(&tm);
	}
	else if (strptime_l(str, "%a, %d %b %Y %H:%M:%S %Z", &tm, c_locate) || // rfc1123-date
			strptime_l(str, "%a, %d-%b-%y %H:%M:%S %Z", &tm, c_locate)) // rfc950-date
	{
		time_t t = mktime(&tm);
		time = t + 8*3600; // convert GMT to local time.
	}
	else
	{
		return false;
	}

	return true;
}

size_t FormatTime(time_t time, char* str, size_t str_length)
{
	struct tm tm;
	gmtime_r(&time, &tm);
	return strftime_l(str, str_length, "%a, %d %b %Y %H:%M:%S GMT", &tm, GetCLocale());
}

bool FormatTime(time_t time, std::string& str)
{
	char buf[sizeof("Wed, 27 Feb 2008 08:25:36 GMT")];
	size_t length = FormatTime(time, buf, sizeof(buf));
	if (length)
	{
		str.assign(buf, length);
		return true;
	}
	return false;
}

size_t ExtractChunkedData(const void* buffer, size_t buffer_size, const void*& data, size_t& data_size)
{
	size_t parsed_size = 0;
	//data_size = 0;	
	const char* begin = reinterpret_cast<const char*>(buffer);
	const char* p = reinterpret_cast<const char*>(memchr(begin, '\n', buffer_size));
	if (p)
	{
		unsigned int size = 0;
		if (sscanf(begin, "%x", &size) == 1)
		{
			size_t head_skip = p - begin + 1;
			if (head_skip + size < buffer_size)
			{
				const char* data_begin = begin + head_skip;
				const char* data_end = data_begin + size;
				p = reinterpret_cast<const char*>(memchr(data_end, '\n', buffer_size - head_skip - size));
				if (p)
				{
					size_t tail_skip = p - data_end + 1;
					data = data_begin;
					data_size = size;
					parsed_size = head_skip + data_size + tail_skip;
				}
			}
		}
	}

	return parsed_size;
}

size_t ChunkedDecode(char *page, size_t size)
{
	char *rpos = page, *wpos = page;
	char *end = page + size;
	char *pos;
	int len;

	while ((pos = reinterpret_cast<char*>(memchr(rpos, '\n', end - rpos))) !=  NULL)
	{
		*pos++ = '\0';
		if (sscanf(rpos, "%x", &len) > 0)
		{
			if (len > 0)
			{
				if (pos + len > end)
					len = end - pos;
				memmove(wpos, pos, len);
				rpos = pos + len;
				wpos += len;
				continue;
			}
			else if (len == 0)
				break;
		}

		rpos = pos;
	}

	return wpos - page;
}

static int InflateData(z_stream& stream, const void *src, size_t src_length, std::vector<char>& result)
{
	const int DEFAULT_RATE = 5;
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<void*>(src));
    stream.avail_in = src_length;

	result.clear();
	result.resize(DEFAULT_RATE * src_length);

	for (;;)
	{
		stream.next_out = (Bytef*)&result[stream.total_out];
		stream.avail_out = result.size() - stream.total_out;

		int code = inflate(&stream, Z_NO_FLUSH);

		if (code == Z_STREAM_END)
			break;

		// if uncompress complete, ignore error
		if (stream.avail_in == 0)
			break;

		if (code < 0 || code == Z_NEED_DICT)
			return code;

		int rate = stream.total_in ? stream.total_out / stream.total_in : DEFAULT_RATE;
		result.resize(result.size() + (rate + 2) * stream.avail_in);
	}

	result.resize(stream.total_out);

    return Z_OK;
}

int GzipUncompress(const void *src, size_t src_length, std::vector<char>& result)
{
    z_stream stream = { 0 };

    int code = inflateInit2(&stream, 15 + 16);
    if (code == Z_OK)
    {
		code = InflateData(stream, src, src_length, result);
        inflateEnd(&stream);
    }

    return code;
}

int DeflateUncompress(const void* src, size_t src_length, std::vector<char>& result)
{
    z_stream stream = { 0 };
	int code = inflateInit(&stream);
    if (code == Z_OK && src_length > 1)
    {
		// See mozilla\netwerk\streamconv\converters\nsHTTPCompressConv.cpp

		// some servers (notably Apache with mod_deflate) don't generate zlib headers
		// insert a dummy header and try again
		static const unsigned char header[2] = { 0x78, 0x9C };

		if (memcmp(src, header, sizeof(header)) != 0)
		{
			stream.next_in = (Bytef*) header;
			stream.avail_in = sizeof(header);

			unsigned char dummy_output[1];
			stream.next_out = dummy_output;
			stream.avail_out = 0;

			code = inflate(&stream, Z_NO_FLUSH);
			if (code == Z_OK)
				code = InflateData(stream, src, src_length, result);
		}
		else
		{
			code = InflateData(stream, src, src_length, result);
		}

		inflateEnd(&stream);
    }

	return code;
}
