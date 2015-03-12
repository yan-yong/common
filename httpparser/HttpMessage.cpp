//////////////////////////////////////////////////////////////////////////
// Http message implementation
// Chen Feng <chenfeng@sohu-rd.com>
//////////////////////////////////////////////////////////////////////////

#include <cstdio>
#include <iostream>

#include "HttpMessage.hpp"
#include "HttpMessageParser.hpp"

/////////////////////////////////////////////////////////////////////
// Http::Message


/////////////////////////////////////////////////////////////////////
// Http::Request

std::string Request::StartLine() const
{
	std::string result = Method;
	result += ' ';
	result += Uri;

	if (!Version.empty())
	{
		result += ' ';
		result += Version;
	}

	return result;
}

/////////////////////////////////////////////////////////////////////
// Http::Response

std::string Response::StartLine() const
{
	std::string result = Version;
	result += ' ';
	char code_str[12];
	int n = std::sprintf(code_str, "%d", StatusCode);
	result.append(code_str, n);
	result += ' ';
	result += ReasonPhrase;

	return result;
}

//////////////////////////////////////////////////////////////////////////
// Parse interface
//////////////////////////////////////////////////////////////////////////

namespace
{

class RequestParser :
	private MessageParser::EventSink // to receive MessageParser events
{
public:
	RequestParser() : 
		m_Parser(static_cast<MessageParser::EventSink&>(*this)),
   		m_Request(NULL),
		m_TypeError(false)
	{
	}

	size_t Parse(const void* message, size_t length, Request& request)
	{
		m_Request = &request;
		request.Clear();
		size_t result = m_Parser.Parse(message, length);
		if (m_TypeError)
			result = 0;
		if (result == 0)
			request.Clear();
		return result;
	}

private: // Event handlers
	virtual void OnRequestLine(
		const char* method, size_t method_length,
		const char* uri, size_t uri_length,
		const char* version, size_t version_length
	)
	{
		m_Request->Method.assign(method, method_length);
		m_Request->Uri.assign(uri, uri_length);
		if (version)
			m_Request->Version.assign(version, version_length);
	}

	virtual void OnStatusLine(
		const char* version, size_t version_length,
		const char* status_code, size_t status_code_length,
		const char* reason_phrase, size_t reason_phrase_length
	)
	{
		m_TypeError = true;
	}

	virtual void OnHeader(
		const char* name, size_t name_length,
		const char* value, size_t value_length
	)
	{
		MessageHeader header =
	   	{
			std::string(name, name_length),
			std::string(value, value_length)
	   	};
		m_Request->Headers.Add(header);
	}

	virtual void OnHeadersComplete()
	{

	}

	virtual void OnBody(const void* body, size_t length)
	{
		m_Request->Body.resize(length);
		memcpy(&m_Request->Body[0], body, length);
	}

private:
	MessageParser m_Parser;
	Request* m_Request;
	bool m_TypeError;
};


class ResponseParser :
	private MessageParser::EventSink // to receive MessageParser events
{
public:
	ResponseParser() : 
		m_Parser(static_cast<MessageParser::EventSink&>(*this)),
   		m_Response(NULL),
		m_TypeError(false)
	{
	}

	size_t Parse(const void* message, size_t length, Response& response)
	{
		response.Clear();
		m_Response = &response;
		
		size_t result = m_Parser.Parse(message, length);

		if (m_TypeError)
			result = 0;
		if (result == 0)
			response.Clear();

		return result;
	}

private: // Event handlers
	virtual void OnRequestLine(
		const char* method, size_t method_length,
		const char* uri, size_t uri_length,
		const char* version, size_t version_length
	)
	{
		m_TypeError = true;
	}

	virtual void OnStatusLine(
		const char* version, size_t version_length,
		const char* status_code, size_t status_code_length,
		const char* reason_phrase, size_t reason_phrase_length
	)
	{
		m_Response->Version.assign(version, version_length);

		// assert(status_code_length == 3);
		m_Response->StatusCode = 
			(status_code[0]-'0') * 100 + 
			(status_code[1]-'0') * 10 + 
			(status_code[2]-'0');

		if (reason_phrase)
			m_Response->ReasonPhrase.assign(reason_phrase, reason_phrase_length);
	}

	virtual void OnHeader(
		const char* name, size_t name_length,
		const char* value, size_t value_length
	)
	{
		MessageHeader header =
	   	{
			std::string(name, name_length),
			std::string(value, value_length)
	   	};
		m_Response->Headers.Add(header);
	}

	virtual void OnHeadersComplete()
	{

	}

	virtual void OnBody(const void* body, size_t length)
	{
		m_Response->Body.resize(length);
		memcpy(&m_Response->Body[0], body, length);
	}

private:
	MessageParser m_Parser;
	Response* m_Response;
	bool m_TypeError;
};

}

size_t ParseRequest(const void* message, size_t length, Request& request)
{
	RequestParser parser;
	return parser.Parse(message, length, request);
}

size_t ParseResponse(const void* message, size_t length, Response& response)
{
	ResponseParser parser;
	return parser.Parse(message, length, response);
}

using namespace std;
ostream &operator<<(ostream &os, const MessageHeader &h)
{

    os << h.Name << ": " << h.Value;
    return os;
}

ostream &operator<<(ostream &os, const MessageHeaders &h)
{
    for(size_t i = 0; i< h.m_Headers.size(); ++i){
        os << h.m_Headers[i] << "\n";
    }
    return os;
}

std::ostream &operator<<(std::ostream&os, const Request& req)
{
    os << req.Method << " " << req.Uri << " " << req.Version << "\n"
        << req.Headers;

    return os;
}

