#include <string>

#undef __DEPRECATED
#include <strstream>

#include <vector>
#include "FastCType.hpp"
#include "URI.hpp"
#include "HtmlEntity.hpp"

namespace
{
const CharSet& IsAlpha = GetAlphaSet();
const CharSet& IsUpper = GetUpperSet();
const CharSet& IsLower = GetLowerSet();
const CharSet& IsDigit = GetDigitSet();
const CharSet& IsAlphaNum = GetAlphaNumSet();
const CharSet& IsPrint = GetPrintSet();
const CharSet& IsSpace = GetSpaceSet();
const CharSet& IsHex = GetHexSet();
}

void UriEscape(const char* begin, const char* end, std::string& dest)
{
	dest.clear();
	const char* i = begin;
	while (i != end)
	{
		if (!IsPrint(*i) || IsSpace(*i))
		{
			dest.append(begin, i - begin);

			static const char hex[] = "0123456789ABCDEF";
			dest += '%';
			unsigned char c = *i;
			dest += hex[c >> 4];
			dest += hex[c & 0x0F];
			begin = i + 1;
		}
		++i;
	}
	dest.append(begin, end - begin);
}

void UriEscape(const std::string& src, std::string& dest)
{
	if (&src == &dest)
	{
		std::string tmp;
		UriEscape(src.begin(), src.end(), tmp);
		std::swap(tmp, dest);
	}
	else
	{
		UriEscape(src.begin(), src.end(), dest);
	}
}

void UriEscape(std::string& uri)
{
	std::string tmp;
	UriEscape(uri.begin(), uri.end(), tmp);
	std::swap(uri, tmp);
}

void UrlUniform(const std::string url, std::string& result)
{
    // skip leading white spaces
    size_t length = url.length();
    size_t begin = 0;
    while (begin < length && isspace(url[begin]))
	++begin;
    size_t end = length;
    while (end > begin && isspace(url[end - 1]))
	--end;
    UriEscape(url.data() + begin, url.data() + end, result);
    EntityDecode(result);

    // Some url has space char in EntityEncoded format.
    length = result.length();
    for(begin = 0; begin < length && isspace(result[begin]); begin++){}
    for(end = length - 1; end > begin && isspace(result[end]); end--){}

    if(end < length - 1){
	result.erase(end+1, length-1-end);
    }

    if(begin > 0){
	result.erase(0, begin);
    }
}

// see RFC 2396
class UriParser
{
	friend class result;
	class result
	{
	public:
		result(UriParser& parser) : m_parser(parser), m_begin(parser.m_current), m_result(false){}
		~result()
		{ 
			if (!m_result) 
				m_parser.m_current = m_begin;
		}
		operator bool() const
		{
			return m_result;
		}
		result& operator=(bool value)
		{
			m_result = value;
			return *this;
		}
		const char* begin() const
		{
			return m_begin;
		}
		const char* end() const
		{
			return m_parser.m_current;
		}
		size_t length() const
		{
			return m_parser.m_current - m_begin;
		}
	private:
		result(const result&);
		result& operator=(const result&);
	private:
		UriParser& m_parser;
		const char* m_begin;
		bool m_result;
	};
public:
	UriParser() : 
		m_begin(NULL),
		m_end(NULL),
		m_current(NULL),
		m_result(NULL)
	{
	}

	std::string UniformSlash(const std::string& url)
	{
		//change '\' to '/' in url
		std::string result(url);
		for (size_t i = 0; i < url.length(); ++i) {
			if (url[i] == '\\') {
				result[i] = '/';
			}
		}
		return result;
	}

	size_t Parse(const char* Uri, size_t UriLength, URI& Result)
	{
		std::string url = UniformSlash(std::string(Uri, UriLength));
		m_begin = url.c_str();
		m_current = url.c_str();
		m_end = url.c_str() + UriLength;
		m_result = &Result;
		match_URI_reference();
		return m_current - m_begin;
	}

private:
	// URI-reference = [ absoluteURI | relativeURI ] [ "#" fragment ]
	bool match_URI_reference()
	{
		bool r = match_absoluteURI() || match_relativeURI();

		result r1(*this);
		r1 = match_literal('#') && match_fragment();
		if (r1)
			m_result->SetFragment(r1.begin() + 1, r1.length() - 1);
		return r || r1;
	}

