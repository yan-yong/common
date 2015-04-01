/*
 * =====================================================================================
 *
 *       Filename:  TUtility.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2009-5-21 10:27:21
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Guo Shiwei (), guoshiwei@sohu-rd.com
 *        Company:  sohu
 *
 * =====================================================================================
 */
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <openssl/md5.h>
#include <iostream>
#include "httpparser/URI.hpp"
#include "httpparser/hlink.h"
#include "httpparser/Http.hpp"
#include "log/log.h"
#include "TUtility.hpp"
#include "httpparser/HttpMessage.hpp"
  
void PageDigestToHex(const char page_digest[PAGE_DIGEST_LEN], std::string& result)
{
    BinarayToHex(page_digest, PAGE_DIGEST_LEN, result);
}

void BinarayToHex(const char*pointer, size_t length, std::string &result)
{
    result.clear();
    result.reserve(length * 2);

    static const char hex_alphabet[] = "0123456789ABCDEF";
    for (size_t i = 0; i < length; ++i)
    {
        unsigned char c = static_cast<unsigned char>(pointer[i]);
        result.push_back(hex_alphabet[c >> 4]);
        result.push_back(hex_alphabet[c & 0x0F]);
    }
}

std::string md5_hex(const std::string& str)
{
    char hex[33];
    unsigned char md5[MD5_DIGEST_LENGTH];
    MD5((const unsigned char *)str.c_str(), str.length(), md5);

    char *p = hex;
    for(int i = 0; i < MD5_DIGEST_LENGTH; ++i){
        sprintf(p, "%.2X", md5[i]);
        p += 2;
    }

    *p = '\0';

    std::string ret(hex);
    return ret;
}

bool addr2IpPort(const std::string &addr, std::string &ip, unsigned short &port)
{
    std::string::size_type sep_pos = addr.find(':', 0);
    if(sep_pos == std::string::npos){
        return false;
    }

    ip = addr.substr(0, sep_pos);
    port = atoi(addr.substr(sep_pos + 1).c_str());

    return true;

}

const char* GetAddressString(const sockaddr* addr, char* addrstr, size_t addrstr_length)
{
    const char *ret = NULL;

    switch (addr->sa_family)
    {
        case AF_INET:
            ret = inet_ntop(addr->sa_family, &((struct sockaddr_in *)addr)->sin_addr, addrstr, addrstr_length);
            break;
        case AF_INET6:
            ret = inet_ntop(addr->sa_family, &((struct sockaddr_in6 *)addr)->sin6_addr, addrstr, addrstr_length);
            break;
        default:
            break;
    }

    if (!ret)
        ret = "0.0.0.0";

    return ret;
}

/// \return true (path exist and is a directory) or (create succeed).
/// \node do not create recursivly.
bool __createDir(const char *path)
{
    int ret = mkdir(path, 0777);
    if(ret == 0){
        return true;
    }
    if(ret < 0 && errno == EEXIST){
        return true;
    }

    return false;

}

bool createDir(const char * dir)
{
    const char * DIR_SEP = "/";
    const char * cur_sep = dir;

    struct stat stat_buf;

    while(cur_sep && (*(cur_sep++) != '\0')){

        cur_sep = strstr(cur_sep, DIR_SEP);

        std::string cur_dir;
        if(cur_sep)
            cur_dir.assign(dir, cur_sep - dir);
        else
            cur_dir.assign(dir);

        if(stat(cur_dir.c_str(), &stat_buf) < 0){
            if(errno != ENOENT || !__createDir(cur_dir.c_str())){
                return false;
            }
        }

    }

    return true;
}

size_t splitString(const char * str, const char *sep, std::vector<std::string> &result)
{
    const char *p = str;
    const char *q = str;

    size_t sep_len = strlen(sep);

    while((q = strstr(p, sep))){
        result.push_back(std::string(p, q - p));
        p = q + sep_len;
    }

    if(p){
        result.push_back(p);
    }
    return result.size();
}

/// Get ring_idx of mirror_id from subbatch_specifer
/// \note format: Example: "A:3,B:6,C:113"
int ringIdxFromSubbatchSpecifer(const char * subbatch_specifer, const char *mirror_id)
{
    const char *p = strstr(subbatch_specifer, mirror_id);
    if(!p){
        return -1;
    }

    p = strchr(p, ':');
    if(!p){
        return -1;
    }
    if(strlen(p) < 2){
        return -1;
    }

    ++p;

    const char *q = strchr(p, ',');
    if(q){
        return atoi(std::string(p, q - p).c_str());
    }
    else{
        return atoi(p);
    }
}

bool taskValidation(const char *url, URI &uri)
{
    if(!url){
        LOG_ERROR("%s: task url is NULL\n", __FUNCTION__);
        return false;
    }

    if (!UriParse(url, strlen(url), uri)){
        LOG_ERROR("%s: UriParse [%s] failed\n", __FUNCTION__,
                url);
        return false;
    }

    if(!HttpUriNormalize(uri)) {
        LOG_ERROR("%s: HttpUriNormalize [%s] failed\n", __FUNCTION__,
                url);
        return false;
    }

    return true;
}

/// return true if str is not NULL and strlen(str)>0
bool noneEmptyStr(const char * str)
{
    return str && (*str != '\0');
}

//void tolower(std::string &str)
//{
//    for(size_t i = 0; i < str.size(); ++i){
//        str[i] = ::tolower(str[i]);
//    }
//}

