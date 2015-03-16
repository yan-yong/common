#include <HttpClient.hpp>
#include <boost/shared_ptr.hpp>
#include "lock/lock.hpp"
#include "TRedirectChecker.hpp"
#include "ChannelManager.hpp"
#include "SchedulerTypes.hpp"
#include "utility/net_utility.h"

static FETCH_FAIL_GROUP __srv_error_group(int error) 
{
    assert(error);
    return error >= 192 ? FETCH_FAIL_GROUP_SSL:FETCH_FAIL_GROUP_SERVER;
}

HttpClient::HttpClient(size_t max_conn_size, size_t max_req_size, 
    size_t max_result_size, const char* eth_name):
    max_req_size_(DEFAULT_REQUEST_SIZE),
    cur_req_size_(0),
    max_result_size_(DEFAULT_RESULT_SIZE), 
    cur_time_(0), stopped_(false),
    local_addr_(NULL)
{
    fetcher_.reset(new ThreadingFetcher(this));
    storage_.reset(new Storage());
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
}

void* HttpClient::RunThread(void *contex) 
{
    HttpClient* http_client = (HttpClient*)contex;
    while(!http_client->stopped_)
        http_client->__pool();
    return NULL;
}

void HttpClient::Close()
{
    stopped_ = true;
    pthread_join(tid_, NULL);
}

void HttpClient::UpdateBatchConfig(std::string& batch_id, 
    const BatchConfig& cfg)
{
    storage_->UpdateBatchConfig(batch_id, cfg);
}

void HttpClient::PutResult(FetchErrorType error, IFetchMessage *message, void* contex)
{
    if(result_cb_)
        result_cb_(error, message, contex);
    else
    {
        boost::shared_ptr<FetchResult> result(new FetchResult(error, message, contex));
        result_queue_.enqueue(result); 
    }
}

void HttpClient::ProcessSuccResult(Resource* res, IFetchMessage *message)
{
    FetchErrorType fetch_ok(FETCH_FAIL_GROUP_OK, RS_OK);
    if(res->serv_)
        res->serv_->AddSucc();
    PutResult(fetch_ok, message, res->contex_);
}

void HttpClient::ProcessFailResult(FetchErrorType fetch_error, 
    Resource* res, IFetchMessage *message)
{
    if(res->serv_)
        res->serv_->AddFail();
    //TODO: add serv error operation
    PutResult(fetch_error, message, res->contex_);
}

