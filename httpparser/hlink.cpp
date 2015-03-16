#include "hlink.h"
#include "URI.hpp"
#include "Http.hpp"

//#include "HtmlLinkParser.hpp"
#include "HtmlEntity.hpp"

#include <iostream>

static char *__attr_href[] = { (char*)"href", NULL };
static char *__attr_src[] = { (char*)"src", NULL };

const struct hlink_elem __hlink_elem_default[] = {
	{ (char*)"A", __attr_href },
	{ (char*)"AREA", __attr_href },
	{ (char*)"BASE", __attr_href },
	{ (char*)"FRAME", __attr_src },
	{ (char*)"IFRAME", __attr_src },
	{ (char*)"LINK", __attr_href },
	{ NULL }
};

const struct hlink_elem __hlink_elem_frame[] = {
	{ (char*)"BASE", __attr_href },
	{ (char*)"FRAME", __attr_src },
	{ (char*)"IFRAME", __attr_src },
	{ NULL }
};

const struct hlink_elem __hlink_elem_empty[] = { { NULL } };

class HtmlLinkParseResult : public IHtmlLinkParserEvents
{
	public:
		HtmlLinkParseResult(
				const URI* base_uri,
				MessageHeaders* http_equiv,
				std::string* title,
				FindLinkCallbackType find_link_callback,
				void* context
				):
			m_original_base_uri(base_uri),
			m_base_uri(base_uri),
			m_http_equiv(http_equiv),	
			m_title(title),
			m_find_link_callback(find_link_callback),
			m_find_link_callback_context(context)
	{
			if(m_title)
				m_title->clear();
	}

		virtual ~HtmlLinkParseResult()
		{
		}

		//09-12
		void Add(const std::string& tag, const std::string attribute, bool has_content)
		{
			if (tag != "BASE")
			{
				m_LinkCheckers[tag] = attribute;
			}
		}

	private:
		virtual void OnFindLink(const HtmlElement& Element)
		{
			size_t i = 0;

			//09-12
			std::map<std::string, std::string>::iterator it = m_LinkCheckers.find(Element.Name);
			if (it != m_LinkCheckers.end())
			{
				for (i = 0; i < Element.Attributes.size(); ++i)
					if (Element.Attributes[i].Name == it->second)
						break;
			}
			std::string url = Element.Attributes[i].Value;

			std::string uniform_url;
			UrlUniform(url, uniform_url);
			URI uri;
			if (UriParse(uniform_url.data(), uniform_url.size(), uri))
			{
				if (!uri.Scheme().empty() || !m_base_uri)
				{
					if(m_find_link_callback)
					{
						m_find_link_callback(Element, m_find_link_callback_context);
					}
				}
				else
				{
					URI abs_uri;
					if (m_base_uri && UriMerge(uri, *m_base_uri, abs_uri))
					{
						if(m_find_link_callback)
						{
							//09-12
							HtmlElement temp = Element;
							temp.Attributes[i].Value = abs_uri.ToString();

							m_find_link_callback(temp, m_find_link_callback_context);
						}
					}
				}
			}
		}

		virtual void OnBaseChanged(const std::string& base)
		{
			std::string uniform_url;
			UrlUniform(base, uniform_url);

			if (m_base_uri != &m_new_base_uri) // first time
			{
				UriParse(uniform_url.data(), uniform_url.size(), m_new_base_uri);
				m_base_uri = &m_new_base_uri;
			}
			else
			{
				UriParse(uniform_url.data(), uniform_url.size(), m_new_base_uri);
			}
		}

		virtual void OnMeta(const HtmlElement& element)
		{
			if (m_http_equiv)
			{
				const std::string* http_equiv = NULL;
				const std::string* content = NULL;
				for (size_t i = 0; i < element.Attributes.size(); ++i)
				{
					if (element.Attributes[i].Name == "http-equiv")
						http_equiv = &element.Attributes[i].Value;
					else if (element.Attributes[i].Name == "content")
						content = &element.Attributes[i].Value;
				}

				if (http_equiv && content)
				{
					m_http_equiv->Add(*http_equiv, *content);
				}
			}
		}

		virtual void OnTitle(const HtmlElement& element){
			if(m_title && m_title->empty() && !element.Content.empty()){
				*m_title = element.Content;
			}
		}

	private:
		const URI* m_original_base_uri;
		const URI* m_base_uri;
		URI m_new_base_uri;
		//meta
		MessageHeaders* m_http_equiv;
		//title
		std::string* m_title;
		FindLinkCallbackType m_find_link_callback;
		void* m_find_link_callback_context;
		//09-12
		std::map<std::string, std::string> m_LinkCheckers;
};

int ParseHtmlLink(
		const char *bytes, size_t len,
		const URI* base_uri,
		const struct hlink_elem *elems,
		MessageHeaders* http_equiv,
		std::string* title,
		FindLinkCallbackType find_link_callback,
		void *context
		)
{
	HtmlLinkParseResult r(base_uri, http_equiv, title, find_link_callback, context);
	HtmlLinkParser p(r);
	while (elems && elems->name)
	{
		if (elems->attrs[0])
		{
			p.Add(elems->name, elems->attrs[0], elems->name[0] == 'A' && elems->name[1] == '\0');
			//09-12
			r.Add(elems->name, elems->attrs[0], elems->name[0] == 'A' && elems->name[1] == '\0');
		}
		++elems;
	}
	return p.Parse(bytes, len);
}

  bool parseHtmlMetaRefresh(const std::string &target_url,
          std::string &result)
  {
      const char* p = target_url.c_str();
      p += strspn(p, " \t");

      int timeout = atoi(p);
      if (timeout < 10)
      {
          p += strspn(p, "0123456789 \t.;");

          if(strncasecmp(p, "url=", 4) == 0)
          {
              p += 4;
              p += strspn(p, " \t;");
              char tempchar = *p;
              if ( tempchar == '\'' || tempchar == '"')
              {
                  result.clear();
                  ++p;
                  while(*p != tempchar && *p != '\0')
                  {
                      result.push_back(*p);
                      p++;
                  }

                  return true;
              }
              else{
                  result = p;
                  return true;
              }
          }
      }

      return false;
  }

  bool metaRedirct(const URI &from_uri, const char *html_page, size_t page_size, std::string &result)
  {
      MessageHeaders meta_headers;

      ParseHtmlLink(html_page, page_size,
              &from_uri, NULL,
              &meta_headers, NULL,
              NULL, NULL);

      if (meta_headers.Empty()){
          return false;
      }

      int index = meta_headers.Find("Refresh");
      if (index < 0)
      {
          return false;
      }

      return parseHtmlMetaRefresh(meta_headers[index].Value, result);
  }


