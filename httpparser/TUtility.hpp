/*
 * =====================================================================================
 *
 *       Filename:  TUtility.hpp
 *
 *    Description:  Utilitys
 *
 *        Version:  1.0
 *        Created:  2009-5-21 10:24:29
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Guo Shiwei (), guoshiwei@sohu-rd.com
 *        Company:  sohu
 *
 * =====================================================================================
 */
#ifndef  __TUTILITY_INC
#define  __TUTILITY_INC

// STD C headers
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

// STD C++ headers
#include <string>
#include <vector>
#include "httpparser/HttpMessage.hpp"

#define PAGE_DIGEST_LEN 8

// Forward declares
class Response;
class MessageHeaders;
class URI;
/// A class that cannot be changed after construct.
template <typename T>
class UnChangable
{
    private:
        T m_val;

    public:
        UnChangable(const T &val):m_val(val){}
        operator T() const { return m_val;}

    private:
        void operator = (const UnChangable &v);
};

void PageDigestToHex(const char page_digest[PAGE_DIGEST_LEN], std::string& result);
void BinarayToHex(const char *pointer, size_t length, std::string& result);
std::string md5_hex(const std::string& str);

/// Convert addr to string format.
/// \return str pointing to addrstr
const char* GetAddressString(const sockaddr* addr, char* addrstr, size_t addrstr_length);

/// Get ip, port from string format addr like '127.0.0.1:80'
bool addr2IpPort(const std::string &addr, std::string &ip, unsigned short &port);

/// Create dir if it is not exist.
/// return true if succeed.
bool createDir(const char *dir);

/// get dir part of path, last '/' not included.
std::string dirname(const char *path);

/// Split string 'str' using seperator string 'sep', 
/// save result to result
///
/// \return result.size()
size_t splitString(const char * str, const char *sep, std::vector<std::string> &result);

/// Get ring_idx of mirror_id from subbatch_specifer
/// \note format: Example: "A:3,B:6,C:113"
int ringIdxFromSubbatchSpecifer(const char * subbatch_specifer, const char *mirror_id);

/// Check wether task is valid
/// If check passed, the URI form of task is returnd, 
/// If failed, empty URI ptr returnd, and reason is set.
bool taskValidation(const char *url, URI &uri);

/// return true if str is not NULL and strlen(str)>0
bool noneEmptyStr(const char * str);

//void tolower(std::string &str);

/// \return true if response is HTML page
bool isHtml(MessageHeaders& headers, std::vector<char>& decode_body);

/// If 'range' is byte-range or content_length == 0, do nothing;
/// If 'range' is suffix-range, convert it to a byte-range.
/// 
/// @note If content_length is not large enough for the suffix-range,
/// range is set to 0-(content_length-1)
std::string &modifyRange(std::string &range, size_t content_length);

bool is_range_valid(std::string& range);

/// Return the length of the whole resource this header says,
/// that is, if this a response of 'Range' req, we return the length of the
/// whole resource, not the Range length.
size_t getContentWholeLength(const MessageHeaders &headers);

/// Return the value of 'Content-Length'
size_t getContentLength(const MessageHeaders &headers);

/// Get host name of ip,
/// @param flag @see man getnameinfo
std::string getHostName(const char *ip, int flag);
// remove space, nonprint char, \r or \n in end of line.
char * chomp(char *line);
bool hasCRLF(const char *str);
bool getRedirectUrl(const MessageHeaders &headers, std::string &to);
bool mergeUrl(const URI &from, const std::string &to_url, URI &result);

#endif
