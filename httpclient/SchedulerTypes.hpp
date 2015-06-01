#ifndef __SCHEDULER_TYPES_HPP
#define	__SCHEDULER_TYPES_HPP

#include <time.h>
#include <stddef.h>
#include <sys/param.h>
#include <string>
#include <boost/function.hpp>
#include "httpparser/URI.hpp"
#include "httpparser/HttpMessage.hpp"
#include "linklist/linked_list.hpp"
#include "linklist/linked_list_map.hpp"
#include "fetcher/Fetcher.hpp"

enum ResourcePriority 
{
    //第一个不用
    RES_PRIORITY_NOUSE = 0,
    RES_PRIORITY_LEVEL_1,
    RES_PRIORITY_LEVEL_2,
    RES_PRIORITY_LEVEL_3,
    RES_PRIORITY_LEVEL_4,
    RES_PRIORITY_LEVEL_5,
    RES_PRIORITY_LEVEL_6,
    RES_PRIORITY_LEVEL_7,
    RES_PRIORITY_LEVEL_8,
    RES_PRIORITY_LEVEL_9,

    __RES_PRIORITY_NUM
}; 

const size_t MAX_URI_LENGTH = 4096;

enum FETCH_FAIL_GROUP
{
    FETCH_FAIL_GROUP_OK = 0x0,
    FETCH_FAIL_GROUP_HTTP = 0x1,
    FETCH_FAIL_GROUP_SSL = 0x2,
    FETCH_FAIL_GROUP_SERVER = 0x3,
    FETCH_FAIL_GROUP_DNS = 0x4,
    FETCH_FAIL_GROUP_RULE = 0x5,
    FETCH_FAIL_GROUP_CANCELED = 0x6,
    FETCH_FAIL_GROUP_UNFINISHED = 0x7,
    __FETCH_FAIL_GROUP_NUM
};

enum FETCH_ERROR
{
    RS_OK = 0,
    RS_NOSSLCTX = 1,
    RS_FILETYPE,  
    RS_DOMAIN_FILTERED,
    RS_URL_PREFIX_FILTERED,

    RS_DUPLICATION,                               // 5
    RS_INVALURI,
    RS_NOHOSTINFO,
    RS_TOOMANYREDIR,
    RS_NOREDIR,                                   // useless

    RS_URLTOOLONG,                                // 10
    RS_ERRORREDIR,
    RS_INVALIDPAGE,
    RS_URLTYPE_DISALLOWED,
    RS_SUBDOMAIN_EXCEED,

    RS_INVALID_PAGESIZE,                          // 15
    RS_ROBOTS_TXT,
    RS_URL_REGEX_FILTERED,
    RS_IP_FILTERED,
    RS_DEPTH_EXCEED,

    RS_TOO_MANY_URL,				//20
    RS_UNFETCH_SUCCESS_RESULT, //special
    RS_INSIGNIFICANT_PAGE,
    RS_TRANSFORM_FILTERED,
    RS_SIZE_EXCEED,

    RS_URL_REGEX_FILTERED_DELETE_REDIRECT,	//25
    RS_URL_DELIVER_REQUEST_FAILED,
    RS_INVALID_HEADER,
    RS_CANCELED_RCV_TASK,
    RS_DNS_SUBMIT_FAIL,

    RS_UNKNOW_SCHDULE_UNIT,			//30
    RS_CANCELED_ACTIVE,
    RS_PADDING_TIMEOUT,
    RS_INVALID_TASK,
    RS_NOMEMORY,

    RS_PAGESIZE_NOCHANGE,			//35

    __RS_NUM
};

enum REDIRECT_TYPE
{
  REDIRECT_TYPE_UNKNOWN,
  REDIRECT_TYPE_HTTP_301,
  REDIRECT_TYPE_HTTP_302,
  REDIRECT_TYPE_HTTP_303,
  REDIRECT_TYPE_HTTP_307,
  REDIRECT_TYPE_META_REFRESH,
  REDIRECT_TYPE_SCRIPT,
};

