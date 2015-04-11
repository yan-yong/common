#ifndef FAST_CTYPE_HPP_INCLUDED
#define FAST_CTYPE_HPP_INCLUDED
#include <ctype.h>
#include <limits.h>
#include <memory.h>
#include <algorithm>
#include <functional>

//template <class Pred>
class CharSet
{
public:
	CharSet()
	{
		memset(m_bitmap, 0, sizeof(m_bitmap));
	}

	template <class Pred>
    explicit CharSet(const Pred& pred)
    {
		memset(m_bitmap, 0, sizeof(m_bitmap));
        for (int c = 0; c <= UCHAR_MAX; ++c)
        {
            if (pred(c))
                set(static_cast<unsigned char>(c));
        }
    }

    explicit CharSet(const char* str)
    {
		memset(m_bitmap, 0, sizeof(m_bitmap));
        while (*str)
        {
			set(*str);
			++str;
        }
    }

	template <class Pred>
	void insert(const Pred& pred)
    {
        for (int c = 0; c <= UCHAR_MAX; ++c)
        {
            if (pred(c))
                set(c);
        }
    }

    void insert(const char* str)
    {
        while (*str)
        {
			set(*str);
			++str;
        }
    }

	template <typename Pred>
	void remove(const Pred& pred)
    {
        for (int c = 0; c <= UCHAR_MAX; ++c)
        {
            if (pred(c))
                clear(c);
        }
    }

    void remove(const char* str)
    {
        while (*str)
        {
			clear(*str);
			++str;
        }
    }

	bool find(unsigned char c) const
	{
		return (m_bitmap[c/CHAR_BIT] >> (c%CHAR_BIT)) & 1;
	}

    bool operator()(unsigned char c) const
    {
		return find(c);
    }

	CharSet& operator|=(const CharSet& rhs)
	{
		for (size_t i = 0; i < sizeof(m_bitmap)/sizeof(m_bitmap[0]); ++i)
			m_bitmap[i] |= rhs.m_bitmap[i];
		return *this;
	}

	CharSet& operator&=(const CharSet& rhs)
	{
		for (size_t i = 0; i < sizeof(m_bitmap)/sizeof(m_bitmap[0]); ++i)
			m_bitmap[i] &= rhs.m_bitmap[i];
		return *this;
	}

	CharSet& operator|=(const char* rhs)
	{
		insert(rhs);
		return *this;
	}

private:
	void set(unsigned char n)
	{
		m_bitmap[n/CHAR_BIT] |= (1U << (n%CHAR_BIT));
	}
	void clear(unsigned char n)
	{
		m_bitmap[n/CHAR_BIT] &= ~(1U << (n%CHAR_BIT));
	}
private:
	unsigned char m_bitmap[(UCHAR_MAX + 1 + CHAR_BIT - 1) / CHAR_BIT];
};

inline const CharSet operator|(const CharSet& lhs, const CharSet& rhs)
{
	return CharSet(lhs) |= rhs;
}

inline const CharSet operator|(const CharSet& lhs, const char* rhs)
{
	CharSet r(lhs);
	return  r |= rhs;
}

inline const CharSet operator&(const CharSet& lhs, const CharSet& rhs)
{
	CharSet r(lhs);
	return r &= rhs;
}

class CharMap
{
public:
    CharMap()
    {
        for (int c = 0; c <= UCHAR_MAX; ++c)
        {
            m_chars[c] = static_cast<unsigned char>(c);
        }
    }

    template <typename Pred>
    explicit CharMap(Pred pred)
    {
        for (int c = 0; c <= UCHAR_MAX; ++c)
        {
            m_chars[c] = static_cast<unsigned char>(pred(c));
        }
    }
public:
	unsigned char& operator[](unsigned char c)
	{
		return m_chars[c];
	}

	const unsigned char& operator[](unsigned char c) const
	{
		return m_chars[c];
	}
	unsigned char operator()(unsigned char c) const
	{
		return m_chars[c];
	}
private:
	unsigned char m_chars[UCHAR_MAX + 1];
};

inline const CharSet& GetSpaceSet()
{
	static const CharSet cs(::isspace);
	return cs;
}

inline const CharSet& GetAlphaSet()
{
	static const CharSet cs(::isalpha);
	return cs;
}

inline const CharSet& GetAlphaNumSet()
{
	static const CharSet cs(::isalnum);
	return cs;
}

inline const CharSet& GetAsciiSet()
{
	static const CharSet cs(::isascii);
	return cs;
}

inline const CharSet& GetHexSet()
{
	static const CharSet cs(::isxdigit);
	return cs;
}

inline const CharSet& GetDigitSet()
{
	static const CharSet cs(::isdigit);
	return cs;
}

inline const CharSet& GetUpperSet()
{
	static const CharSet cs(::isupper);
	return cs;
}

inline const CharSet& GetLowerSet()
{
	static const CharSet cs(::islower);
	return cs;
}

inline const CharSet& GetPrintSet()
{
	static const CharSet cs(::isprint);
	return cs;
}

inline const CharMap& GetUpperMap()
{
	static const CharMap map(::toupper);
	return map;
}

inline const CharMap& GetLowerMap()
{
	static const CharMap map(::tolower);
	return map;
}

#endif//FAST_CTYPE_HPP_INCLUDED
