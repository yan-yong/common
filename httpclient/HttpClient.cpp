#include <HttpClient.hpp>
#include <boost/shared_ptr.hpp>
#include "lock/lock.hpp"
#include "TRedirectChecker.hpp"
#include "ChannelManager.hpp"
#include "SchedulerTypes.hpp"
#include "utility/net_utility.h"

typedef Storage::HostKey HostKey;

struct FetchRequest
{
    URI uri_;
    void* contex_;
    MessageHeaders* user_headers_;
    const std::vector<char>* content_;
    ResourcePriority prior_;
    BatchConfig * batch_cfg_;
    struct addrinfo * proxy_ai_;
    Resource*         root_res_;
    time_t            fetch_time_;

    FetchRequest(
            const URI& uri,
            void*  contex,
            MessageHeaders* user_headers,
            const std::vector<char>* content,
            ResourcePriority prior,
            BatchConfig* batch_cfg,
            struct addrinfo * proxy_ai):
        uri_(uri), contex_(contex), 
        user_headers_(user_headers),
        content_(content), prior_(prior), 
        batch_cfg_(batch_cfg), proxy_ai_(proxy_ai)
    {
        root_res_ = NULL;
    }

    FetchRequest(const URI& uri, Resource* root_res)
    {
        uri_ = uri;
        contex_ = NULL;
        user_headers_ = NULL;
        content_ = NULL;
        prior_ = RES_PRIORITY_NOUSE;
        batch_cfg_ = NULL;
        proxy_ai_ = NULL; 
        root_res_ = root_res;    
    }
};

static FETCH_FAIL_GROUP __srv_error_group(int error) 
{
    assert(error);
    return error >= 192 ? FETCH_FAIL_GROUP_SSL:FETCH_FAIL_GROUP_SERVER;
}

HttpClient::HttpClient(
    size_t max_req_size, size_t max_result_size, const char* eth_name, 
    boost::shared_ptr<DNSResolver> dns_resolver):
    request_queue_(max_req_size*2), 
    result_queue_(max_result_size), 
    max_req_size_(max_req_size),
    max_result_size_(max_result_size),
    cur_req_size_(0),
    cur_time_(0), stopped_(false), local_addr_(NULL),
    serv_concurency_mode_(ServChannel::DEFAULT_CONCURENCY_MODE),
    serv_max_err_rate_(ServChannel::DEFAULT_MAX_ERR_RATE),
    serv_err_delay_sec_(ServChannel::DEFAULT_ERR_DELAY_SEC),
    serv_max_err_count_(ServChannel::DEFAULT_MAX_ERR_NUM),
    dns_update_time_(DEFAULT_DNS_UPDATE_TIME),
    dns_error_time_(HostChannel::DEFAULT_DNS_ERROR_TIME) 
{
    fetcher_.reset(new ThreadingFetcher(this));
    if(!dns_resolver)
        dns_resolver_.reset(new DNSResolver());
    else
        dns_resolver_ = dns_resolver;
    pthread_create(&tid_, NULL, RunThread, this);
    channel_manager_ = ChannelManager::Instance();
    if(eth_name)
    {
        local_addr_ = (struct sockaddr*)malloc(sizeof(struct sockaddr));
        memset(local_addr_, 0, sizeof(struct sockaddr)); 
        struct in_addr * p_addr = &((struct sockaddr_in*)local_addr_)->sin_addr;
        assert(getifaddr(AF_INET, 0, eth_name, p_addr) == 0);
    }
    memset(&fetcher_params_, 0, sizeof(fetcher_params_));
    BatchConfig batch_cfg;
    default_batch_cfg_ = Storage::Instance()->AcquireBatchCfg(BatchConfig::DEFAULT_BATCH_ID, batch_cfg);
}

BatchConfig* HttpClient::AcquireBatchCfg(const std::string& batch_id, const BatchConfig& batch_cfg)
{
    return Storage::Instance()->AcquireBatchCfg(batch_id, batch_cfg);
}

void HttpClient::Open()
{
    dns_resolver_->Open();
    fetcher_->Begin(fetcher_params_);
}