inline const char* GetRedirectTypeName(REDIRECT_TYPE type)
{
  switch (type)
  {
    case REDIRECT_TYPE_UNKNOWN:
      return "UNKNOWN";
    case REDIRECT_TYPE_HTTP_301:
      return "HTTP_301";
    case REDIRECT_TYPE_HTTP_302:
      return "HTTP_302";
    case REDIRECT_TYPE_HTTP_303:
      return "HTTP_303";
    case REDIRECT_TYPE_HTTP_307:
      return "HTTP_307";
    case REDIRECT_TYPE_META_REFRESH:
      return "META_FRESH";
    case REDIRECT_TYPE_SCRIPT:
      return "SCRIPT";
  }
  return "INVALID";
}

enum ConcurencyMode
{
    //该serv抓取时，不允许并发，一个抓完才抓下一个
    NO_CONCURENCY,
    //如果该serv有多个ip时，允许对不同ip间并发抓取
    CONCURENCY_PER_SERV, 
    //对并发没有限制
    CONCURENCY_NO_LIMIT
};

struct BatchConfig
{
    static const time_t   DEFAULT_TIMEOUT_SEC        = 3600;
    static const unsigned DEFAULT_MAX_RETRY_TIMES    = 128;
    static const unsigned DEFAULT_MAX_REDIRECT_TIMES = 4;
    static const unsigned DEFAULT_MAX_BODY_SIZE      = UINT_MAX;
    static const unsigned DEFAULT_TRUNCATE_SIZE      = UINT_MAX;
    static const ResourcePriority DEFAULT_RES_PRIOR  = RES_PRIORITY_LEVEL_5;
    static const char* DEFAULT_USER_AGENT;
    static const char* DEFAULT_BATCH_ID;
    static const char* DEFAULT_ACCEPT_LANGUAGE;
    static const char* DEFAULT_ACCEPT_ENCODING;
    static const char* DEFAULT_ACCEPT;
    static const char* DEFAULT_HTTP_VERSION;

    time_t   timeout_sec_; 
    unsigned max_retry_times_;
    unsigned max_redirect_times_;
    unsigned max_body_size_;
    unsigned truncate_size_;
    ResourcePriority prior_;
    char user_agent_[512];
    char accept_encoding_[512];
    char accept_language_[512];
    char accept_[512];
    char http_version_[10];

    BatchConfig()
    {
        memset(this, 0, sizeof(BatchConfig));
        timeout_sec_     = DEFAULT_TIMEOUT_SEC;
        max_retry_times_ = DEFAULT_MAX_RETRY_TIMES;
        max_redirect_times_ = DEFAULT_MAX_REDIRECT_TIMES;
        max_body_size_ = DEFAULT_MAX_BODY_SIZE;
        truncate_size_ = DEFAULT_TRUNCATE_SIZE;
        prior_ = DEFAULT_RES_PRIOR;
        strncpy(user_agent_, DEFAULT_USER_AGENT, 512);
        strncpy(accept_encoding_, DEFAULT_ACCEPT_ENCODING, 512); 
        strncpy(accept_language_, DEFAULT_ACCEPT_LANGUAGE, 512); 
        strncpy(accept_, DEFAULT_ACCEPT, 512);
        strncpy(http_version_, DEFAULT_HTTP_VERSION, 10);
    }

    BatchConfig(const BatchConfig& other)
    {
        memcpy(this, &other, sizeof(other));
    }
};

class FetchErrorType
{
public:
	FetchErrorType(FETCH_FAIL_GROUP group, int error): error_group_(group), error_num_(error)
    {
    }
    FETCH_FAIL_GROUP group() const 
    {
        return error_group_;
    }
    int error_num() const 
    {
        return error_num_;
    }
    void set(FETCH_FAIL_GROUP group, int error_num)
    {
        error_group_=group; 
        error_num_ = error_num;
    }

private:
    FETCH_FAIL_GROUP error_group_;
    int error_num_;
};

const char* GetFetchErrorGroupName(int gid);
const char* GetSpiderError(int code);
const char* GetSpiderError(FETCH_ERROR code);
std::string GetSpiderError(FetchErrorType error);

inline time_t current_time_ms()
{
    timeval tv; 
    gettimeofday(&tv, NULL);
    return (tv.tv_sec*1000000 + tv.tv_usec) / 1000;
}
#endif//SPIDER_BASE_HPP_INCLUDED