	// absoluteURI   = scheme ":" ( hier_part | opaque_part )
	bool match_absoluteURI()
	{
		result r(*this);
		if(match_scheme() && match_literal(':'))
		{
			m_result->SetScheme(r.begin(), r.length() - 1);
			r = match_hier_part() || match_opaque_part();
		}
		return r;
	}

	// relativeURI   = ( net_path | abs_path | rel_path ) [ "?" query ]
	bool match_relativeURI()
	{
		result r(*this);
		r = match_net_path() || match_abs_path() || match_rel_path();
	//	if (r)
		{
			result r1(*this);
			r1 = match_literal('?') && match_query();
			if (r1)
			{
				m_result->SetQuery(r1.begin() + 1, r1.length() - 1);
				if(!r)
				{
					r = true;
				}
				return true;
			}
	//		return true;
		}
		if (r)
		{
			return true;
		}
		return false;
	}

	// hier_part     = ( net_path | abs_path ) [ "?" query ]
	bool match_hier_part()
	{
		result r(*this);
		r = match_net_path() || match_abs_path();
		if (r)
		{
			result r1(*this);
			r1 = match_literal('?') && match_query();
			if (r1)
				m_result->SetQuery(r1.begin() + 1, r1.length() - 1);
			return true;
		}
		return false;
	}

	// opaque_part   = uric_no_slash *uric
	bool match_opaque_part()
	{
		result r(*this);
		r = match_uric_no_slash();
		if (r)
		{
			while(match_uric())
			{
			}
			m_result->SetOpaquePart(r.begin(), r.length());
		}
		return r;
	}

	// uric_no_slash = unreserved | escaped | ";" | "?" | ":" | "@" | "&" | "=" | "+" | "$" | ","
	static const CharSet m_cs_uric_no_slash;
	bool match_uric_no_slash()
	{
		result r(*this);
		r = match_charset(m_cs_uric_no_slash) || match_escaped();
		return r;
	}

	// net_path      = "//" authority [ abs_path ]
	bool match_net_path()
	{
		result r(*this);
		r = match_literal("//") && match_authority();
		if (r)
			match_abs_path();
		return r;
	}

	// abs_path      = "/"  path_segments
	bool match_abs_path()
	{
		result r(*this);
		r = match_literal('/') && match_path_segments();
		if (r)
			m_result->SetPath(r.begin(), r.length());
		return r;
	}

	// rel_path      = rel_segment [ abs_path ]
	bool match_rel_path()
	{
		result r(*this);
		r = match_rel_segment();
		if (r)
		{
			match_abs_path();
			m_result->SetPath(r.begin(), r.length());
		}
		return r;
	}

	// rel_segment   = 1*( unreserved | escaped | ";" | "@" | "&" | "=" | "+" | "$" | "," )
	static const CharSet m_cs_rel_segment;
	bool match_rel_segment()
	{
		result r(*this);
		int n = 0;
		while(match_charset(m_cs_rel_segment) || match_escaped())
			++n;
		r = n > 0;
		return r;
	}

	// scheme        = alpha *( alpha | digit | "+" | "-" | "." )
	static const CharSet m_cs_scheme;
	bool match_scheme()
	{
		result r(*this);
		r = match_alpha();
		if (r)
		{
			while (match_charset(m_cs_scheme))
			{
			}
		}
		return r;
	}

	// authority     = server | reg_name
	bool match_authority()
	{
		result r(*this);
		r = match_server() || match_reg_name();
		return r;
	}

	// reg_name      = 1*( unreserved | escaped | "$" | "," | ";" | ":" | "@" | "&" | "=" | "+" )
	static const CharSet m_cs_reg_name;
	bool match_reg_name()
	{
		result r(*this);
		int n = 0;
		while (match_charset(m_cs_reg_name) || match_escaped());
			++n;
		r = n > 0;
		if (r)
			m_result->SetRegName(r.begin(), r.length());
		return r;
	}

