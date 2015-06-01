#include <openssl/md5.h>
#include <assert.h>
#include <boost/function.hpp>
#include "SchedulerTypes.hpp"
#include "utility/net_utility.h"
#include "log/log.h"
#include "Channel.hpp"
#include "ChannelManager.hpp"
#include <errno.h>

const char* BatchConfig::DEFAULT_USER_AGENT = "Mozilla/5.0 (Windows NT 6.1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/31.0.1650.63 Safari/537.36 SE 2.X MetaSr 1.0";
const char* BatchConfig::DEFAULT_BATCH_ID   = "default";
const char* BatchConfig::DEFAULT_ACCEPT_LANGUAGE = "zh-cn"; 
const char* BatchConfig::DEFAULT_ACCEPT_ENCODING = "gzip";
const char* BatchConfig::DEFAULT_ACCEPT = "*/*";
const char* BatchConfig::DEFAULT_HTTP_VERSION = "HTTP/1.1";

const char* GetFetchErrorGroupName(int gid)
{
	switch (gid)
	{
	case FETCH_FAIL_GROUP_OK:
		return "OK";
	case FETCH_FAIL_GROUP_HTTP:
		return "HTTP";
	case FETCH_FAIL_GROUP_SSL:
		return "SSL";
	case FETCH_FAIL_GROUP_SERVER:
		return "SERVER";
	case FETCH_FAIL_GROUP_DNS:
		return "DNS";
	case FETCH_FAIL_GROUP_RULE:
		return "RULE";
	case FETCH_FAIL_GROUP_CANCELED:
		return "CANCEL";
	default:
		assert(!"No default");
	}
    return "";
}

const char* GetSpiderError(FETCH_ERROR code) 
{
    return GetSpiderError(int(code));
}

const char* GetSpiderError(int code)
{
    switch (code)
    {
	case RS_NOSSLCTX:
		return "no ssl context";
	case RS_FILETYPE:		
		return "file type filtered";
	case RS_DOMAIN_FILTERED:
		return "domain filtered";
	case RS_URL_PREFIX_FILTERED:
		return "url prefix filtered";
	case RS_DUPLICATION:
		return "url duplication";
	case RS_INVALURI:
		return "invalid uri";
	case RS_NOHOSTINFO:
		return "no hostinfo";
	case RS_TOOMANYREDIR:
		return "too many redirect";
	case RS_NOREDIR:
		return "no redir";
	case RS_URLTOOLONG:
		return "url to long";
	case RS_ERRORREDIR:
		return "error redirect";
	case RS_INVALIDPAGE:
		return "invalid page";
	case RS_URLTYPE_DISALLOWED:
		return "url type disallowed";
	case RS_SUBDOMAIN_EXCEED:
		return "subdomain exceed";
	case RS_INVALID_PAGESIZE:
		return "invalid pagesize";
	case RS_ROBOTS_TXT:
		return "robots.txt";
	case RS_URL_REGEX_FILTERED:
		return "url regex filtered";
	case RS_IP_FILTERED:
		return "IP address filtered";
	case RS_DEPTH_EXCEED:
		return "link depth exceed";
	case RS_TOO_MANY_URL:
		return "too many urls";
	case RS_INSIGNIFICANT_PAGE:
		return "insignificant page";
	case RS_TRANSFORM_FILTERED:
		return "transform filtered";
	case RS_SIZE_EXCEED:
		return "size exceed";
    case RS_URL_REGEX_FILTERED_DELETE_REDIRECT:
        return "url regex filtered deleted";
    case RS_INVALID_HEADER:
        return "Invalid header";
	case RS_CANCELED_RCV_TASK:
		return "Canceled recv task";
	case RS_DNS_SUBMIT_FAIL:
		return "Submit dns request fail";
	case RS_UNKNOW_SCHDULE_UNIT:
		return "Unknow ScheduleUnit";
	case RS_CANCELED_ACTIVE:
		return "Canceled Active";
	case RS_PADDING_TIMEOUT:
	    return "padding timeout";
	case RS_INVALID_TASK:
	    return "invalid task";
	case RS_NOMEMORY:
	    return "out of memory";
	case RS_PAGESIZE_NOCHANGE:
	    return "pagesize no changed";
    }
    return "";
}

std::string GetSpiderError(FetchErrorType err)
{
    std::string err_msg = GetFetchErrorGroupName(err.group());
    err_msg += ": ";
    if(err.group() != FETCH_FAIL_GROUP_SERVER)
        err_msg += GetSpiderError(err.error_num());
    else
        err_msg += strerror(err.error_num());
    return err_msg;
}
