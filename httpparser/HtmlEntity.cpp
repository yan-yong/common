#include <climits>
#include <map>
#include <string>

#include "HtmlEntity.hpp"

static const std::map<std::string, wint_t>& GetEntityNameValueMap()
{
	static const std::pair<const char*, wint_t> init_table[] =
	{
		std::pair<const char*, wint_t>("quot", 34 ),
		std::pair<const char*, wint_t>("apos", 39),
		std::pair<const char*, wint_t>("amp", 38),
		std::pair<const char*, wint_t>("lt", 60),
		std::pair<const char*, wint_t>("gt", 62),
		std::pair<const char*, wint_t>("nbsp", 160),
		std::pair<const char*, wint_t>("iexcl", 161),
		std::pair<const char*, wint_t>("curren", 164),
		std::pair<const char*, wint_t>("cent", 162),
		std::pair<const char*, wint_t>("pound", 163),
		std::pair<const char*, wint_t>("yen", 165),
		std::pair<const char*, wint_t>("brvbar", 166),
		std::pair<const char*, wint_t>("sect", 167),
		std::pair<const char*, wint_t>("uml", 168),
		std::pair<const char*, wint_t>("copy", 169),
		std::pair<const char*, wint_t>("ordf", 170),
		std::pair<const char*, wint_t>("laquo", 171),
		std::pair<const char*, wint_t>("not", 172),
		std::pair<const char*, wint_t>("shy", 173),
		std::pair<const char*, wint_t>("reg", 174),
		std::pair<const char*, wint_t>("trade", 8482),
		std::pair<const char*, wint_t>("macr", 175),
		std::pair<const char*, wint_t>("deg", 176),
		std::pair<const char*, wint_t>("plusmn", 177),
		std::pair<const char*, wint_t>("sup2", 178),
		std::pair<const char*, wint_t>("sup3", 179),
		std::pair<const char*, wint_t>("acute", 180),
		std::pair<const char*, wint_t>("micro", 181),
		std::pair<const char*, wint_t>("para", 182),
		std::pair<const char*, wint_t>("middot", 183),
		std::pair<const char*, wint_t>("cedil", 184),
		std::pair<const char*, wint_t>("sup1", 185),
		std::pair<const char*, wint_t>("ordm", 186),
		std::pair<const char*, wint_t>("raquo", 187),
		std::pair<const char*, wint_t>("frac14", 188),
		std::pair<const char*, wint_t>("frac12", 189),
		std::pair<const char*, wint_t>("frac34", 190),
		std::pair<const char*, wint_t>("iquest", 191),
		std::pair<const char*, wint_t>("times", 215),
		std::pair<const char*, wint_t>("divide", 247),
		std::pair<const char*, wint_t>("Agrave", 192),
		std::pair<const char*, wint_t>("Aacute", 193),
		std::pair<const char*, wint_t>("Acirc", 194),
		std::pair<const char*, wint_t>("Atilde", 195),
		std::pair<const char*, wint_t>("Auml", 196),
		std::pair<const char*, wint_t>("Aring", 197),
		std::pair<const char*, wint_t>("AElig", 198),
		std::pair<const char*, wint_t>("Ccedil", 199),
		std::pair<const char*, wint_t>("Egrave", 200),
		std::pair<const char*, wint_t>("Eacute", 201),
		std::pair<const char*, wint_t>("Ecirc", 202),
		std::pair<const char*, wint_t>("Euml", 203),
		std::pair<const char*, wint_t>("Igrave", 204),
		std::pair<const char*, wint_t>("Iacute", 205),
		std::pair<const char*, wint_t>("Icirc", 206),
		std::pair<const char*, wint_t>("Iuml", 207),
		std::pair<const char*, wint_t>("ETH", 208),
		std::pair<const char*, wint_t>("Ntilde", 209),
		std::pair<const char*, wint_t>("Ograve", 210),
		std::pair<const char*, wint_t>("Oacute", 211),
		std::pair<const char*, wint_t>("Ocirc", 212),
		std::pair<const char*, wint_t>("Otilde", 213),
		std::pair<const char*, wint_t>("Ouml", 214),
		std::pair<const char*, wint_t>("Oslash", 216),
		std::pair<const char*, wint_t>("Ugrave", 217),
		std::pair<const char*, wint_t>("Uacute", 218),
		std::pair<const char*, wint_t>("Ucirc", 219),
		std::pair<const char*, wint_t>("Uuml", 220),
		std::pair<const char*, wint_t>("Yacute", 221),
		std::pair<const char*, wint_t>("THORN", 222),
		std::pair<const char*, wint_t>("szlig", 223),
		std::pair<const char*, wint_t>("agrave", 224),
		std::pair<const char*, wint_t>("aacute", 225),
		std::pair<const char*, wint_t>("acirc", 226),
		std::pair<const char*, wint_t>("atilde", 227),
		std::pair<const char*, wint_t>("auml", 228),
		std::pair<const char*, wint_t>("aring", 229),
		std::pair<const char*, wint_t>("aelig", 230),
		std::pair<const char*, wint_t>("ccedil", 231),
		std::pair<const char*, wint_t>("egrave", 232),
		std::pair<const char*, wint_t>("eacute", 233),
		std::pair<const char*, wint_t>("ecirc", 234),
		std::pair<const char*, wint_t>("euml", 235),
		std::pair<const char*, wint_t>("igrave", 236),
		std::pair<const char*, wint_t>("iacute", 237),
		std::pair<const char*, wint_t>("icirc", 238),
		std::pair<const char*, wint_t>("iuml", 239),
		std::pair<const char*, wint_t>("eth", 240),
		std::pair<const char*, wint_t>("ntilde", 241),
		std::pair<const char*, wint_t>("ograve", 242),
		std::pair<const char*, wint_t>("oacute", 243),
		std::pair<const char*, wint_t>("ocirc", 244),
		std::pair<const char*, wint_t>("otilde", 245),
		std::pair<const char*, wint_t>("ouml", 246),
		std::pair<const char*, wint_t>("oslash", 248),
		std::pair<const char*, wint_t>("ugrave", 249),
		std::pair<const char*, wint_t>("uacute", 250),
		std::pair<const char*, wint_t>("ucirc", 251),
		std::pair<const char*, wint_t>("uuml", 252),
		std::pair<const char*, wint_t>("yacute", 253),
		std::pair<const char*, wint_t>("thorn", 254),
		std::pair<const char*, wint_t>("yuml", 255),
		std::pair<const char*, wint_t>("OElig", 338),
		std::pair<const char*, wint_t>("oelig", 339),
		std::pair<const char*, wint_t>("Scaron", 352),
		std::pair<const char*, wint_t>("scaron", 353),
		std::pair<const char*, wint_t>("Yuml", 376),
		std::pair<const char*, wint_t>("circ", 710),
		std::pair<const char*, wint_t>("tilde", 732),
		std::pair<const char*, wint_t>("ensp", 8194),
		std::pair<const char*, wint_t>("emsp", 8195),
		std::pair<const char*, wint_t>("thinsp", 8201),
		std::pair<const char*, wint_t>("zwnj", 8204),
		std::pair<const char*, wint_t>("zwj", 8205),
		std::pair<const char*, wint_t>("lrm", 8206),
		std::pair<const char*, wint_t>("rlm", 8207),
		std::pair<const char*, wint_t>("ndash", 8211),
		std::pair<const char*, wint_t>("mdash", 8212),
		std::pair<const char*, wint_t>("lsquo", 8216),
		std::pair<const char*, wint_t>("rsquo", 8217),
		std::pair<const char*, wint_t>("sbquo", 8218),
		std::pair<const char*, wint_t>("ldquo", 8220),
		std::pair<const char*, wint_t>("rdquo", 8221),
		std::pair<const char*, wint_t>("bdquo", 8222),
		std::pair<const char*, wint_t>("dagger", 8224),
		std::pair<const char*, wint_t>("Dagger", 8225),
		std::pair<const char*, wint_t>("hellip", 8230),
		std::pair<const char*, wint_t>("permil", 8240),
		std::pair<const char*, wint_t>("lsaquo", 8249),
		std::pair<const char*, wint_t>("rsaquo", 8250),
		std::pair<const char*, wint_t>("euro", 8364), 
	};

	static std::map<std::string, wint_t> the_map(
		&init_table[0],
		init_table + sizeof(init_table) / sizeof(init_table[0])
	);

	return the_map;
}