	// server        = [ [ userinfo "@" ] hostport ]
	bool match_server()
	{
		{
			result r(*this);
			r = match_userinfo() && match_literal('@');
			if (r)
			{
				m_result->SetUserInfo(r.begin(), r.length() - 1);
				result r1(*this);
				r1 = match_hostport();
				return r1;
			}
		}
		result r1(*this);
		r1 = match_hostport();
		return true;
	}

	// userinfo      = *( unreserved | escaped | ";" | ":" | "&" | "=" | "+" | "$" | "," )
	static const CharSet m_cs_userinfo;
	bool match_userinfo()
	{
		while (match_charset(m_cs_userinfo) || match_escaped())
		{
		}
		return true;
	}

	// hostport      = host [ ":" port ]
	bool match_hostport()
	{
		result r(*this);
		r = match_host();
		if (r)
		{
			m_result->SetHost(r.begin(), r.length());
			result r1(*this);
			r1 = match_literal(':') && match_port();
			if (r1)
				m_result->SetPort(r1.begin() + 1, r1.length() - 1);
		}
		return r;
	}

	// host          = hostname | IPv4address
	bool match_host()
	{
		return match_hostname() || match_IPv4address();
	}

	// hostname      = *( domainlabel "." ) toplabel [ "." ]
	bool match_hostname()
	{
		result r(*this);
		for(;;)
		{
			result r1(*this);
			r1 = match_domainlabel() && match_literal('.');
			if (!r1)
				break;
		}
		r = match_toplabel();
		if (r)
			match_literal('.');
		return r;
	}

	// domainlabel   = alphanum | alphanum *( alphanum | "-" ) alphanum
	bool match_domainlabel()
	{
		//alphanum | alphanum *( alphanum | "-" ) alphanum
		result r(*this);
		if (match_alphanum())
		{
			result r1(*this);
			for (;;)
			{
				while (match_literal('-') || match_literal('_')) // acore 说要容错
//by tianwei: 明明写着acore说要容错，但是代码却被注释了，真是怪哉。现在明确一下，必须容错！允许site/domain中含有下划线。
//准确的说，domain是必须按照字母数字横线来搞的，但是domain下边的各个主机的名字，应该是随便搞。
				{
				}
				if (match_alphanum())
					r1 = true;
				else
					break;
			}
			r = true;
		}
		return r;
	}

	// toplabel      = alpha | alpha *( alphanum | "-" ) alphanum
	bool match_toplabel()
	{
		result r(*this);
		if (match_alpha())
		{
			result r1(*this);
			for (;;)
			{
				while (match_literal('-'))
				{
				}
				if (match_alphanum())
					r1 = true;
				else
					break;
			}
			r = true;
		}
		return r;
	}

	// IPv4address   = 1*digit "." 1*digit "." 1*digit "." 1*digit
	bool match_IPv4address()
	{
		result r(*this);
		for (int i = 0; i < 3; ++i)
		{
			if (match_digit())
			{
				while (match_digit())
				{
				}
				if (!match_literal('.'))
					return false;
			}
		}

		// last field, no following dot
		if (match_digit())
		{
			while (match_digit())
			{
			}
			r = true;
		}

		return r;
	}

	// port          = *digit
	bool match_port()
	{
		while (match_charset(IsDigit))
		{
		}
		return true;
	}

	// path          = [ abs_path | opaque_part ]
	bool match_path()
	{
		return match_abs_path() || match_opaque_part();
	}

	// path_segments = segment *( "/" segment )
	bool match_path_segments()
	{
		result r(*this);
		r = match_segment();
		if (r)
		{
			for (;;)
			{
				result r1(*this);
				r1 = match_literal('/') && match_segment();
				if (!r1)
					break;
			}
		}
		return r;
	}

	// segment       = *pchar *( ";" param )
	bool match_segment()
	{
		while(match_pchar())
		{
		}
		for (;;)
		{
			result r(*this);
			r = match_literal(';') &&  match_param();
			if (!r)
				break;
		}
		return true;
	}

	// param         = *pchar
	bool match_param()
	{
		while(match_pchar())
		{
		}
		return true;
	}

	// pchar         = unreserved | escaped | ":" | "@" | "&" | "=" | "+" | "$" | ","
	static const CharSet m_cs_pchar;
	bool match_pchar()
	{
		//return match_charset(m_cs_pchar) || match_escaped();
		return unmatch_charset(m_uri_reserved);
	}

