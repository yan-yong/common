#ifndef HTTP_MESSAGE_PARSER_HPP_INCLUDED
#define HTTP_MESSAGE_PARSER_HPP_INCLUDED

//////////////////////////////////////////////////////////////////////////
// Http Message Parser declaration
// Chen Feng <chenfeng@sohu-rd.com>
//////////////////////////////////////////////////////////////////////////

#pragma once

#include <stddef.h>
#include <string>

class MessageParser
{
public:
	struct EventSink
	{
	  virtual ~EventSink(){}

		virtual void OnRequestLine(
				const char* method, size_t method_length,
				const char* uri, size_t uri_length,
				const char* version, size_t version_length
				) = 0;
		virtual void OnStatusLine(
				const char* version, size_t version_length,
				const char* status_code, size_t status_code_length,
				const char* reason_phrase, size_t reason_phrase_length
				) = 0;
		virtual void OnHeader(
				const char* name, size_t name_length,
				const char* value, size_t value_length
				) = 0;

		virtual void OnHeadersComplete() = 0;

		virtual void OnBody(const void* body, size_t length) = 0;
	};

public:
	MessageParser(EventSink& event_sink):
		m_EventSink(event_sink),
		m_Begin(NULL),
		m_End(NULL),
		m_Current(NULL)
	{
	}
	size_t Parse(const void* message, size_t size);

private:
	class Result
	{
		friend class MessageParser;
	public:
		Result(MessageParser& parser) : m_Parser(parser), m_Begin(parser.m_Current), m_Result(false){}
		Result(MessageParser* parser) : m_Parser(*parser), m_Begin(parser->m_Current), m_Result(false){}
		~Result()
		{ 
			// if match failure, roll back
			if (!m_Result) 
				m_Parser.m_Current = m_Begin;
		}
		operator bool() const
		{
			return m_Result;
		}
		Result& operator=(bool value)
		{
			m_Result = value;
			return *this;
		}
		const char* Begin() const
		{
			return m_Begin;
		}
		const char* End() const
		{
			return m_Parser.m_Current;
		}
		size_t Length() const
		{
			return m_Parser.m_Current - m_Begin;
		}
	private:
		// forbid copy
		Result(const Result&);
		Result& operator=(const Result&);
	private:
		MessageParser& m_Parser;
		const char* m_Begin;
		bool m_Result;
	};

private:
	static bool IsSeparator(char c);

private: // Match functions
	// All Match function must restore m_Current if it return false

	bool MatchChar(char c);
	bool MatchString(const char* text, size_t length);
	template <size_t N>
	bool MatchString(const char (&text)[N])
	{
		return MatchString(text, N - 1);
	}
	bool MatchLWS();
	bool MatchCRLF();
	bool MatchSP();
	bool MatchHT();
	bool MatchToken();
	bool MatchRequestUri();

	bool MatchHttpVersion();
	bool MatchStatusCode();
	
	bool MatchReasonPhrase();
	bool MatchMethod();

	bool MatchFieldName();
	bool MatchFieldValue(std::string& value);
	bool MatchFieldContent();
	bool MatchMessageHeader(const char*& name, size_t& name_length, std::string& value);

	bool MatchMessageBody();
	bool MatchMessageHeaders();
	bool MatchRequestLine();
	bool MatchStatusLine();

	bool MatchRequest();
	bool MatchResponse();
	bool MatchMessage();

    bool SkipInvalidHeader();
private:
	EventSink& m_EventSink;
	const char* m_Begin;
	const char* m_End;
	const char* m_Current;
};

#endif//HTTP_MESSAGE_PARSER_HPP_INCLUDED
