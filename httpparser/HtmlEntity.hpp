#ifndef HTML_ENTITY_HPP_INCLUDED
#define HTML_ENTITY_HPP_INCLUDED

#include <string>
#include <utility>

#include <string.h>
#include <wchar.h>

/// decode one entity character, lower level API
int EntityDecode(const char* begin, size_t length, wint_t& wc);

//////////////////////////////////////////////////////////////////////////
/// decode string

// inplace decode
int EntityDecode(char* text, size_t length);
bool EntityDecode(std::string& text);

bool EntityDecode(const char* text, size_t length, std::string& result);

inline bool EntityDecode(const char* text, std::string& result)
{
	return EntityDecode(text, strlen(text), result);
}

inline bool EntityDecode(const std::string& text, std::string& result)
{
	if (&text == &result)
	{
		return EntityDecode(result);
	}
	else
	{
		return EntityDecode(text.data(), text.size(), result);
	}
}

#endif//HTML_ENTITY_HPP_INCLUDED

