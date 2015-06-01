#ifndef HTTP_MESSAGE_HPP_INCLUDED
#define HTTP_MESSAGE_HPP_INCLUDED

#pragma once

//////////////////////////////////////////////////////////////////////////
// Http Message, Http Request, Http Reponse, and Parsing
// Chen Feng <chenfeng@sohu-rd.com>
//////////////////////////////////////////////////////////////////////////

#include <cstring>
#include <string>
#include <vector>
#include <stdexcept>
#include <iosfwd>

struct MessageHeader
{
	std::string Name;
	std::string Value;
};
std::ostream &operator<<(std::ostream&, const MessageHeader&);

class MessageHeaders
{
public:
	void Add(const MessageHeader& header)
	{
		m_Headers.push_back(header);
	}

	void Add(const std::string& name, const std::string& value)
	{
		MessageHeader header = { name, value };
		m_Headers.push_back(header);
	}

    void Set(const std::string& name, const std::string& value)
    {
        int index = Find(name);
        if(index < 0)
            Add(name, value);
        else
            m_Headers[index].Value = value;

    }

	bool Remove(size_t index)
	{
		if (index < m_Headers.size())
		{
			m_Headers.erase(m_Headers.begin() + index);
			return true;
		}
		return false;
	}

    MessageHeader& Back()
    {
        return m_Headers.back();
    }

	size_t Size() const
	{
		return m_Headers.size();
	}

	void Clear()
	{
		m_Headers.clear();
	}

	bool Empty() const
	{
		return m_Headers.empty();
	}

	const MessageHeader& operator[](size_t index) const
	{
		return m_Headers.at(index);
	}

	MessageHeader& operator[](size_t index)
	{
		return m_Headers.at(index);
	}

	int Find(const char* name, int start = 0) const
	{
		size_t name_length = strlen(name);
		for (size_t i = start; i < m_Headers.size(); ++i)
		{
			if (m_Headers[i].Name.length() == name_length &&
				strcasecmp(name, m_Headers[i].Name.c_str()) == 0)
			{
				return i;
			}
		}
		return -1;
	}

	int Find(const std::string& name, int start = 0) const
	{
		for (size_t i = start; i < m_Headers.size(); ++i)
		{
			if (name.length() == m_Headers[i].Name.length() &&
				strcasecmp(name.c_str(), m_Headers[i].Name.c_str()) == 0)
			{
				return i;
			}
		}
		return -1;
	}

	std::string& operator[](const std::string& name)
	{
		int i = Find(name);
		if (i < 0)
			throw std::runtime_error(name + " doesn't exist");
		return m_Headers[i].Value;
	}

	const std::string& operator[](const std::string& name) const
	{
		return const_cast<const std::string&>(const_cast<MessageHeaders&>(*this)[name]);
	}

	bool FindValue(const std::string& name, int start, std::string& value) const
	{
		int i = Find(name, start);
		if (i < 0)
			return false;
		value = m_Headers[i].Value;
		return true;
	}

	bool FindValue(const std::string& name, std::string& value) const
	{
		int i = Find(name);
		if (i < 0)
			return false;
		value = m_Headers[i].Value;
		return true;
	}

    std::vector<std::string> FindAllValue(const std::string& name) const
    {
        std::vector<std::string> val;
        for(unsigned i = 0; i < m_Headers.size(); ++i)
        {
            if(m_Headers[i].Name == name)
                val.push_back(m_Headers[i].Value);
        }
        return val;
    }

private:
	std::vector<MessageHeader> m_Headers;
        friend std::ostream &operator<<(std::ostream&, const MessageHeaders&);
};

std::ostream &operator<<(std::ostream&, const MessageHeaders&);

class Message
{
public:
public:
	virtual ~Message(){}
	virtual std::string StartLine() const = 0;
	MessageHeaders Headers;
	std::vector<char> Body;
public:
	virtual bool Empty() const = 0;
	virtual void Clear()
	{
		Headers.Clear();
		Body.clear();
	}
	void AppendBody(const void* data, size_t length)
	{
		const char* fragment = reinterpret_cast<const char*>(data);
		Body.insert(Body.end(), fragment, fragment + length);
	}
};

class Request : public Message
{
public:
	std::string Method;
	std::string Uri;
	std::string Version;

public:
	Request()
	{
	}
	Request(const std::string& method, const std::string& uri, const std::string& version)
		: Method(method), Uri(uri), Version(version)
	{
	}
	Request(const std::string& method, const std::string& uri)
		: Method(method), Uri(uri)
	{
	}
	virtual std::string StartLine() const;
	std::string RequestLine() const
	{
		return Request::StartLine();
	}

	virtual bool Empty() const
	{
		return Method.empty();
	}

	virtual void Clear()
	{
		Message::Clear();
		Method.clear();
		Uri.clear();
		Version.clear();
	}
};
std::ostream &operator<<(std::ostream&, const Request&);


class Response : public Message
{
public:
	Response() : StatusCode(0)
	{
	}

	std::string Version;
	int StatusCode;
	std::string ReasonPhrase;

public:
	virtual std::string StartLine() const;
	std::string StatusLine() const
	{
		return Response::StartLine();
	}
	
	// Header is not acquired if it is Empty
	virtual bool Empty() const
	{
		return Version.empty();
	}

	virtual void Clear()
	{
		Message::Clear();
		Version.clear();
		StatusCode = 0;
		ReasonPhrase.clear();
	}
};


// size_t ParseMessage(const void* message, size_t length);

size_t ParseRequest(const void* message, size_t length, Request& request);
size_t ParseResponse(const void* message, size_t length, Response& response);


#endif//HTTP_MESSAGE_HPP_INCLUDED
