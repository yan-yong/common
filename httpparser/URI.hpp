#ifndef URI_HPP_INCLUDED
#define URI_HPP_INCLUDED

#include <cassert>
#include <string>
#include <utility>
#include "FastCType.hpp"

template <typename InputIterator>
void UriEscape(InputIterator begin, InputIterator end, std::string& dest)
{
	const CharSet& IsAscii = GetAsciiSet();
	dest.clear();
	InputIterator i = begin;
	while (i != end)
	{
		if (!IsAscii((unsigned char)(*i)))
		{
			dest.append(begin, i);

			static const char hex[] = "0123456789ABCDEF";
			dest += '%';
			unsigned char c = *i;
			dest += hex[c >> 4];
			dest += hex[c & 0x0F];
			begin = i + 1;
		}
		++i;
	}
	dest.append(begin, end);
}

template <typename InputIterator>
void UriEscape(InputIterator begin, size_t length, std::string& dest)
{
	InputIterator end = begin;
	std::advance(end, length);
	UriEscape(begin, end, dest);
}

void UriEscape(const char* begin, const char* end, std::string& dest);
void UriEscape(const std::string& src, std::string& dest);
void UriEscape(std::string& uri);
void UrlUniform(const std::string url, std::string& result);

struct UriAuthority
{
	friend class URI;
public:
	UriAuthority():
		m_HasUserInfo(false),
		m_HasPort(false)
	{
	}

	bool HasUserInfo() const
	{
		return m_HasUserInfo;
	}

	const std::string& UserInfo() const
	{
		assert(m_HasUserInfo);
		return m_UserInfo;
	}

	void SetUserInfo(const std::string& value)
	{
		m_UserInfo = value;
		m_HasUserInfo = true;
	}

	void ClearUserInfo()
	{
		m_UserInfo.clear();
		m_HasUserInfo = false;
	}

	const std::string& Host() const
	{
		return m_Host;
	}

	void SetHost(const std::string& value)
	{
		m_Host = value;
	}

	bool HasPort() const
	{
		return m_HasPort;
	}

	const std::string& Port() const
	{
		assert(m_HasPort);
		return m_Port;
	}

	void SetPort(const std::string& value)
	{
		m_Port = value;
		m_HasPort = true;
	}

	void ClearPort()
	{
		m_Port.clear();
		m_HasPort = false;
	}

	void Clear()
	{
		ClearUserInfo();
		m_Host.clear();
		ClearPort();
	}

	bool operator == (const UriAuthority &other) const
	{
	    return m_HasUserInfo == other.m_HasUserInfo
		&& m_HasPort == other.m_HasPort
		&& m_UserInfo == other.m_UserInfo
		&& m_Host == other.m_Host
		&& m_Port == other.m_Port;
	}

private:
	bool m_HasUserInfo : 1;
	bool m_HasPort : 1;
	std::string m_UserInfo;
	std::string m_Host;
	std::string m_Port;
};

struct URI
{
public:
	URI() : 
		m_IsOpaque(true), 
		m_HasAuthority(false),
		m_HasQuery(false),
		m_HasFragment(false)
	{
	}

public: //Attributes
	// Scheme
	const std::string& Scheme() const
	{
		return m_Scheme;
	}

	void SetScheme(const std::string& value)
	{
		m_Scheme = value;
	}

	void SetScheme(const char* value, size_t length)
	{
		m_Scheme.assign(value, length);
	}

	// OpaquePart
	bool IsOpaque() const
	{
		return m_IsOpaque;
	}

	const std::string& OpaquePart() const
	{
		return m_OpaquePart;
	}

	void SetOpaquePart(const std::string& value)
	{
		m_OpaquePart = value;
		m_IsOpaque = true;
	}

	void SetOpaquePart(const char* value, size_t length)
	{
		m_OpaquePart.assign(value, length);
		m_IsOpaque = true;
	}

	// Authority
	bool HasAuthority() const
	{
		return m_HasAuthority;
	}

	UriAuthority& Authority()
	{
		assert(m_HasAuthority);
		return m_Authority;
	}
	const UriAuthority& Authority() const
	{
		assert(m_HasAuthority);
		return m_Authority;
	}
	void SetAuthority(const UriAuthority& value)
	{
		m_HasAuthority = true;
		m_Authority = value;
	}

	void ClearAuthority()
	{
		m_Authority.Clear();
		m_HasAuthority = false;
		m_RegName.clear();
	}

	// RegName
	const std::string& RegName() const
	{
		return m_RegName;
	}

	void SetRegName(const std::string& value)
	{
		m_RegName = value;
		m_HasAuthority = false;
	}

	void SetRegName(const char* value, size_t length)
	{
		m_RegName.assign(value, length);
		m_HasAuthority = false;
	}

	// UserInfo
	bool HasUserInfo() const
	{
		return m_HasAuthority && m_Authority.HasUserInfo();
	}
	const std::string& UserInfo() const
	{
		assert(m_HasAuthority);
		return Authority().UserInfo();
	}

