#ifndef _HLINK_H_
#define _HLINK_H_

#include <vector>

#define HLINK_ELEM_DEFAULT		__hlink_elem_default
#define HLINK_ELEM_FRAME		__hlink_elem_frame
#define HLINK_ELEM_EMPTY		__hlink_elem_empty

#include "HtmlLinkParser.hpp"

// Forward declares
class URI;
class MessageHeaders;

struct hlink_elem
{
	char *name;
	char **attrs;
};

extern const struct hlink_elem __hlink_elem_default[];
extern const struct hlink_elem __hlink_elem_frame[];
extern const struct hlink_elem __hlink_elem_empty[];

typedef int (*FindLinkCallbackType)(const HtmlElement& Element, void * context);

int ParseHtmlLink(
	const char *bytes, size_t len,
	const URI* base_uri,
	const struct hlink_elem *elems,
	MessageHeaders* meta_headers,
	std::string* title,
	FindLinkCallbackType find_link_callback,
	void *context
);


/// \param result redirect result.
/// \return true If html_page is redirected
bool metaRedirct(const URI &from_uri, const char *html_page, size_t page_size, std::string &result);

/// @param  target_url The content field of HTML meta header:
///         '<meta http-equiv="refresh" content="0.1; url=http://www.shdf.gov.cn/portal/index.html">'
/// @param result Out value, dest url parsed.
/// @return True if parse successed.
bool parseHtmlMetaRefresh(const std::string &target_url, std::string &result);

#endif

