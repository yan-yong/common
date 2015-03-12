#ifndef HTML_ELEMENT_PARSER_HPP
#define HTML_ELEMENT_PARSER_HPP

#include "HtmlElementExtractor.hpp"
#include<iostream>

struct IHtmlLinkParserEvents
{
	virtual ~IHtmlLinkParserEvents(){}

	virtual void OnFindLink(const HtmlElement& Element)
	{
	}
	virtual void OnBaseChanged(const std::string& base)
	{
	}

	virtual void OnMeta(const HtmlElement& element)
	{
	}
	
	virtual void OnTitle(const HtmlElement& title)
	{
	}
};

class HtmlLinkParser : private IHtmlElementExtractorEvents
{
	public:
		HtmlLinkParser(IHtmlLinkParserEvents& EventsSink) : 
			m_ElementExtractor(*this),
			m_EventsSink(&EventsSink)
	{
		m_ElementExtractor.Add("META", false);
		m_ElementExtractor.Add("BASE", false);
		m_ElementExtractor.Add("FRAME", true);
		m_ElementExtractor.Add("IFRAME", true);
		m_ElementExtractor.Add("TITLE", true);
	}

		virtual ~HtmlLinkParser(){}

		size_t Parse(const void* Document, size_t DocumentSize)
		{
			return m_ElementExtractor.Parse(Document, DocumentSize);
		}

		void Add(const std::string& tag, const std::string attribute, bool has_content)
		{
			if (tag != "BASE")
			{
				m_ElementExtractor.Add(tag, has_content);
				m_LinkCheckers[tag] = attribute;
			}
		}


		//test
		int GetNum()
		{
			//std::string temp;
			return m_num;        
		}

	private:
#if 0
		// just for debug
		void DumpElement(const HtmlElement& Element) const
		{
			std::cout << "<" << Element.Name << " ";
			for (int i = 0; i < Element.Attributes.size(); ++i)
			{
				const HtmlAttribute a = Element.Attributes[i];
				std::cout << a.Name << "=\"";
				std::cout << a.Value << "\" ";
			}
			std::cout << ">" << Element.Content << "</" << Element.Name << ">";
			std::cout << "\n\n";
		}
#endif

		virtual void OnFindElement(const HtmlElement& Element)
		{
			m_num = 1;
			//DumpElement(Element);
			if (Element.Name.length() == 4 && Element.Name == "BASE")
			{
				for (size_t i = 0; i < Element.Attributes.size(); ++i)
				{
					if (Element.Attributes[i].Name == "href")
					{
						m_EventsSink->OnBaseChanged(Element.Attributes[i].Value);
						break;
					}
				}
			}
			else if (Element.Name.length() == 4 && Element.Name == "META")
			{
				m_EventsSink->OnMeta(Element);
			}
			else if (Element.Name.length() == 5 && Element.Name == "TITLE"){
				m_EventsSink->OnTitle(Element);
			}
			else
			{
				std::map<std::string, std::string>::iterator it = m_LinkCheckers.find(Element.Name);
				if (it != m_LinkCheckers.end())
				{
					bool nof = false;
					bool hasl = false;
					std::string relname = "rel";
					std::string relvalue = "nofollow";
					std::string lname;
					std::string lvalue;
					for (size_t i = 0; i < Element.Attributes.size(); ++i)
					{
						if (Element.Attributes[i].Name == it->second)
						{
							hasl = true;
							lname = Element.Attributes[i].Name;
							lvalue = Element.Attributes[i].Value;
						}else if(Element.Attributes[i].Name == relname && Element.Attributes[i].Value == relvalue){
							nof = true;
							break;
						}	
					}
					if( hasl && !nof ){
						//m_EventsSink->OnFindLink(
						//		Element.Name, lname,
						//		lvalue, Element.Content
						//		);
						m_EventsSink->OnFindLink(Element);
					}
				}
			}
		}
private:
		int m_num;
		HtmlElementExtractor m_ElementExtractor;
		IHtmlLinkParserEvents* m_EventsSink;
		std::map<std::string, std::string> m_LinkCheckers;
};

#endif//HTML_ELEMENT_PARSER_HPP