void HttpClient::SetServConfig(
    ConcurencyMode mode, 
    double max_err_rate, unsigned err_delay_sec, 
    unsigned serv_max_err_count)
{
    serv_concurency_mode_ = mode;
    serv_max_err_rate_    = max_err_rate;
    serv_err_delay_sec_   = err_delay_sec;
    serv_max_err_count_   = serv_max_err_count;
}

void HttpClient::SetDnsCacheTime(time_t dns_update_time, time_t dns_error_time)
{
    dns_update_time_ = dns_update_time;
    dns_error_time_  = dns_update_time;
}

void HttpClient::SetDefaultBatchConfig(const BatchConfig& batch_cfg)
{
    std::string default_batch_id = BatchConfig::DEFAULT_BATCH_ID;
    Storage::Instance()->UpdateBatchConfig(default_batch_id, batch_cfg);
}

void HttpClient::UpdateBatchConfig(std::string batch_id, const BatchConfig& batch_cfg)
{
    Storage::Instance()->UpdateBatchConfig(batch_id, batch_cfg);
}

void HttpClient::SetFetcherParams(Fetcher::Params params)
{
    memcpy(&fetcher_params_, &params, sizeof(params));
    fetcher_->UpdateParams(fetcher_params_);
}

void* HttpClient::RunThread(void *contex) 
{
    HttpClient* http_client = (HttpClient*)contex;
    while(!http_client->stopped_)
        http_client->Pool();
    return NULL;
}

void HttpClient::Close()
{
    if(!__sync_bool_compare_and_swap(&stopped_, false, true))
        return;
    request_queue_.exit();
    result_queue_.exit();
    fetcher_->End();
    dns_resolver_->Close(); 
    pthread_join(tid_, NULL);
}

void HttpClient::UpdateBatchConfig(std::string& batch_id, 
    const BatchConfig& cfg)
{
    Storage::Instance()->UpdateBatchConfig(batch_id, cfg);
}

void HttpClient::PutResult(FetchErrorType error, 
    HttpFetcherResponse *message, void* contex)
{
    boost::shared_ptr<FetchResult> result(
        new FetchResult(error, message, contex));
    if(result_cb_)
        result_cb_(result);
    else
        result_queue_.enqueue(result);
}

void HttpClient::ProcessSuccResult(Resource* res, HttpFetcherResponse* message)
{
    __sync_fetch_and_sub(&cur_req_size_, 1);
    FetchErrorType fetch_ok(FETCH_FAIL_GROUP_OK, RS_OK);
    LOG_INFO("%s, SUCCESS\n", res->GetUrl().c_str());
    if(res->serv_)
        res->serv_->AddSucc();
    PutResult(fetch_ok, message, res->contex_);
    Storage::Instance()->DestroyResource(res);
}

void HttpClient::ProcessFailResult(FetchErrorType fetch_error, 
    Resource* res, HttpFetcherResponse* message)
{
    __sync_fetch_and_sub(&cur_req_size_, 1);
    LOG_ERROR("%s, FAILED, %s\n", res->GetUrl().c_str(), 
        GetSpiderError(fetch_error).c_str());
    PutResult(fetch_error, message, res->contex_);
    Storage::Instance()->DestroyResource(res);
    // 检查是否Server错误
    if(res->serv_ && fetch_error.group() == FETCH_FAIL_GROUP_SERVER)
    {
        res->serv_->AddFail();
        if(res->serv_->IsServErr())
        {
            LOG_ERROR("%s: server ERROR\n", channel_manager_->ToString(res->serv_).c_str());
            ResourceListPtr res_lst = channel_manager_->RemoveUnfinishRes(res->host_);
            while(!res_lst->empty())
            {
                Resource * res = res_lst->get_front();
                res_lst->pop_front();
                timed_lst_map_.del(*res);
                PutResult(fetch_error, NULL, res->contex_);
                Storage::Instance()->DestroyResource(res);
            }
            // 删除这个serv
            channel_manager_->DestroyChannel(res->serv_);
        }
    }
}

void HttpClient::PutDnsResult(DnsResultType dns_result)
{
    std::string addr_str;
    uint16_t port = 0;
    dns_result->GetAddr(addr_str, port);
    //LOG_DEBUG("Put dns result: %s\n", addr_str.c_str());
    dns_queue_.enqueue(dns_result);
}

