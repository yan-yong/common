#ifndef HTTP_HPP_INCLUDED
#define HTTP_HPP_INCLUDED

#include <cstddef>
#include <string>

#pragma once

#include "HttpMessage.hpp"
#include "HttpMessageParser.hpp"

const int HTTP_DEFAULT_PORT = 80;
const int HTTPS_DEFAULT_PORT = 443;

#define HTTP_DEFAULT_PORT_STR "80"
#define HTTPS_DEFAULT_PORT_STR "443"

const char* GetReasonPhrase(int status_code, const char* unmatched_text);

inline const char* GetReasonPhrase(int status_code)
{
	return GetReasonPhrase(status_code, NULL);
}

inline const char* GetReasonPhraseSafe(int status_code)
{
	return GetReasonPhrase(status_code, "");
}

bool ParseTime(const char* str, time_t& time);
inline bool ParseTime(const std::string& str, time_t& time)
{
	return ParseTime(str.c_str(), time);
}

size_t FormatTime(time_t time, char* str, size_t str_length);
bool FormatTime(time_t time, std::string& str);

size_t ExtractChunkedData(const void* buffer, size_t buffer_size, const void*& data, size_t& data_size);

size_t ChunkedDecode(char *page, size_t size);

int GzipUncompress(const void *src, size_t src_length, std::vector<char>& result);

int DeflateUncompress(const void* src, size_t src_length, std::vector<char>& result);

#endif//HTTP_HPP_INCLUDED