	// query         = *uric
	bool match_query()
	{
		while(match_uric())
		{
		}
		return true;
	}

	// fragment      = *uric
	bool match_fragment()
	{
		while(match_uric())
		{
		}
		return true;
	}

	// uric          = reserved | unreserved | escaped
	static const CharSet m_cs_uric;
	bool match_uric()
	{
		//return match_charset(m_cs_uric) || match_escaped();
		return unmatch_charset(m_uri_reserved) || match_charset(m_cs_uric);
	}

	// reserved      = ";" | "/" | "?" | ":" | "@" | "&" | "=" | "+" | "$" | ","
	static const CharSet m_cs_reserved;
	bool match_reserved()
	{
		return match_charset(m_cs_reserved);
	}

	// unreserved    = alphanum | mark
	static const CharSet m_cs_unreserved;
	bool match_unreserved()
	{
		return match_charset(m_cs_unreserved);
	}

	static const CharSet m_uri_reserved;
	// mark          = "-" | "_" | "." | "!" | "~" | "*" | "'" | "(" | ")"
	static const CharSet m_cs_mark;
	bool match_mark()
	{
		return match_charset(m_cs_mark);
	}

	// escaped       = "%" hex hex
	bool match_escaped()
	{
		result r(*this);
		r = match_literal('%') && match_hex() && match_hex();
		return r;
	}

	bool match_hex()
	{
		return match_charset(IsHex);
	}

	bool match_alphanum()
	{
		return match_charset(IsAlphaNum);
	}

	bool match_alpha()
	{
		return match_charset(IsAlpha);
	}

	bool match_lowalpha()
	{
		return match_charset(IsLower);
	}

	bool match_upalpha()
	{
		return match_charset(IsUpper);
	}

	bool match_digit()
	{
		return match_charset(IsDigit);
	}

	bool match_literal(char c)
	{
		if (m_current < m_end && *m_current == c)
		{
			++m_current;
			return true;
		}
		return false;
	}

	template <size_t N>
	bool match_literal(const char (&l)[N])
	{
		const ptrdiff_t w = N - 1;
		if (m_end - m_current > w && memcmp(m_current, l, w)==0)
		{
			m_current += w;
			return true;
		}
		return false;
	}

	template <typename Pred>
	bool match_charset(Pred cs)
	{
		if (m_current < m_end && cs(*m_current))
		{
			++m_current;
			return true;
		}
		return false;
	}

	template <typename Pred>
	bool unmatch_charset(Pred cs)
	{
		if (m_current < m_end && !cs(*m_current))
		{
			++m_current;
			return true;
		}
		return false;
	}


private:
	const char* m_begin;
	const char* m_end;
	const char* m_current;
	URI* m_result;
};

const CharSet UriParser::m_uri_reserved("/?# ");
//by tianwei
//m_uri_reserved should not include &
//e.g
//http://dl.pconline.com.cn/html_2/1/89/id=42443&pn=0&linkPage=1.html
//m_uri_reserved should include #
//or we cannot trim fragment part
const CharSet UriParser::m_cs_reserved(";/?:@&=+$,");
const CharSet UriParser::m_cs_mark("-_.!~*'()");
const CharSet UriParser::m_cs_unreserved(GetAlphaNumSet() | m_cs_mark);
const CharSet UriParser::m_cs_userinfo(m_cs_unreserved | ";:&=+$,");
const CharSet UriParser::m_cs_uric_no_slash(m_cs_unreserved | ";?:@&=+$,");
const CharSet UriParser::m_cs_rel_segment(m_cs_unreserved | ";@&=+$,");
const CharSet UriParser::m_cs_scheme(GetAlphaNumSet() | "+-.");
const CharSet UriParser::m_cs_reg_name(m_cs_unreserved | "$,;:@&=+");
const CharSet UriParser::m_cs_pchar(m_cs_unreserved | ":@&=+$,");
const CharSet UriParser::m_cs_uric(m_cs_reserved | m_cs_unreserved);

size_t UriParse(const char* Uri, size_t UriLength, URI& Result)
{
	UriParser p;
	Result.Clear();
	return p.Parse(Uri, UriLength, Result);
}