void HttpClient::HandleDnsResult(DnsResultType dns_result)
{
    std::string err_msg  = dns_result->err_msg_;
    struct addrinfo * ai = dns_result->ai_;
    HostKey*  host_key   = (HostKey*)dns_result->contex_;
    HostChannel *host_channel = Storage::Instance()->GetHostChannel(*host_key);
    delete host_key;
    if(!host_channel)
        return;
    if(err_msg.empty())
    {
        char ai_str[1024];
        get_ai_string(ai, ai_str, 1024);
        LOG_INFO("%s, DNS resolve success: %p %s.\n", 
            host_channel->host_.c_str(), ai, ai_str);
        ServChannel* serv_channel  = Storage::Instance()->AcquireServChannel(
            host_channel->scheme_, ai, 
            serv_concurency_mode_, serv_max_err_rate_,
            serv_max_err_count_,   serv_err_delay_sec_,
            local_addr_ );
        channel_manager_->SetServChannel(host_channel, serv_channel);
        return;
    }
    //dns resolve error
    LOG_ERROR("%s, DNS resolve error, %s\n", host_channel->host_.c_str(),
        err_msg.c_str());
    ResourceListPtr res_lst = channel_manager_->RemoveUnfinishRes(host_channel);
    while(!res_lst->empty())
    {
        Resource * res = res_lst->get_front();
        res_lst->pop_front();
        timed_lst_map_.del(*res);
        FetchErrorType fetch_err(FETCH_FAIL_GROUP_DNS, RS_DNS_SUBMIT_FAIL);
        ProcessFailResult(fetch_err, res, NULL);
    }
    channel_manager_->SetServChannel(host_channel, NULL);
}

void HttpClient::HandleHttpResponse3xx(Resource* res, HttpFetcherResponse *resp)
{
    // error 304
    if(resp->StatusCode == 304)
    {
        FetchErrorType fetch_error(FETCH_FAIL_GROUP_HTTP,304);
        return ProcessFailResult(fetch_error, res, resp);
    }

    RedirectInfo ri;
    if(getRedirectUrl(resp->Headers, ri.to_url))
    {
        ri.type = __get_redirect_type(resp->StatusCode);
        HandleRedirectResult(res, resp, ri);
        return;
    }
    FetchErrorType fetch_error(FETCH_FAIL_GROUP_RULE, 
        RS_ERRORREDIR);
    return ProcessFailResult(fetch_error, res, resp);
}

void HttpClient::HandleHttpResponse2xx(Resource* res, HttpFetcherResponse *resp)
{
    char error_msg[100];
    if(resp->ContentEncoding(error_msg) != 0)
        LOG_ERROR("%s, ContentEncoding error, %s", res->RootUrl().c_str(), error_msg);
    if (resp->SizeExceeded())
    {
        FetchErrorType fetch_error(FETCH_FAIL_GROUP_RULE, RS_INVALID_PAGESIZE);
        ProcessFailResult(fetch_error, res, resp);
        return;
    }
    RedirectInfo ri;
    // Check Location headers
    int idx = 0;
    if(resp->StatusCode == 206 && (idx = resp->Headers.Find("Location")) >= 0)
    {
        // When the send an range request while the server sill 
        // redirect orig url, it reponse an 206 StatusCode not 3xx,
        // so we check redirect here
        ri.to_url = resp->Headers[idx].Value;
        ri.type = REDIRECT_TYPE_HTTP_302;
        HandleRedirectResult(res, resp, ri);
        return;
    }
    // Meta redirect check
    if(TRedirectChecker::Instance()->checkMetaRedirect(
        res->GetUrl(), *resp, ri.to_url))
    {
        ri.type = REDIRECT_TYPE_META_REFRESH;
        HandleRedirectResult(res, resp, ri);
        return;
    }

#if 0
    // Script redirect check
    if(doScriptRedirect(resp->Body.size())
        && TRedirectChecker::instance()->checkScriptRedirect(
        res->GetUrl(), *resp, ri.to_url))
    {
        ri.type = REDIRECT_TYPE_SCRIPT;
        HandleRedirectResult(res, resp, ri);
        return;
    }
#endif

    ProcessSuccResult(res, resp);
}

