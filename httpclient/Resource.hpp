#ifndef __RESOURCE_HPP
#define __RESOURCE_HPP

#include <time.h>
#include <stddef.h>
#include <sys/param.h>
#include <string>
#include <vector>
#include <boost/function.hpp>
#include "httpparser/URI.hpp"
#include "httpparser/HttpMessage.hpp"
#include "linklist/linked_list.hpp"
#include "linklist/linked_list_map.hpp"
#include "fetcher/Fetcher.hpp"
#include "SchedulerTypes.hpp"

class ServChannel;
class HostChannel;

struct Resource 
{
private:
    ~Resource();

public:
    enum ProxyState
    {
        NO_PROXY,
        PROXY_HTTP, 
        PROXY_CONNECT,
        PROXY_HTTPS
    };

public:
    void Initialize(HostChannel* host_channel,
        const std::string& suffix, ResourcePriority prior, 
        void* contex, const MessageHeaders * user_headers,
        const std::vector<char>* post_content, 
        Resource* parent_res, BatchConfig *cfg);
    std::string GetHostWithPort(bool with_port = false) const;
    void SetProxyServ(ServChannel* serv_channel);
    void Destroy();
    std::string GetUrl() const;
    URI GetURI() const;
    int GetScheme() const;
    uint16_t GetPort() const;
    bool ExceedMaxRetryNum() const;
    time_t GetTimeoutStamp() const;
    bool ReachMaxRedirectNum() const;
    Resource* RootResource();
    std::string RootUrl();
    unsigned RedirectCount() const;
    const MessageHeaders* GetUserHeaders() const;
    const std::vector<char>* GetPostContent() const;
    std::string GetHttpMethod() const;
    std::string GetHttpVersion() const;

public:
    //是否有自定义头
    char has_user_headers_:     1;
    //是否是重定向Resource
    char is_redirect_:          1;
    char has_post_content_:     1;
    //引用当前Resource的重定向Resource数目
    char root_ref_:             5;
    unsigned cur_retry_times_;
    ProxyState proxy_state_;
    ResourcePriority   prior_;
    linked_list_node_t queue_node_;
    linked_list_node_t timed_lst_node_;
    HostChannel *      host_;
    //resource所属的serv
    ServChannel *      serv_;
    Connection  *      conn_;
    char*              suffix_;
    time_t             arrive_time_;
    time_t             fetch_time_;
    void*              contex_;
    BatchConfig *      cfg_;
    void*              extend_[0]; 
};

typedef linked_list_map<time_t, Resource, &Resource::timed_lst_node_> ResTimedMap;

struct ResExtend
{
    const MessageHeaders*    user_headers_;
    const std::vector<char>* post_content_;
    Resource*       root_res_;
    unsigned        cur_redirect_times_;
};

#endif