std::string& URI::ToString(std::string& Result) const
{
	Result.clear();
	if (!this->Scheme().empty())
	{
		Result = this->Scheme();
		Result += ':';
	}

	if (this->IsOpaque())
	{
		Result += this->OpaquePart();
	}
	else
	{
		if (this->HasAuthority())
		{
			Result.append("//", 2);
			if (this->HasUserInfo())
			{
				Result += this->UserInfo();
				Result += '@';
			}
			Result += this->Host();
			if (this->HasPort())
			{
				Result += ':';
				Result += Port();
			}
		}
		else
		{
			Result.append("//", 2);
			Result += RegName();
		}

		Result += this->Path();
		if (this->HasQuery())
		{
			Result += '?';
			Result += this->Query();
		}
	}
	if (this->HasFragment())
	{
		Result += '#';
		Result += this->Fragment();
	}
	
	return Result;
}

std::string URI::ToString() const
{
	std::string s;
	ToString(s);
	return s;
}

bool URI::ToString(char* buffer, size_t buffer_size, size_t& result_size) const
{
	if (buffer_size < 1)
	{
		result_size = 0;
		return -1;
	}

	std::ostrstream oss(buffer, buffer_size);
	if (!this->Scheme().empty())
	{
		oss << m_Scheme;
		oss << ':';
	}

	if (this->IsOpaque())
	{
		oss << m_OpaquePart;
	}
	else
	{
		if (this->HasAuthority())
		{
			oss << "//";
			if (this->HasUserInfo())
			{
				oss << this->UserInfo();
				oss << '@';
			}
			oss << this->Host();
			if (this->HasPort())
			{
				oss << ':';
				oss << this->Port();
			}
		}
		else
		{
			oss << "//";
			oss << RegName();
		}

		oss << this->Path();
		if (this->HasQuery())
		{
			oss << '?';
			oss << this->Query();
		}
	}
	if (this->HasFragment())
	{
		oss << '#';
		oss << this->Fragment();
	}

	oss.flush();

	result_size = oss.tellp();
	buffer[result_size] = '\0';

	return oss;
}

bool URI::Normalize()
{
	StringLower(m_Scheme);
	StringLower(m_Authority.m_Host);
	return true;
}

bool URI::ToAbsolute(const URI& base)
{
	URI result;
	UriMerge(*this, base, result);
	*this = result;
	return true;
}

static void remove_last_segment_and_preceding_slash(std::string& path)
{
	size_t pos = path.find_last_of('/');
	if (pos != path.npos)
	{
		path.erase(pos);
	}
}

// 5.2.4.  Remove Dot Segments
static void remove_dot_segments(const std::string& path, std::string& result)
{
	result.clear();

	std::vector<char> input(path.begin(), path.end());
	char* input_buffer = &input[0];
	size_t input_length = input.size();

	while (input_length > 0)
	{
		// rule A
		if (input_length >= 3 && memcmp(input_buffer, "../", 3) == 0)
		{
			input_buffer += 3;
			input_length -= 3;
		}
		else if (input_length >= 2 && memcmp(input_buffer, "./", 2) == 0)
		{
			input_buffer += 2;
			input_length -= 2;
		}
		// rule B
		else if (input_length >= 3 && memcmp(input_buffer, "/./", 3) == 0)
		{
			input_buffer += 2;
			input_length -= 2;
			*input_buffer = '/';
		}
		else if (input_length == 2 && memcmp(input_buffer, "/.", 2) == 0)
		{
			input_buffer += 1;
			input_length -= 1;
			*input_buffer = '/';
		}
		// rule C
		else if (input_length >= 4 && memcmp(input_buffer, "/../", 4) == 0)
		{
			input_buffer += 3;
			input_length -= 3;
			*input_buffer = '/';
			remove_last_segment_and_preceding_slash(result);
		}
		else if (input_length == 3 && memcmp(input_buffer, "/..", 3) == 0)
		{
			input_buffer += 2;
			input_length -= 2;
			*input_buffer = '/';
			remove_last_segment_and_preceding_slash(result);
		}
		// rule D
		else if (input_length == 1 && *input_buffer == '.')
		{
			++input_buffer;
			--input_length;
		}
		else if (input_length == 2 && memcmp(input_buffer, "..", 2) == 0)
		{
			input_buffer += 2;
			input_length -= 2;
		}
		// rule E
		else
		{
			char* p = reinterpret_cast<char*>(memchr(input_buffer + 1, '/', input_length - 1));
			if (p)
			{
				result.append(input_buffer, p - input_buffer);
				input_length -= p - input_buffer;
				input_buffer = p;
			}
			else
			{
				result.append(input_buffer, input_length);
				input_length = 0;
			}
		}
	}
}