int EntityDecode(const char* begin, size_t length, wint_t& wc)
{
	const char* p = begin;
	const char* end = begin + length;

	if (++p >= end)
		return 0;

	if (*p == '#')
	{
		if (++p >= end)
			return 0;

		wc = 0;

		if (*p == 'x' || *p == 'X')
		{
			if (++p >= end)
				return 0;

			while (isxdigit(*p))
			{
				wc *= 16;
				if (isupper(*p))
					wc += *p - 'A' + 10;
				else if (islower(*p))
					wc += *p - 'a' + 10;
				else
					wc += *p - '0';

				if (++p >= end)
					return 0;
			}
		}
		else if (isdigit(*p))
		{
			do
			{
				wc *= 10;
				wc += *p - '0';
				if (++p >= end)
					return 0;
			}
			while (isdigit(*p));
		}

		if (*p == ';')
		{
			return p - begin + 1;
		}
	}
	else if (isalpha(*p))
	{
		const char* name_begin = p;
		do
		{
			++p;
		}
		while (isalnum(*p));

		if (*p == ';')
		{
			std::string name(name_begin, p);
			const std::map<std::string, wint_t>& entity_map = GetEntityNameValueMap();
			std::map<std::string, wint_t>::const_iterator i = entity_map.find(name);
			if (i != entity_map.end())
			{
				wc = i->second;
				return p - begin + 1;
			}
		}
	}

	// invalid entity
	return -1;
}

int EntityDecode(char* text, size_t length)
{
	const char* p = text;
	const char* end = text + length;

	char* output = text;

	while (p < end)
	{
		wint_t wc;
		int l;
		if (p[0] == '&' && (l = EntityDecode(p, end - p, wc)) > 0)
		{
			if (wc > UCHAR_MAX)
				return -1;
			*output++ = wc;
			p += l;
		}
		else
		{
			*output++ = *p++;
		}
	}

	*output = '\0';
	return output - text;
}

bool EntityDecode(std::string& text)
{
	size_t input = 0;
	size_t output = 0;
	size_t length = text.length();

	while (input < length)
	{
		wint_t wc;
		int l;
		if (text[input] == '&' && (l = EntityDecode(text.data() + input, length - input, wc)) > 0)
		{
			if (wc > UCHAR_MAX)
				return false;
			text[output++] = wc;
			input += l;
		}
		else
		{
			text[output++] = text[input++];
		}
	}

	text.resize(output);
	return true;
}

bool EntityDecode(const char* text, size_t length, std::string& result)
{
	result.clear();
	result.reserve(length);
	const char* p = text;
	const char* end = text + length;
	while (p < end)
	{
		wint_t wc;
		int l;
		if (p[0] == '&' && (l = EntityDecode(p, end - p, wc)) > 0)
		{
			if (wc > UCHAR_MAX)
				return false;
			result.push_back(wc);
			p += l;
		}
		else
		{
			result.push_back(*p);
			++p;
		}
	}

	return true;
}

