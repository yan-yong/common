//////////////////////////////////////////////////////////////////////////
// Http Message Parser implementation
// see RFC2616: Hypertext Transfer Protocol -- HTTP/1.1
// Chen Feng <chenfeng@sohu-rd.com>
//////////////////////////////////////////////////////////////////////////

#include <ctype.h>
#include <string.h>
#include "HttpMessageParser.hpp"

//	separators     = "(" | ")" | "<" | ">" | "@"
//		| "," | ";" | ":" | "\" | <">
//		| "/" | "[" | "]" | "?" | "="
//		| "{" | "}" | SP | HT
bool MessageParser::IsSeparator(char c)
{
	switch (c)
	{
	case '(':
	case ')':
	case '<':
	case '>':
	case '@':
	case ',':
	case ';':
	case ':':
	case '\\':
	case '\'':
	case '/':
	case '[':
	case ']':
	case '?':
	case '=':
	case '{':
	case '}':
	case ' ':
	case '\t':
		return true;
	}
	return false;
}

// Match a single character
bool MessageParser::MatchChar(char c)
{
	if (m_Current < m_End && *m_Current == c)
	{
		++m_Current;
		return true;
	}
	return false;
}

// Match string literal
bool MessageParser::MatchString(const char* text, size_t length)
{
	if (m_End - m_Current >= static_cast<int>(length) && memcmp(text, m_Current, length) == 0)
	{
		m_Current += length;
		return true;
	}
	return false;
}

// LWS: linear white space
//  LWS            = [CRLF] 1*( SP | HT )
bool MessageParser::MatchLWS()
{
	Result result(this);
	
	MatchCRLF(); // optional matching

	int sp_vt_count = 0;
	while (MatchSP() || MatchHT())
		++sp_vt_count;
	result = sp_vt_count > 0;

	return result;
}

// CRLF           = CR LF
// but for UNIX compatible, CR can be absent
bool MessageParser::MatchCRLF()
{
	Result result(this);
	
	// this result may be ignored for UNIX compatible in non-strict mode
	result = MatchChar('\r'); 
	
	result = MatchChar('\n');
	return result;
}

// SP = ' '
bool MessageParser::MatchSP()
{
	return MatchChar(' ');
}

// HT = '\t', horizontal table
bool MessageParser::MatchHT()
{
	return MatchChar('\t');
}

// token          = 1*<any CHAR except CTLs or separators>
bool MessageParser::MatchToken()
{
	const char* begin = m_Current;
	while (m_Current < m_End && !iscntrl(*m_Current) && !IsSeparator(*m_Current))
		++m_Current;
	return m_Current > begin;
}

// Request-URI    = "*" | absoluteURI | abs_path | authority
bool MessageParser::MatchRequestUri()
{
	Result result(this);
	result = MatchChar('*');
	if (result)
		return true;

	while (m_Current < m_End && !isspace(*m_Current))
	{
		++m_Current;
	}

	result = result.Length() > 0;

	return result;
}

// HTTP-Version   = "HTTP" "/" 1*DIGIT "." 1*DIGIT
bool MessageParser::MatchHttpVersion()
{
	Result result(this);
	if (!MatchString("HTTP/"))
		return false;

	const char* begin;

	begin = m_Current;
	while (m_Current < m_End && isdigit(*m_Current))
		++m_Current;
	if (m_Current == begin)
		return false;

	if (!MatchChar('.'))
			return false;

	begin = m_Current;
	while (m_Current < m_End && isdigit(*m_Current))
		++m_Current;
	if (m_Current == begin)
		return false;

	result = true;

	return result;
}

// Status-Code    = 3DIGIT
bool MessageParser::MatchStatusCode()
{
	if (m_End - m_Current >= 3 &&
		isdigit(m_Current[0]) &&
		isdigit(m_Current[1]) &&
		isdigit(m_Current[2]))
	{
		m_Current += 3;
		return true;
	}
	return false;	
}