static std::string remove_dot_segments(const std::string& path)
{
	std::string result;
	remove_dot_segments(path, result);
	return result;
}

static void merge_path(bool base_has_authority, const std::string& base_path, const std::string& path, std::string& result)
{
	if (base_has_authority && base_path.empty())
	{
		result = '/';
		result += path;
	}
	else
	{
		size_t pos = base_path.find_last_of('/');
		if (pos != result.npos)
		{
			result.assign(base_path, 0, pos + 1);
			result += path;
		}
		else
		{
			result = path;
		}
	}
}

static std::string merge_path(bool base_has_authority, const std::string& base_path, const std::string& path)
{
	std::string result;
	merge_path(base_has_authority, base_path, path, result);
	return result;
}


// RFC 3986
// 5.2.2.  Transform References
bool UriMerge(const URI& uri, const URI& base, URI& result, bool strict)
{
	result.Clear();

	bool scheme_undefined = (!strict && uri.Scheme() == base.Scheme());

	if (!scheme_undefined && !uri.Scheme().empty())
	{
		result.SetScheme(uri.Scheme());
		if (uri.HasAuthority())
			result.SetAuthority(uri.Authority());
		result.SetPath(remove_dot_segments(uri.Path()));
		if (uri.HasQuery())
			result.SetQuery(uri.Query());
	}
	else
	{
		if (uri.HasAuthority())
		{
			if (uri.HasAuthority())
	                        result.SetAuthority(uri.Authority());
                        
			result.SetPath(remove_dot_segments(uri.Path()));
			if (uri.HasQuery())
				result.SetQuery(uri.Query());
//			printf("%s\n", result.ToString().c_str());


		}
		else
		{
			if (uri.Path().empty())
			{
				result.SetPath(base.Path());
				if (uri.HasQuery())
					result.SetQuery(uri.Query());
				else if (base.HasQuery())
					result.SetQuery(base.Query());
			}
			else
			{
				std::string path;
				if (!uri.Path().empty() && uri.Path()[0] == '/')
				{
					path = remove_dot_segments(uri.Path());
				}
				else
				{
					path = merge_path(base.HasAuthority(), base.Path(), uri.Path());
					path = remove_dot_segments(path);
				}
				result.SetPath(path);
				if (uri.HasQuery())
					result.SetQuery(uri.Query());
			}
			if (base.HasAuthority())
				result.SetAuthority(base.Authority());
		}
		result.SetScheme(base.Scheme());
	}

	if (uri.HasFragment())
	{
		result.SetFragment(uri.Fragment());
	}
//	printf("%s\n", result.ToString().c_str());
	return true;
}

bool HttpUriNormalize(URI& uri)
{
	uri.Normalize();
	const std::string& scheme = uri.Scheme();
	if (scheme.empty())
		return false;

	if (scheme != "http" && scheme != "https")
		return false;

	if (uri.IsOpaque())
		return false;

	if (!uri.HasAuthority())
		return false;

	uri.ClearUserInfo();

	const std::string& host = uri.Host();
	if (host.empty())
		return false;

	if (host[host.length() - 1] == '.')
	{
		uri.SetHost(host.c_str(), host.length() - 1);
	}

	if (uri.HasPort())
	{
		const std::string& port = uri.Port();
		if ((scheme == "http" && port == "80") ||
			(scheme == "https" && port == "443"))
		{
			uri.ClearPort();
		}
	}

	if (uri.Path().empty())
		uri.SetPath("/");

	uri.ClearFragment();

	URI root_uri = uri;
	root_uri.ClearQuery();
	root_uri.SetPath("");
	uri.ToAbsolute(root_uri);

	return true;
}

