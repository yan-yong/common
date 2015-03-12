#ifndef _URI_H_
#define _URI_H_

#define AT_SERVER		1
#define AT_REG_NAME		2

#define C_SCHEME		0001
#define C_AUTHORITY		0036
#define C_USERINFO		0002
#define C_HOST			0004
#define C_PORT			0010
#define C_REG_NAME		0020
#define C_PATH			0040
#define C_QUERY			0100
#define C_FRAGMENT		0200
#define C_URI			0377

struct authority
{
	int type;
	union
	{
		struct
		{
			char *userinfo;
			char *host;
			char *port;
		};
		char *reg_name;
	};
};

struct uri
{
	char *scheme;
	struct authority *authority;
	char *path;
	char *query;
	char *fragment;
};

extern char __hex2char[];
extern char __char2hex[];
extern char __uri_chr[];

static inline int hex2char(int h)
{
	return __hex2char[(h) & 0x7f];
}

static inline int char2hex(int c)
{
	return __char2hex[(c) & 0x0f];
}

static inline int is_uri_chr(int c)
{
	unsigned char tmp = (c);
	return tmp < 128 && __uri_chr[tmp >> 3] & 0x80 >> (tmp & 0x07);
}

#ifdef __cplusplus
extern "C"
{
#endif

int uri_parse_string(const char *string, struct uri *uri);
int uri_parse_bytes(const char *bytes, int len, struct uri *uri);
int uri_parse_buffer(char *buffer, unsigned int size, struct uri *uri);
void uri_destroy(struct uri *uri);
int uri_length_string(const char *string);
int uri_length_bytes(const char *bytes, int len);
int uri_length_buffer(char *base, unsigned int size);
int uri_merge(const struct uri *rel_uri, const struct uri *base_uri,
			  struct uri *result);
int uri_recombine(const struct uri *uri, char *uristr, unsigned int n,
				  int flags);
int uri_escape(const char *bytes, unsigned int len,
			   char *escstr, unsigned int n);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
class URI;
bool URI_to_uri(const URI& new_uri, struct uri* old_uri);
bool uri_to_URI(const struct uri* old_uri, URI& new_uri);
#endif

#endif