void HttpClient::HandleDnsResult(std::string err_msg, 
    struct addrinfo* ai, const void* contex)
{
    HostChannel *host_channel = (HostChannel*)contex;
    ServChannel* serv_channel = 
        storage_->AcquireServChannel(ai, host_channel->scheme_, local_addr_);
    channel_manager_->SetServChannel(host_channel, serv_channel);
    if(!serv_channel->queue_node_.empty())
        __fetch_serv(serv_channel); 
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
        LOG_ERROR("%s ContentEncoding error: %s", res->RootUrl().c_str(), error_msg);
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
    Resource* sub_res  = storage_->CreateResource(
        ri.to_url, root_res->contex_, root_res->cfg_, 
        root_res->prior_, root_res->GetUserHeaders(), 
        root_res->RootResource());
    __put_request(sub_res);
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

void HttpClient::__put_request(Resource* res)
{
    //dns不知, 则先取解dns
    if(!res->serv_)
    {
        HostChannel * host_channel = res->host_;
        DNSResolver::ResolverCallback dns_resolver_cb = 
            boost::bind(&HttpClient::HandleDnsResult, this, _1, _2, _3);
        dns_resolver_->Resolve(host_channel->host_, host_channel->port_, 
            dns_resolver_cb, host_channel); 
        return;
    }
     //加入到超时队列中, 0表示不超时
    time_t timeout_stamp = res->GetTimeoutStamp();
    if(timeout_stamp > 0)
        timed_lst_map_.add_back(timeout_stamp, *res);
    if(res->serv_->queue_node_.empty())
        __fetch_serv(res->serv_);
}

bool HttpClient::PutRequest(
    const   std::string& url,
    void*  contex,
    MessageHeaders* user_headers,
    ResourcePriority prior,
    std::string batch_id)
{
    if(cur_req_size_ > max_req_size_)
    {
        LOG_ERROR("[HttpClient] exceed max request size: %zd, %s\n", 
            max_req_size_, url.c_str());
        return false;
    }
    BatchConfig * batch_cfg = storage_->AcquireBatchCfg(batch_id);
    Resource* res = storage_->CreateResource(url, contex, 
        batch_cfg, prior, user_headers, NULL);
    if(!res)
    {
        LOG_ERROR("[HttpClient] invalid uri: %s\n", url.c_str());
        return false; 
    }
    __put_request(res);
    return true;
}

bool HttpClient::PutRequest(Resource* res)
{
    if(cur_req_size_ > max_req_size_)
    {
        LOG_ERROR("[HttpClient] exceed max request size: %zd, %s\n", 
            max_req_size_, res->GetUrl().c_str());
        return false;
    }
    __put_request(res);
    return true;
}

void HttpClient::__update_curent_time()
{
    timeval tv; 
    gettimeofday(&tv, NULL);
    cur_time_ = (tv.tv_sec*1000000 + tv.tv_usec) / 1000;
}

/** 
抓取有三个维度的限制：
1) serv的并发连接限制(ConnectionAvailable)；
2) 内核抓取排队长度限制(IsOverload)；
3) serv的抓取速度限制；
对于满足了1，但是不满足2或3，会放入wait_lst_map_里
 **/
void HttpClient::__fetch_serv(ServChannel* serv)
{
    while(channel_manager_->WaitResCnt(serv) && 
        channel_manager_->ConnectionAvailable(serv))
    {
        time_t ready_time = serv->GetReadyTime();
        if(fetcher_->IsOverload())
        {
            LOG_INFO("notice: fetcher overload.\n");
            wait_lst_map_.add_front(ready_time, *serv);
            break;
        }
        if(ready_time > cur_time_)
        {
            wait_lst_map_.add_back(ready_time, *serv);
            break;
        }
        Resource* res = channel_manager_->PopAvailableResource(serv);
        __fetch_resource(res);
        serv->SetFetchTime(cur_time_);
    }
}

void HttpClient::__fetch_resource(Resource* p_res)
{
    assert(p_res);
    p_res->cur_retry_times_++;
    RawFetcherRequest request;
    request.conn = p_res->conn_;
    request.context = p_res;
    fetcher_->PutRequest(request);
    cur_req_size_ += 1;
    //进入内核后不再控制超时，从超时队列中删除
    timed_lst_map_.del(*p_res);
}

//handle wait list
time_t HttpClient::__handle_wait_list()
{
    __update_curent_time();
    while(!wait_lst_map_.empty() && !fetcher_->IsOverload())
    {
        time_t timeout = 0;
        ServChannel * serv_channel = NULL;
        wait_lst_map_.get_front(timeout, serv_channel);
        if(timeout > cur_time_)
            return timeout - cur_time_;
        wait_lst_map_.pop_front();
        if(!channel_manager_->WaitResCnt(serv_channel))
            continue;
        __fetch_serv(serv_channel);
    }
    return 0;
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
        storage_->DestroyResource(p_res);
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
    channel_manager_->ReleaseConnection(res);
    ServChannel * serv = res->serv_;
    int err_num = fetch_result.err_num;
    time_t resp_time = 0;
    if(res->arrive_time_ < cur_time_)
        resp_time = cur_time_ - res->arrive_time_;
    serv->AddRespTime(resp_time);

    do
    {
        if(err_num)
        {
            FetchErrorType fetch_error(__srv_error_group(err_num), err_num);
            ProcessFailResult(fetch_error, res, resp);
            break;
        }
        assert(resp);
        if (resp->SizeExceeded())
        {
            FetchErrorType fetch_error(FETCH_FAIL_GROUP_RULE, RS_INVALID_PAGESIZE);
            ProcessFailResult(fetch_error, res, resp);
            break;
        }
        if(2 == resp->StatusCode / 100)
        {
            HandleHttpResponse2xx(res, resp);
            break;
        }
        if(3 == resp->StatusCode / 100) 
        {
            HandleHttpResponse3xx(res, resp);
            break;
        }
        FetchErrorType fetch_error(FETCH_FAIL_GROUP_HTTP, resp->StatusCode);
        ProcessFailResult(fetch_error, res, resp);
    }while(false);

    //后续处理
    storage_->DestroyResource(res);
    delete resp;
    if(serv->queue_node_.empty())
        __fetch_serv(serv);
}

void HttpClient::__pool()
{
    struct timeval timeout; 
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    __update_curent_time();
    //handle fetch result
    RawFetcherResult fetch_result;
    while (fetcher_->GetResult(&fetch_result, &timeout) == 0) 
        ProcessResult(fetch_result);
    __handle_wait_list();
    __handle_timeout_list();
}

void HttpClient::SetResultCallback(ResultCallback call_cb)
{
    result_cb_ = call_cb;
}
