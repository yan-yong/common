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

//user_headers/post_content 在外部开内存, 浅拷贝
void Resource::Initialize(
    HostChannel* host_channel,const std::string& suffix, 
    ResourcePriority prior, void* contex,
    const MessageHeaders * user_headers,
    const char* post_content, 
    Resource* root_res, 
    BatchConfig *cfg)
{
    host_ = host_channel;
    suffix_ = strdup(suffix.c_str());
    prior_ = prior;
    arrive_time_ = current_time_ms();
    contex_ = contex;
    cfg_ = cfg;
    cur_retry_times_ = 0;
    conn_ = NULL;
    queue_node_.prev  = &queue_node_;
    queue_node_.next  = &queue_node_;
    timed_lst_node_.prev = &timed_lst_node_;
    timed_lst_node_.next = &timed_lst_node_;
    ResExtend* pextend = (ResExtend*)extend_;
    serv_ = NULL;
    is_redirect_     = 0;
    root_ref_     = 0;
    has_user_headers_= 0;
    has_post_content_= 0;
    proxy_state_   = NO_PROXY; 

    if(user_headers || root_res || post_content)
        memset(pextend, 0, sizeof(ResExtend));
    if(user_headers)
    {
        has_user_headers_ = 1;
        pextend->user_headers_ = user_headers;
    }
    if(post_content)
    {
        has_post_content_ = 1;
        pextend->post_content_ = post_content;
    }
    if(root_res)
    {
        is_redirect_ = 1;
        root_res->root_ref_++;
        pextend->cur_redirect_times_  = root_res->RedirectCount() + 1;
        pextend->root_res_ = root_res; 
        arrive_time_       = root_res->arrive_time_;
        if(!has_user_headers_ && root_res->has_user_headers_)
        {
            pextend->user_headers_ = root_res->GetUserHeaders();
            has_user_headers_ = 1;
        }
        if(!has_post_content_ && root_res->has_post_content_)
        {
            pextend->post_content_ = root_res->GetPostContent(); 
            has_post_content_ = 1;
        }
    }
} 

void Resource::SetProxyServ(ServChannel* serv_channel)
{
    serv_ = serv_channel;
    if(host_->scheme_ == PROTOCOL_HTTPS)
        proxy_state_ = PROXY_CONNECT;
    else
        proxy_state_ = PROXY_HTTP;
}

std::string Resource::GetHostWithPort(bool with_port) const
{
    char buf[2048];
    size_t sz = snprintf(buf, 2048, "%s", host_->host_.c_str());
    if(with_port || !IsHttpDefaultPort(host_->scheme_, host_->port_))
    {
        snprintf(buf + sz, 2048 - sz, ":%hu", host_->port_);
    }
    return buf;
}

std::string Resource::GetUrl() const
{
    return (ChannelManager::Instance())->ToString(host_) + suffix_;
}
    
std::string Resource::GetHttpMethod() const
{
    if(proxy_state_ == PROXY_CONNECT)
        return "CONNECT";
    return has_post_content_ ? "POST":"GET";
}

std::string Resource::GetHttpVersion() const
{
    return cfg_->http_version_;
}

URI Resource::GetURI() const
{
    URI uri;
    std::string url = this->GetUrl();
    UriParse(url.c_str(), url.length(), uri);
    HttpUriNormalize(uri);
    return uri;
}

void Resource::Destroy()
{
    if(suffix_)
    {
        free(suffix_);
        suffix_ = NULL;
    }
    Resource* root_res = RootResource(); 
    if(is_redirect_)
        root_res->root_ref_ -= 1;
    if(has_user_headers_)
    {
        //delete ((ResExtend*)extend_)->user_headers_;
        ((ResExtend*)extend_)->user_headers_ = NULL;
        ((ResExtend*)extend_)->post_content_ = NULL;
    }
}

const MessageHeaders* Resource::GetUserHeaders() const
{
    if(!has_user_headers_)
        return NULL;
    return ((ResExtend*)extend_)->user_headers_;
}

const char* Resource::GetPostContent() const
{
    if(!has_post_content_)
        return NULL;
    return ((ResExtend*)extend_)->post_content_;
}

int Resource::GetScheme() const 
{
    return host_->scheme_;
}

uint16_t Resource::GetPort() const
{
    return host_->port_;
}

bool Resource::ExceedMaxRetryNum() const
{
    return cur_retry_times_ > cfg_->max_retry_times_; 
}

time_t Resource::GetTimeoutStamp() const
{
    if(!cfg_->timeout_sec_)
        return 0;
    return cfg_->timeout_sec_ + arrive_time_/1000;
}

bool Resource::ReachMaxRedirectNum() const
{
    return RedirectCount() >= cfg_->max_redirect_times_;
}

Resource* Resource::RootResource()
{
    if(!is_redirect_)
        return this; 
    return ((ResExtend*)extend_)->root_res_;
}

std::string Resource::RootUrl()
{
    return RootResource()->GetUrl();
}

unsigned Resource::RedirectCount() const
{
    if(!is_redirect_)
        return 0;
    return ((ResExtend*)extend_)->cur_redirect_times_;
}

/*
void Resource::SetFetchAddr(const char* dst_ip, uint16_t port,  sockaddr* local_addr)
{
    struct sockaddr * dst_addr = get_sockaddr_in(dst_ip, port);
    assert(dst_addr);
    fetch_addr.remote_addr = dst_addr;
    fetch_addr.remote_addrlen = sizeof(struct sockaddr);
    fetch_addr.local_addr = local_addr;
}

void Resource::SetFetchAddr(struct addrinfo* serv_addr, struct sockaddr* local_addr)
{
    assert(valid_);
    fetch_addr.remote_addr = serv_addr->ai_addr;
    fetch_addr.remote_addrlen = sizeof(struct sockaddr);
    fetch_addr.local_addr  = local_addr;
    if(local_addr)
        fetch_addr.local_addrlen = sizeof(struct sockaddr);
    else
        fetch_addr.local_addrlen = 0;
    MD5_CTX ctx; 
    MD5_Init(&ctx);
    MD5_Update(&ctx, fetch_addr.remote_addr, fetch_addr.remote_addrlen);
    MD5_Final((unsigned char *)&serv_key, &ctx);
}
*/