// Reason-Phrase  = *<TEXT, excluding CR, LF>
bool MessageParser::MatchReasonPhrase()
{
	while (m_Current < m_End && *m_Current != '\r' && *m_Current != '\n')
		++m_Current;
	return true;
}

// Method         = "OPTIONS"                ; Section 9.2
//	| "GET"                    ; Section 9.3
//	| "HEAD"                   ; Section 9.4
//	| "POST"                   ; Section 9.5
//	| "PUT"                    ; Section 9.6
//	| "DELETE"                 ; Section 9.7
//	| "TRACE"                  ; Section 9.8
//	| "CONNECT"                ; Section 9.9
//	| extension-method
//	extension-method = token
bool MessageParser::MatchMethod()
{
	return MatchToken();
}

// field-name     = token
bool MessageParser::MatchFieldName()
{
	return MatchToken();
}

// field-value    = *( field-content | LWS )
bool MessageParser::MatchFieldValue(std::string& value)
{
	value.clear();
	for (;;)
	{
		Result result(this);
		if (MatchLWS()) 
		{
			// replace LWS with space
			if (!value.empty())
				value += ' ';
			result = true;
		}
		else if (MatchFieldContent())
		{
			value.append(result.Begin(), result.Length());
			result = true;
		}
		else
		{
			break;
		}
	}

	// TODO trim value string

	return true;
}

// field-content  = <the OCTETs making up the field-value
//                  and consisting of either *TEXT or combinations
//                  of token, separators, and quoted-string>
bool MessageParser::MatchFieldContent()
{
	const char* begin = m_Current;
	while (m_Current < m_End && *m_Current != '\r' && *m_Current != '\n')
		++m_Current;
	return m_Current > begin;
}

bool MessageParser::SkipInvalidHeader()
{
	const char* begin = m_Current;
	while (m_Current < m_End && *m_Current != '\r' && *m_Current != '\n')
		++m_Current;
	return m_Current > begin;
}

// message-header = field-name ":" [ field-value ]
bool MessageParser::MatchMessageHeader(
	const char*& name, size_t& name_length,
	std::string& value
)
{
	Result result(this);
	result = MatchFieldName() && MatchChar(':');
	if (result)
	{
		name = result.Begin();
		name_length = result.Length() - 1; // not include ':'
		MatchFieldValue(value);
	}
	return result;
}

// Request-Line   = Method SP Request-URI SP HTTP-Version CRLF
bool MessageParser::MatchRequestLine()
{
	Result result(this);

	// Method part
	Result method_matching(this);
   	method_matching = MatchMethod();
	if (!method_matching)
		return false;
	const char* method = method_matching.Begin();
	size_t method_length = method_matching.Length();

   	if (!MatchSP())
	   return false;

	// Result-URI part
	Result uri_matching(this);
	uri_matching = MatchRequestUri();
	if (!uri_matching)
		return false;
	const char* uri = uri_matching.Begin();
	size_t uri_length = uri_matching.Length();

	const char* version = NULL;
	size_t version_length = 0;
   	if (MatchSP()) // for HTTP 1.0 compatible
	{
		// HTTP-Version part
		Result version_matching(this);
		version_matching = MatchHttpVersion();
		if (!version_matching)
			return false;
		version = version_matching.Begin();
		version_length = version_matching.Length();
	}

	result = MatchCRLF();
	if (!result)
		return false;

	m_EventSink.OnRequestLine(
		method, method_length, 
		uri, uri_length,
		version, version_length
	);

	return result;
}

//	message-body = entity-body
//               | <entity-body encoded as per Transfer-Encoding>
bool MessageParser::MatchMessageBody()
{
	m_EventSink.OnBody(m_Current, m_End - m_Current);
	m_Current = m_End;
	return true;
}

