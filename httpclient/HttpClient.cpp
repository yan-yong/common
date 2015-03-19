#include <HttpClient.hpp>
#include <boost/shared_ptr.hpp>
#include "lock/lock.hpp"
#include "TRedirectChecker.hpp"
#include "ChannelManager.hpp"
#include "SchedulerTypes.hpp"
#include "utility/net_utility.h"

typedef Storage::HostKey HostKey;

static FETCH_FAIL_GROUP __srv_error_group(int error) 
{
    assert(error);
    return error >= 192 ? FETCH_FAIL_GROUP_SSL:FETCH_FAIL_GROUP_SERVER;
}

HttpClient::HttpClient(
    size_t max_req_size, size_t max_result_size, 
    const char* eth_name):
    max_req_size_(max_req_size),
    max_result_size_(max_result_size), 
    cur_req_size_(0),
    cur_time_(0), stopped_(false), local_addr_(NULL),
    serv_concurency_mode_(ServChannel::DEFAULT_CONCURENCY_MODE),
    serv_max_err_rate_(ServChannel::DEFAULT_MAX_ERR_RATE),
    serv_err_delay_sec_(ServChannel::DEFAULT_ERR_DELAY_SEC),
    serv_max_err_count_(ServChannel::DEFAULT_MAX_ERR_NUM) 
{
    fetcher_.reset(new ThreadingFetcher(this));
    dns_resolver_.reset(new DNSResolver());
    pthread_create(&tid_, NULL, RunThread, this);
    channel_manager_ = ChannelManager::Instance();
    if(eth_name)
    {
        local_addr_ = (struct sockaddr*)malloc(sizeof(struct sockaddr));
        memset(local_addr_, 0, sizeof(struct sockaddr)); 
        struct in_addr * p_addr = &((struct sockaddr_in*)local_addr_)->sin_addr;
        assert(getifaddr(AF_INET, 0, eth_name, p_addr) == 0);
    }
    dns_resolver_->Open();
}

void HttpClient::SetServConfig(ServChannel::ConcurencyMode mode, 
    double max_err_rate, unsigned err_delay_sec, unsigned serv_max_err)
{
    serv_concurency_mode_ = mode;
    serv_max_err_rate_    = max_err_rate;
    serv_err_delay_sec_   = err_delay_sec;
    serv_max_err_count_   = serv_max_err;
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
    stopped_ = true;
    dns_resolver_->Close(); 
    pthread_join(tid_, NULL);
}

void HttpClient::UpdateBatchConfig(std::string& batch_id, 
    const BatchConfig& cfg)
{
    Storage::Instance()->UpdateBatchConfig(batch_id, cfg);
}

void HttpClient::PutResult(FetchErrorType error, 
    IFetchMessage *message, void* contex)
{
    boost::shared_ptr<FetchResult> result(
        new FetchResult(error, message, contex));
    if(result_cb_)
        result_cb_(result);
    else
        result_queue_.enqueue(result); 
}

void HttpClient::ProcessSuccResult(Resource* res, IFetchMessage *message)
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
    Resource* res, IFetchMessage *message)
{
    __sync_fetch_and_sub(&cur_req_size_, 1);
    LOG_ERROR("%s, ERROR, %s\n", res->GetUrl().c_str(), 
        GetSpiderError(fetch_error).c_str());
    if(res->serv_ && fetch_error.group() == FETCH_FAIL_GROUP_SERVER)
    {
        res->serv_->AddFail();
        if(res->serv_->IsServErr())
        {
            //TODO: add serv error operation
        }
    }
    PutResult(fetch_error, message, res->contex_);
    Storage::Instance()->DestroyResource(res);
}