	void SetUserInfo(const std::string& value)
	{
		m_IsOpaque = false;
		m_HasAuthority = true;
		m_Authority.SetUserInfo(value);
	}

	void SetUserInfo(const char* value, size_t length)
	{
		std::string str(value, length);
		SetUserInfo(str);
	}

	void ClearUserInfo()
	{
		assert(m_HasAuthority);
		m_Authority.ClearUserInfo();
	}

	// Host
	bool HasHost() const
	{
		return m_HasAuthority;
	}

	const std::string& Host() const
	{
		assert(m_HasAuthority);
		return m_Authority.Host();
	}

	void SetHost(const std::string& value)
	{
		m_IsOpaque = false;
		m_HasAuthority = true;
		m_Authority.SetHost(value);
	}

	void SetHost(const char* value, size_t length)
	{
		std::string str(value, length);
		SetHost(str);
	}

	// Port
	bool HasPort() const
	{
		return m_HasAuthority && m_Authority.HasPort();
	}
	const std::string& Port() const
	{
		assert(m_HasAuthority);
		return m_Authority.Port();
	}

	void SetPort(const std::string& value)
	{
		m_IsOpaque = false;
		m_HasAuthority = true;
		m_Authority.SetPort(value);
	}

	void SetPort(const char* value, size_t length)
	{
		std::string str(value, length);
		SetPort(str);
	}

	void ClearPort()
	{
		assert(m_HasAuthority);
		m_Authority.ClearPort();
	}

	// Path
	const std::string& Path() const
	{
		return m_Path;
	}

	void SetPath(const std::string& value)
	{
		m_Path = value;
		m_IsOpaque = false;
	}

	void SetPath(const char* value, size_t length)
	{
		m_Path.assign(value, length);
		m_IsOpaque = false;
	}

	// Query
	bool HasQuery() const
	{
		return m_HasQuery;
	}

	const std::string& Query() const
	{
		return m_Query;
	}

	void SetQuery(const std::string& value)
	{
		m_Query = value;
		m_HasQuery = true;
		m_IsOpaque = false;
	}
	void SetQuery(const char* value, size_t length)
	{
		m_Query.assign(value, length);
		m_HasQuery = true;
		m_IsOpaque = false;
	}

	void ClearQuery()
	{
		m_Query.clear();
		m_HasQuery = false;
		m_IsOpaque = false;
	}

	// Fragment
	bool HasFragment() const
	{
		return m_HasFragment;
	}

	const std::string& Fragment() const
	{
		return m_Fragment;
	}

	void SetFragment(const std::string& value)
	{
		m_Fragment = value;
		m_HasFragment = true;
	}

	void SetFragment(const char* value, size_t length)
	{
		m_Fragment.assign(value, length);
		m_HasFragment = true;
	}

	void ClearFragment()
	{
		m_Fragment.clear();
		m_HasFragment = false;
	}

public: // operations
	std::string& ToString(std::string& Result) const;
	std::string ToString() const;
	bool ToString(char* buffer, size_t buffer_size, size_t& result_size) const;
	bool ToString(char* buffer, size_t buffer_size) const
	{
		size_t result_size;
		return ToString(buffer, buffer_size, result_size);
	}

	void Clear()
	{
		m_Scheme.clear();
		ClearAuthority();
		m_OpaquePart.clear();
		m_IsOpaque = true;
		m_Path.clear();
		ClearQuery();
		ClearFragment();
	}

	bool Normalize();
	bool ToAbsolute(const URI& base);

	bool operator == (const URI &other) const
	{
	    return m_IsOpaque == other.m_IsOpaque
		&& m_HasAuthority == other.m_HasAuthority
		&& m_HasQuery == other.m_HasQuery
		&& m_HasFragment == other.m_HasFragment
		&& m_Scheme == other.m_Scheme
		&& m_RegName == other.m_RegName
		&& m_Authority == other.m_Authority
		&& m_OpaquePart == other.m_OpaquePart
		&& m_Path == other.m_Path
		&& m_Query == other.m_Query
		&& m_Fragment == other.m_Fragment ;
	}
private:
	void StringLower(std::string& str)
	{
		for (size_t i = 0; i < str.length(); ++i)
			str[i] = tolower(str[i]);
	}
private:
	bool m_IsOpaque : 1;
	bool m_HasAuthority : 1;
	bool m_HasQuery : 1;
	bool m_HasFragment : 1;
	std::string m_Scheme;
	std::string m_RegName;
	UriAuthority m_Authority;
	std::string m_OpaquePart;
	std::string m_Path;
	std::string m_Query;
	std::string m_Fragment;
};


size_t UriParse(const char* Uri, size_t UriLength, URI& Result);

inline size_t UriParse(const char* Uri, URI& Result)
{
	return UriParse(Uri, strlen(Uri), Result);
}

inline size_t UriParse(const std::string& Uri, URI& Result)
{
	return UriParse(Uri.data(), Uri.length(), Result);
}

bool UriMerge(const URI& uri, const URI& base, URI& Result, bool strict = false);

bool HttpUriNormalize(URI& uri);

#endif//URI_HPP_INCLUDED