bool isHtml(const Response& response)
{
    int index = response.Headers.Find("Content-Type");
    if (index >= 0)
    {
        if (response.Headers[index].Value.find("text/html") != std::string::npos)
            return true;
    }


    const char* body = &response.Body[0];
    size_t body_size = response.Body.size();
    while (body_size > 0 && isspace(*body))
    {
        body++;
        body_size--;
    }
    if(body_size >= 6 && strncasecmp(body, "<!", 2) == 0)
    {

        while (body_size > 0 )
        {
            if(*body == '>')
            {
                body++;
                body_size--;
                break;
            }
            body++;
            body_size--;
        }
        while (body_size > 0 && isspace(*body))
        {
            body++;
            body_size--;
        }

    }
    return body_size >= 6 && strncasecmp(body, "<html", 5) == 0;
}

/// If 'range' is byte-range, do nothing;
/// If 'range' is suffix-range, convert it to a byte-range.
/// 
/// @note, if content_length is not large enough for the suffix-range,
/// range is set to 0-(content_length-1)
std::string &modifyRange(std::string &range, size_t content_length)
{
    if(!range.empty() && range[0] == '-' && content_length > 0){
        // A suffix range, some server do not suport this,
        // so modify it to a bytes-range.
        int suffix_bytes = atoi(range.c_str());
        int range_beg = content_length + suffix_bytes;
        if(range_beg < 0)
            range_beg = 0;

        char bytes_range[64];
        snprintf(bytes_range, sizeof(bytes_range),
                "%d-%lu", range_beg, content_length - 1);
        range = bytes_range;
    }

    return range;
}

// test the range string match bytes-range format (used for sender conf)
bool is_range_valid(std::string& range)
{
    if(range.empty())
        return false;

    if( range.find(" ") != std::string::npos )
        return false;

    char tmp[512];
    char* tmp_p=(char*)tmp;
    int ret = sscanf(range.c_str(),"bytes=%s",tmp_p); 
    if( ret != 1)
    {
        return false;
    }

    while( (*tmp_p >= '0'&& *tmp_p <='9') || (*tmp_p) == ',' || (*tmp_p) == '-') tmp_p++;
    if( *tmp_p != '\0')
        return false;

    int num1,num2;
    size_t pos;
    std::string str1,str2,tmp_str;

    pos = range.find("=");
    if( pos == std::string::npos)
    {
        return false;
    }

    tmp_str = range.substr(pos+1);
    pos = tmp_str.find(",");
    while( pos != std::string::npos)
    {
        str1 = tmp_str.substr(0,pos);
        str2 = tmp_str.substr(pos+1);
        ret = sscanf(str1.c_str(),"%d-%d",&num1,&num2);
        if( ret != 2)
        {
            return false;
        }
        if( num1 > num2)
            return false;
        tmp_str = str2;
        pos = str2.find(",");
    }

    ret = sscanf(tmp_str.c_str(),"-%d",&num1);
    if( ret != 1)
    {
        ret = sscanf(tmp_str.c_str(),"%d-%d",&num1,&num2);
        if( ret != 2)
        {
            return false;
        }
        if( num1 > num2)
            return false;
    }

    return true;
}

size_t getContentLength(const MessageHeaders &headers)
{
    int index = headers.Find("Content-Length");
    if(index >=0){
        return atol(headers[index].Value.c_str());
    }
    return 0;
}
size_t getContentWholeLength(const MessageHeaders &headers)
{
    int index = headers.Find("Content-Range");
    if(index >= 0){
        const std::string &range = headers[index].Value;
        std::string::size_type pos = range.find("/");
        if(pos != std::string::npos
                && (++pos) != range.length()){
            return atol(range.c_str() + pos );
        }
    }

    index = headers.Find("Content-Length");
    if(index >=0){
        return atol(headers[index].Value.c_str());
    }
    return 0;
}

std::string getHostName(const char *ip, int flag)
{
    char host[NI_MAXHOST];
    bzero(host, sizeof(host));

    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = 0;
    if(inet_aton(ip, &sa.sin_addr) == 0){
        LOG_ERROR("%s: inet_aton errno == %d, %s\n", __FUNCTION__, 
                errno, strerror(errno));
        return "";
    }

    if(getnameinfo((sockaddr*)&sa, sizeof(sa), host, sizeof(host),
                NULL, 0, flag)){
        LOG_ERROR("%s: getnameinfo errno == %d, %s\n", __FUNCTION__, 
                errno, strerror(errno));
        return "";
    }

    return host;
}

std::string dirname(const char *path)
{
    const char *p = strrchr(path, '/');
    if(p == NULL){
        // no dir part, 
        return ".";
    }

    if(p == path){
        // '/' is root path
        return "/";
    }

    return std::string(path, p - path);
}

// remove \r or \n in end of line
char * chomp(char *line)
{
    if(!line){
        return NULL;
    }

    char *p = line + strlen(line);
    --p;
    while(p >= line &&(*p == '\r' || *p == '\n' || isspace(*p))){
        *p-- = '\0';
    }

    return line;
}

bool hasCRLF(const char *str)
{
    if(!str){
        return false;
    }
    const char *p = str;
    while(*p){
        if(*p == '\n' || *p == '\r')
            return true;

        ++p;
    }

    return false;
}

bool getRedirectUrl(const MessageHeaders &headers, std::string &to)
{
    // find location
    int index = headers.Find("Location");
    if (index < 0)
    {
        return false;
    }

    to = headers[index].Value;
    to = to.empty()?  "./" : to;
    return true;
}

bool mergeUrl(const URI &from, const std::string &to_url, URI &result)
{
    URI target;
    if (!UriParse(to_url, target))
    {
        return false;
    }

    if (!UriMerge(target, from, result) || !HttpUriNormalize(result))
    {
        return false;
    }

    return true;
}