void HttpClient::PutDnsResult(DnsResultType dns_result)
{
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
        LOG_INFO("%s, DNS resolve success.\n", host_channel->host_.c_str());
        ServChannel* serv_channel  = Storage::Instance()->AcquireServChannel(
            host_channel->scheme_, ai, 
            serv_concurency_mode_, serv_max_err_rate_,
            serv_max_err_count_,   serv_err_delay_sec_,
            local_addr_ 
        );
        channel_manager_->SetServChannel(host_channel, serv_channel);
        return;
    }
    //dns resolve error
    LOG_ERROR("%s, DNS resolve error, %s\n", host_channel->host_.c_str(),
        err_msg.c_str());
    ResourceList res_lst = channel_manager_->RemoveUnfinishRes(host_channel);
    while(!res_lst.empty())
    {
        Resource * res = res_lst.get_front();
        res_lst.pop_front();
        timed_lst_map_.del(*res);
        FetchErrorType fetch_err(FETCH_FAIL_GROUP_DNS, RS_DNS_SUBMIT_FAIL);
        ProcessFailResult(fetch_err, res, NULL);
    }
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
    if(TRedirectChecker::instance()->checkMetaRedirect(
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

void HttpClient::HandleRedirectResult( Resource* res, HttpFetcherResponse *resp, 
    RedirectInfo ri)
{
    if(res->ReachMaxRedirectNum())
    {
        FetchErrorType error_type(FETCH_FAIL_GROUP_RULE, RS_ERRORREDIR);
        ProcessFailResult(error_type, res, NULL);
        return; 
    }
    Resource* root_res = res->RootResource(); 
    Resource* sub_res  = Storage::Instance()->CreateResource(
        ri.to_url, root_res->contex_, root_res->cfg_, 
        root_res->prior_, root_res->GetUserHeaders(), 
        root_res->RootResource());
    __handle_request(sub_res);
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

void HttpClient::__handle_request(Resource* res)
{
    LOG_INFO("%s, RECV request.\n", res->GetUrl().c_str()); 
    __sync_fetch_and_add(&cur_req_size_, 1);
    channel_manager_->AddResource(res);
    //dns不知, 则先去解dns
    if(!res->serv_)
    {
        HostChannel * host_channel = res->host_;
        HostKey* host_key = new HostKey(host_channel->host_key_);
        DNSResolver::ResolverCallback dns_resolver_cb = 
            boost::bind(&HttpClient::PutDnsResult, this, _1);
        dns_resolver_->Resolve(host_channel->host_, host_channel->port_, 
            dns_resolver_cb, host_key);
        LOG_INFO("%s, request DNS\n", host_channel->host_.c_str()); 
        return;
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
    ResourcePriority prior,
    std::string batch_id )
{
    if(cur_req_size_ > max_req_size_)
    {
        LOG_ERROR("%s, exceed max request size: %zd\n", 
            url.c_str(), max_req_size_);
        return false;
    }
    BatchConfig * batch_cfg = Storage::Instance()->AcquireBatchCfg(batch_id);
    Resource* res = Storage::Instance()->CreateResource(url, contex, 
        batch_cfg, prior, user_headers, NULL);
    if(!res)
    {
        LOG_ERROR("%s, invalid uri\n", url.c_str());
        return false; 
    }
    __handle_request(res);
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
    HttpFetcherResponse* resp = new HttpFetcherResponse(address.remote_addr, 
        address.remote_addrlen,
        address.local_addr,
        address.local_addrlen,
        p_res->cfg_->max_body_size_,
        p_res->cfg_->truncate_size_);
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
    req->Uri = res->GetUrl();

    req->Headers.Add("Host", res->GetHostWithPort());
    req->Headers.Add("Accept", cfg->accept_);
    req->Headers.Add("Accept-Language", cfg->accept_language_);
    req->Headers.Add("Accept-Encoding", cfg->accept_encoding_);
    if(strlen(res->cfg_->user_agent_))
        req->Headers.Add("User-Agent", cfg->user_agent_);
    // add user header
    MessageHeaders* user_headers = res->GetUserHeaders();
    for(unsigned i = 0; i < user_headers->Size(); i++)
        req->Headers.Set((*user_headers)[i].Name, (*user_headers)[i].Value);
    req->Close();
    LOG_INFO("%s, FETCH request\n", req->Uri.c_str());
    return req; 
}

void HttpClient::FreeRequestData(struct RequestData * request_data)
{
    delete request_data;
}

void HttpClient::ProcessResult(RawFetcherResult& fetch_result)
{
    Resource * res = (Resource*)fetch_result.context;
    HttpFetcherResponse *resp = (HttpFetcherResponse *)fetch_result.message;
    assert(res);
    fetcher_->CloseConnection(res->conn_);
    //channel_manager_->ReleaseConnection(res);
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

void HttpClient::Pool()
{
    struct timeval timeout; 
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    __update_curent_time();
    //handle  dns result
    DnsResultType dns_result;
    while(dns_queue_.try_dequeue(dns_result))
        HandleDnsResult(dns_result);
    //handle fetch result
    RawFetcherResult fetch_result;
    while (fetcher_->GetResult(&fetch_result, &timeout) == 0) 
        ProcessResult(fetch_result);
    unsigned quota = fetcher_->AvailableQuota();
    std::vector<Resource*> res_vec = channel_manager_->PopAvailableResources(quota);
    for(unsigned i = 0; i < res_vec.size(); i++)
        __fetch_resource(res_vec[i]);
    __handle_timeout_list();
}

void HttpClient::SetResultCallback(ResultCallback call_cb)
{
    result_cb_ = call_cb;
}