void HttpClient::HandleRedirectResult( Resource* res, 
    HttpFetcherResponse *resp, RedirectInfo ri)
{
    if(res->ReachMaxRedirectNum())
    {
        FetchErrorType error_type(FETCH_FAIL_GROUP_RULE, RS_ERRORREDIR);
        ProcessFailResult(error_type, res, NULL);
        return; 
    }
    URI uri;
    if(!UriParse(ri.to_url.c_str(), ri.to_url.length(), uri) 
        || !HttpUriNormalize(uri))
    {
        LOG_ERROR("%s, invalid uri\n", ri.to_url.c_str());
        return;
    }
    Resource*  root_res = res->RootResource();
    request_queue_.enqueue(new FetchRequest(uri, root_res)); 
}

REDIRECT_TYPE HttpClient::__get_redirect_type(int status_code)
{
    REDIRECT_TYPE type = REDIRECT_TYPE_UNKNOWN;
    switch (status_code)
    {    
        case 301: type = REDIRECT_TYPE_HTTP_301; break;
        case 302: type = REDIRECT_TYPE_HTTP_302; break;
        case 303: type = REDIRECT_TYPE_HTTP_303; break;
        case 307: type = REDIRECT_TYPE_HTTP_307; break;
    }    
    return type;
}

void HttpClient::HandleRequest(RequestPtr request)
{
    char scheme = str2protocal(request->uri_.Scheme());
    ServChannel * proxy_serv = NULL;
    Resource* res = NULL;
    //重定向的request
    if(request->root_res_)
    {
        // 如果root resource使用了代理，则使用该代理
        if(request->root_res_->proxy_state_ != Resource::NO_PROXY)
            proxy_serv = request->root_res_->serv_;
        res = Storage::Instance()->CreateResource(
            request->uri_,  request->root_res_->contex_, 
            request->root_res_->cfg_, request->prior_,
            request->root_res_->GetUserHeaders(),
            request->root_res_->GetPostContent(), 
            request->root_res_, proxy_serv);
    }
    //非重定向request
    else 
    {
        if(request->proxy_ai_)
        {
            proxy_serv = Storage::Instance()->AcquireServChannel(
                scheme, request->proxy_ai_, serv_concurency_mode_, 
                serv_max_err_rate_,   serv_max_err_count_,   
                serv_err_delay_sec_,  local_addr_);
        }
        res = Storage::Instance()->CreateResource(
            request->uri_, request->contex_, request->batch_cfg_, 
            request->prior_, request->user_headers_, 
            request->content_, NULL, proxy_serv);
    }

    __sync_fetch_and_add(&cur_req_size_, 1);
    HostChannel * host_channel = res->host_;
    // 不使用代理时，去检查解dns
    if(proxy_serv == NULL)
    {
        if(channel_manager_->CheckResolveDns(host_channel, 
            dns_update_time_, dns_error_time_))
        {
            HostKey* host_key = new HostKey(host_channel->host_key_);
            DNSResolver::ResolverCallback dns_resolver_cb = 
                boost::bind(&HttpClient::PutDnsResult, this, _1);
            dns_resolver_->Resolve(host_channel->host_, host_channel->port_, 
                dns_resolver_cb, host_key);
            LOG_INFO("%s, request DNS\n", host_channel->host_.c_str()); 
        }
        else if(host_channel->host_error_)
        {
            FetchErrorType host_err(FETCH_FAIL_GROUP_DNS, RS_DNS_SUBMIT_FAIL);
            ProcessFailResult(host_err, res, NULL);
            return;
        }
    }

    //加入到超时队列中, 0表示不超时
    time_t timeout_stamp = res->GetTimeoutStamp();
    if(timeout_stamp > 0)
        timed_lst_map_.add_back(timeout_stamp, *res);
}