// Status-Line = HTTP-Version SP Status-Code SP Reason-Phrase CRLF
bool MessageParser::MatchStatusLine()
{
	Result result(this);
	
	// HTTP-Version part
	Result version_matching(this);
	version_matching = MatchHttpVersion();
	if (!version_matching)
		return false;
	const char* version = version_matching.Begin();
	size_t version_length = version_matching.Length();

	if (!MatchSP())
		return false;

	// Status-Code part
	Result status_code_matching(this);
	status_code_matching = MatchStatusCode();
	if (!status_code_matching)
		return false;
	const char* status_code = status_code_matching.Begin();
	size_t status_code_length = status_code_matching.Length();
/*
	if (!MatchSP())
		return false;

	// Reason-Phrase part
	const char* reason_phrase = NULL;
	size_t reason_phrase_length = 0;
	{
		Result reason_phrase_matching(this);
		reason_phrase_matching = MatchReasonPhrase();
		if (!reason_phrase_matching)
			return false;
		reason_phrase = reason_phrase_matching.Begin();
		reason_phrase_length = reason_phrase_matching.Length();
	}

	result = MatchCRLF();
	if (!result)
		return false;
*/

	const char* reason_phrase = NULL;
        size_t reason_phrase_length = 0;

	result = MatchCRLF();
        if (!result)
        {
                result = true;
                if (!MatchSP())
                        return false;

                // Reason-Phrase part
                {
                        Result reason_phrase_matching(this);
                        reason_phrase_matching = MatchReasonPhrase();
                        if (!reason_phrase_matching)
                                return false;
                        reason_phrase = reason_phrase_matching.Begin();
                        reason_phrase_length = reason_phrase_matching.Length();
                }
                result = MatchCRLF();
                if (!result)
                {
                        return false;
                }
        }
        else
        {
                reason_phrase = m_Current - 2;
                reason_phrase_length = 0;
        }

	m_EventSink.OnStatusLine(
		version, version_length, 
		status_code, status_code_length,
		reason_phrase, reason_phrase_length
	);

	return result;
}

//	*(message-header CRLF)
//	CRLF
bool MessageParser::MatchMessageHeaders()
{
	Result result(this);
	const char* name;
	size_t name_length;
	std::string value;
	while (true)
	{
        if(MatchMessageHeader(name, name_length, value) && MatchCRLF()){
            m_EventSink.OnHeader(name, name_length, value.data(), value.length());
        }
        else if(SkipInvalidHeader() && MatchCRLF())
        {
            // do nothing;
        }
        else
        {
            break;
        }
	}
	result = MatchCRLF();
	return result;
}

//	Request       = Request-Line              ; Section 5.1
//		*(( general-header        ; Section 4.5
//		| request-header         ; Section 5.3
//		| entity-header ) CRLF)  ; Section 7.1
//		CRLF
//		[ message-body ]          ; Section 4.3
bool MessageParser::MatchRequest()
{
	Result result(this);
	result = MatchRequestLine() && MatchMessageHeaders() && MatchMessageBody();
	return result;
}

//	Response = Status-Line ; Section 6.1
//		*(( general-header ; Section 4.5
//		| response-header ; Section 6.2
//		| entity-header ) CRLF) ; Section 7.1
//	CRLF
//	[ message-body ] ; Section 7.2

bool MessageParser::MatchResponse()
{
	Result result(this);
	result = MatchStatusLine() && MatchMessageHeaders() && MatchMessageBody();
	return result;
}

// HTTP-message   = Request | Response     ; HTTP/1.1 messages
bool MessageParser::MatchMessage()
{
	Result result(this);
	result = MatchRequest() || MatchResponse();
	return result;
}


size_t MessageParser::Parse(const void* message, size_t size)
{
	m_Begin = reinterpret_cast<const char*>(message);
	m_End = m_Begin + size;
	
	m_Current = m_Begin;
	
	if (MatchMessage())
		return m_Current - m_Begin;
	else
		return 0;
}
