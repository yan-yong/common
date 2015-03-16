#ifndef HTML_ELEMENT_EXTRACTOR_HPP
#define HTML_ELEMENT_EXTRACTOR_HPP

#include <vector>
#include <map>
#include <set>
#include <string>
#include <cctype>

#include "HtmlParser.hpp"
#include "URI.hpp"

struct IHtmlElementExtractorEvents
{
    virtual ~IHtmlElementExtractorEvents(){}
    virtual void OnFindElement(const HtmlElement& Element) = 0;
};

class HtmlElementExtractor : private IHtmlParserEvents
{
public:
	HtmlElementExtractor(IHtmlElementExtractorEvents& EventsSink) :
		ToUpper(GetUpperMap()),
		ToLower(GetLowerMap()),
		m_HtmlParser(*this),
		m_EventsSink(&EventsSink),
		m_InStyle(false),
		m_NoScript(false)
	{
	}

	virtual ~HtmlElementExtractor()
	{
	}

	void Add(const std::string& ElementName, bool HasContent)
	{
		if (HasContent)
		{
			if (m_Elements.size() <= ElementName.size())
				m_Elements.resize(ElementName.size() + 1);
			m_Elements[ElementName.size()].insert(ElementName);
		}
		else
		{
			if (m_NoContentElements.size() <= ElementName.size())
				m_NoContentElements.resize(ElementName.size() + 1);
			m_NoContentElements[ElementName.size()].insert(ElementName);
		}
	}

	size_t Parse(const void* content, size_t size)
	{
		m_CurrentElement.clear();
		m_DetectingElement.Name.clear();
		m_DetectingElement.Attributes.clear();
		m_DetectingElement.Content.clear();
		m_InStyle = false;
		m_NoScript = false;
		return m_HtmlParser.Parse(content, size);
	}

private:
	virtual void OnStartTag(const char* Name, size_t Length)
	{
		if (Length == 5 && strncasecmp(Name, "STYLE", 5) == 0)
		{
			m_InStyle = true;
			return;
		}
		else if (Length == 8 && strncasecmp(Name, "NOSCRIPT", 8) == 0)
		{
			m_NoScript = true;
			return;
		}
		if(m_NoScript)
		{
			return;
		}

		if (Length < m_Elements.size())
		{
			const std::set<std::string>& s = m_Elements[Length];
			if (!s.empty())
			{
				m_CurrentElement.assign(Name, Length);
				StringCaseUniform(m_CurrentElement, ToUpper);
				std::set<std::string>::const_iterator i = s.find(m_CurrentElement);
				if (i != s.end())
				{
					m_DetectingElement.Name = m_CurrentElement;
					m_DetectingElement.Attributes.clear();
					m_DetectingElement.Content.clear();

					//09-12
					m_DetectingElement.Code.clear();
					m_code_start = Name; 
					return;
				}
			}
		}

		if (Length < m_NoContentElements.size())
		{
			const std::set<std::string>& s = m_NoContentElements[Length];
			if (!s.empty())
			{
				m_CurrentElement.assign(Name, Length);
				StringCaseUniform(m_CurrentElement, ToUpper);
				std::set<std::string>::const_iterator i = s.find(m_CurrentElement);
				if (i != s.end())
				{
					m_DetectingElement.Name = m_CurrentElement;
					m_DetectingElement.Attributes.clear();
					m_DetectingElement.Content.clear();
					//09-12
					m_DetectingElement.Code.clear();
					m_code_start = Name;
				}
			}
		}
		//09-12
		m_CurrentElement.assign(Name, Length);
		StringCaseUniform(m_CurrentElement, ToUpper);
	}

	virtual void OnStartTagClose(const char* p)
	{
		if(m_NoScript)
		{
			return;
		}
		size_t Length = m_DetectingElement.Name.length();
		if (!m_DetectingElement.Name.empty() && Length < m_NoContentElements.size())
		{
			const std::set<std::string>& s = m_NoContentElements[Length];
			if (!s.empty())
			{
				std::set<std::string>::const_iterator i = s.find(m_CurrentElement);
				if (i != s.end())
				{
					//09-12
					if(m_code_start)
					{
						std::string code="<";
						const char* temp = m_code_start;
						while( (temp = m_code_start++)  != p)
							code += *temp;

						m_DetectingElement.Code = code+*p;
						m_code_start = NULL;
					}

					m_EventsSink->OnFindElement(m_DetectingElement);
					m_DetectingElement.Name = m_CurrentElement;
					m_DetectingElement.Attributes.clear();
					m_DetectingElement.Content.clear();
					//09-12
					m_DetectingElement.Code.clear();
				}
			}
		}
	}

	virtual void OnEndTag(const char* Name, size_t Length)
	{
		if (Length == 8 && strncasecmp(Name, "NOSCRIPT", 8) == 0)
		{
			m_NoScript = false;
		}


		m_InStyle = false;
		if (!m_DetectingElement.Name.empty())
		{
			if (Length != 0)
			{
				m_CurrentElement.assign(Name, Length);
				StringCaseUniform(m_CurrentElement, ToUpper);
				if(m_CurrentElement != m_DetectingElement.Name)
				{
					return;
				}
			}
			else{

				if(m_CurrentElement != m_DetectingElement.Name)
				{
					m_CurrentElement.clear();
					return;
				}
			}

			//09-12
			if (m_CurrentElement == m_DetectingElement.Name && m_code_start)
			{
				std::string code="<";
				const char* p = m_code_start;
				const char* end = Name;
				while( *end++ != '>');
				while( (p = m_code_start++) != end )
					code += *p;

				m_DetectingElement.Code = code;
				m_code_start = NULL;
				m_CurrentElement.clear();
			}
			else
			{
				return;
			}

			m_EventsSink->OnFindElement(m_DetectingElement);
			m_DetectingElement.Name.clear();
			m_DetectingElement.Code.clear();
		}
	}
	virtual void OnAttribute(
		const char* Name, size_t NameLength,
		const char* Value, size_t ValueLength
	)
	{
		if (!m_DetectingElement.Name.empty() && m_DetectingElement.Name == m_CurrentElement)
		{
			HtmlAttribute Attribute;
			Attribute.Name.assign(Name, NameLength);
			StringCaseUniform(Attribute.Name, ToLower);
			if (ValueLength)
				Attribute.Value.assign(Value, ValueLength);
			m_DetectingElement.Attributes.push_back(Attribute);
		}
	}

	virtual void OnPlainText(const char* Text, size_t Length)
	{
		if (!m_DetectingElement.Name.empty() && !m_InStyle)
			m_DetectingElement.Content.append(Text, Length);
	}

private:
	template <typename Pred>
	static void StringCaseUniform(std::string& s, Pred pred)
	{
		size_t length = s.length();
		for (size_t i = 0; i < length; ++i)
		{
			s[i] = pred(s[i]);
		}
	}
private:
	const CharMap& ToUpper;
	const CharMap& ToLower;
	HtmlParser m_HtmlParser;
	IHtmlElementExtractorEvents* m_EventsSink;
	std::vector<std::set<std::string> > m_Elements;
	std::vector<std::set<std::string> > m_NoContentElements;
	std::string m_CurrentElement;
	HtmlElement m_DetectingElement;
	bool m_InStyle;
	bool m_NoScript;
    //09-12
    const char* m_code_start;
};

#endif//HTML_ELEMENT_EXTRACTOR_HPP