bool HttpClient::PutRequest(
    const  std::string& url,
    void*  contex,
    MessageHeaders* user_headers,
    const std::vector<char>* content,
    BatchConfig * batch_cfg,
    struct addrinfo * proxy_ai,
    ResourcePriority prior)
{
    if(cur_req_size_ > max_req_size_)
    {
        LOG_ERROR("%s, exceed max request size: %zd\n", 
            url.c_str(), max_req_size_);
        return false;
    }
    URI uri;
    if(!UriParse(url.c_str(), url.length(), uri) 
        || !HttpUriNormalize(uri))
    {
        LOG_ERROR("%s, invalid uri\n", url.c_str());
        return false;
    }
    if(!batch_cfg)
        batch_cfg = default_batch_cfg_;
    //如果不指定Resource优先级，则使用批次的优先级
    if(prior == RES_PRIORITY_NOUSE)
        prior = batch_cfg->prior_;
    request_queue_.enqueue(new FetchRequest(uri, contex, 
        user_headers, content, prior, batch_cfg, proxy_ai));
    return true;
}

void HttpClient::__update_curent_time()
{
    timeval tv; 
    gettimeofday(&tv, NULL);
    cur_time_ = (tv.tv_sec*1000000 + tv.tv_usec) / 1000;
}

void HttpClient::__fetch_resource(Resource* p_res)
{
    assert(p_res);
    //进入内核后不再控制超时，从超时队列中删除
    timed_lst_map_.del(*p_res);
    p_res->fetch_time_ = current_time_ms();
    p_res->cur_retry_times_++;
    RawFetcherRequest request;
    request.conn = p_res->conn_;
    request.context = p_res;
    fetcher_->PutRequest(request);
}

time_t HttpClient::__handle_timeout_list()
{
    __update_curent_time();
    while(!timed_lst_map_.empty())
    {
        time_t timeout_stamp = 0;
        Resource* p_res = NULL;
        timed_lst_map_.get_front(timeout_stamp, p_res);
        if(timeout_stamp > cur_time_/1000)
            return timeout_stamp - cur_time_/1000;
        timed_lst_map_.pop_front();
        FetchErrorType fetch_error(FETCH_FAIL_GROUP_CANCELED, RS_PADDING_TIMEOUT); 
        ProcessFailResult(fetch_error, p_res, NULL);
    }
    return 0;
}

IFetchMessage* HttpClient::CreateFetchResponse(const FetchAddress& address, void * request_context)
{
    Resource * p_res = (Resource*) request_context;
    size_t max_body_size = p_res->cfg_->max_body_size_;
    size_t truncate_size = p_res->cfg_->truncate_size_;
    if(p_res->proxy_state_ == Resource::PROXY_CONNECT)
    {
        max_body_size = 0;
        truncate_size = 0;
    } 
    HttpFetcherResponse* resp = new HttpFetcherResponse(address.remote_addr, 
        address.remote_addrlen, address.local_addr,
        address.local_addrlen, max_body_size, truncate_size);
    return resp;
}

void HttpClient::FreeFetchMessage(IFetchMessage * message)
{
    delete message;
}

struct RequestData* HttpClient::CreateRequestData(void * request_context)
{
    Resource* res    = (Resource*)request_context;
    BatchConfig* cfg = res->cfg_;
    HttpFetcherRequest* req = new HttpFetcherRequest();
    req->Clear();
    req->Version= res->GetHttpVersion();
    req->Method = res->GetHttpMethod();
    //GET or POST
    if(req->Method == "GET" || req->Method == "POST")
    {
        req->Uri    = res->GetUrl();
        req->Headers.Add("Host", res->GetHostWithPort());
        req->Headers.Add("Accept", cfg->accept_);
        req->Headers.Add("Accept-Language", cfg->accept_language_);
        req->Headers.Add("Accept-Encoding", cfg->accept_encoding_);
        if(strlen(res->cfg_->user_agent_))
            req->Headers.Add("User-Agent", cfg->user_agent_);
        // add post content
        if(res->GetPostContent())
        {
            const std::vector<char>* post_content = res->GetPostContent();
            size_t content_len = post_content->size();
            char content_len_str[16];
            snprintf(content_len_str, 16, "%zd", content_len);
            req->Headers.Add("Content-Type", "application/x-www-form-urlencoded");
            req->Headers.Add("Content-Length", content_len_str);
            req->Body = *post_content;
        }
        // add user header
        const MessageHeaders* user_headers = res->GetUserHeaders();
        for(unsigned i = 0; user_headers && i < user_headers->Size(); i++)
            req->Headers.Set((*user_headers)[i].Name, (*user_headers)[i].Value);
    }
    else
    {
        req->Uri = res->GetHostWithPort(true);
        req->Headers.Add("Host", res->GetHostWithPort(true));
    }

    req->Close();
    char conn_addr_str[200];
    fetcher_->ConnectionToString(res->conn_, conn_addr_str, 200);
    LOG_INFO("%s, FETCH request %s.\n", req->Uri.c_str(), conn_addr_str);
    req->Dump();
    return req; 
}

void HttpClient::FreeRequestData(struct RequestData * request_data)
{
    delete (HttpFetcherRequest*)request_data;
}

void HttpClient::ProcessResult(RawFetcherResult& fetch_result)
{
    Resource * res = (Resource*)fetch_result.context;
    HttpFetcherResponse *resp = (HttpFetcherResponse *)fetch_result.message;
    assert(res);
    // handle proxy result
    if(res->proxy_state_ != Resource::NO_PROXY && 
        !HandleProxyResult(fetch_result))
    {
        return;
    }

    fetcher_->CloseConnection(res->conn_);
    channel_manager_->ReleaseConnection(res);
    ServChannel * serv = res->serv_;
    int err_num = fetch_result.err_num;
    time_t resp_time = 0;
    if(res->arrive_time_ < cur_time_)
        resp_time = cur_time_ - res->arrive_time_;
    serv->AddRespTime(resp_time);

    if(err_num)
    {
        FetchErrorType fetch_error(__srv_error_group(err_num), err_num);
        ProcessFailResult(fetch_error, res, resp);
        return;
    }
    assert(resp);
    if (resp->SizeExceeded())
    {
        FetchErrorType fetch_error(FETCH_FAIL_GROUP_RULE, RS_INVALID_PAGESIZE);
        ProcessFailResult(fetch_error, res, resp);
        return;
    }
    if(2 == resp->StatusCode / 100)
        HandleHttpResponse2xx(res, resp);
    else if(3 == resp->StatusCode / 100) 
        HandleHttpResponse3xx(res, resp);
    else
    {
        FetchErrorType fetch_error(FETCH_FAIL_GROUP_HTTP, resp->StatusCode);
        ProcessFailResult(fetch_error, res, resp);
    }
}

// https隧道, 先用http connect
// return false: no continious; else, continious next process
bool HttpClient::HandleProxyResult(RawFetcherResult& fetch_result)
{    
    Resource * res = (Resource*)fetch_result.context;
    HttpFetcherResponse *resp = (HttpFetcherResponse *)fetch_result.message;
    int err_num = fetch_result.err_num;

    switch(res->proxy_state_)
    {
        //对于https proxy请求，只是重置成PROXY_CONNECT，方便下次连接
        case Resource::PROXY_HTTPS:
        {
            res->proxy_state_ = Resource::PROXY_CONNECT;
            break;
        }
        case Resource::PROXY_CONNECT:
        {
            //走正常流程
            if(err_num)
                break;
            if(resp->StatusCode == 200)
            {
                res->proxy_state_ = Resource::PROXY_HTTPS; 
                fetcher_->SetConnectionScheme(res->conn_, PROTOCOL_HTTPS);
                __fetch_resource(res);
                return false;
            }
            break;
        }
        default:
        {
            break;
        }
    }
    return true;
}

void HttpClient::Pool()
{
    struct timeval timeout; 
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    __update_curent_time();
    //handle fetch result
    RawFetcherResult fetch_result;
    while (fetcher_->GetResult(&fetch_result, &timeout) == 0) 
        ProcessResult(fetch_result);

    //handle request
    RequestPtr request;
    while(request_queue_.try_dequeue(request))
    {
        HandleRequest(request);
        delete request;
    }

    //handle dns result
    DnsResultType dns_result;
    while(dns_queue_.try_dequeue(dns_result))
        HandleDnsResult(dns_result);
    unsigned quota = fetcher_->AvailableQuota();
    std::vector<Resource*> res_vec = channel_manager_->PopAvailableResources(quota);
    for(unsigned i = 0; i < res_vec.size(); i++)
        __fetch_resource(res_vec[i]);
    __handle_timeout_list();
}

bool HttpClient::GetResult(ResultPtr& result)
{
    return result_queue_.dequeue(result);
}

void HttpClient::SetResultCallback(ResultCallback call_cb)
{
    result_cb_ = call_cb;
}
