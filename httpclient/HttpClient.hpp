#ifndef __HTTP_CLIENT_HPP
#define __HTTP_CLIENT_HPP
#include <string>
#include <vector>
#include <queue>
#include <map>
#include <list>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include "fetcher/Fetcher.hpp"
#include "SchedulerTypes.hpp"
#include "queue/CQueue.h"
#include "linklist/linked_list.hpp"
#include "linklist/linked_list_map.hpp"
#include "Channel.hpp"
#include "Storage.hpp"
#include "dnsresolver/DNSResolver.hpp"
#include "TRedirectChecker.hpp"

class HttpClient: protected IMessageEvents
{
public:
    struct FetchRequest
    {
        URI uri_;
        void* contex_;
        MessageHeaders* user_headers_;
        char* content_;
        ResourcePriority prior_;
        BatchConfig * batch_cfg_;
        struct addrinfo * proxy_ai_;
        Resource*         root_res_;

        FetchRequest()
        {
            memset(&contex_, 0, sizeof(struct FetchRequest) - sizeof(URI));
        }

        FetchRequest(
            const URI& uri,
            void*  contex,
            MessageHeaders* user_headers,
            char* content,
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
            memset(&contex_, 0, sizeof(struct FetchRequest) - sizeof(URI));
            uri_ = uri;
            root_res_ = root_res;    
        }
    };

    struct FetchResult 
    {
        FetchErrorType  error_;
        HttpFetcherResponse*  resp_;
        void* contex_;

        FetchResult(FetchErrorType error, 
            HttpFetcherResponse *resp, void* contex): 
            error_(error), resp_(resp), contex_(contex)
        {}
        ~FetchResult()
        {
            if(resp_)
            {
                delete resp_;
                resp_ = NULL;
            }
        }
    };

    typedef boost::shared_ptr<FetchResult> ResultPtr;
    typedef FetchRequest* RequestPtr;
    typedef boost::function<void (ResultPtr) > ResultCallback;
    typedef CQueue<ResultPtr >   ResultQueue;
    typedef CQueue<RequestPtr >  RequestQueue;
    typedef DNSResolver::DnsResultType DnsResultType;
    typedef CQueue<DnsResultType > DnsResultQueue;

private:
    static const unsigned DEFAULT_REQUEST_SIZE = 1000000;
    static const unsigned DEFAULT_RESULT_SIZE  = 1000000;
    typedef linked_list_map<time_t, ServChannel, &ServChannel::queue_node_> ServWaitMap;

private:
    void __fetch_resource(Resource* p_res);
    void __fetch_serv(ServChannel* serv);
    time_t __handle_timeout_list();
    void __update_curent_time();
    REDIRECT_TYPE __get_redirect_type(int status_code);

protected:
    static  void* RunThread(void *context);
    void UpdateBatchConfig(std::string&, const BatchConfig&);
    void Pool();
    void PutResult(FetchErrorType, HttpFetcherResponse*, void*);
    void PutDnsResult(DnsResultType dns_result);
    void HandleRequest(RequestPtr req);

    virtual struct RequestData* CreateRequestData(void *);
    virtual void FreeRequestData(struct RequestData *);
    virtual IFetchMessage* CreateFetchResponse(const FetchAddress&, void *);
    virtual void FreeFetchMessage(IFetchMessage *);

    virtual void ProcessResult(RawFetcherResult&);
    virtual void ProcessSuccResult(Resource*, HttpFetcherResponse*);
    virtual void ProcessFailResult(FetchErrorType, Resource*, HttpFetcherResponse*);
    virtual void HandleDnsResult(DnsResultType dns_result);
    virtual void HandleRedirectResult(Resource*, HttpFetcherResponse*, RedirectInfo);
    virtual void HandleHttpResponse3xx(Resource*, HttpFetcherResponse *);
    virtual void HandleHttpResponse2xx(Resource*, HttpFetcherResponse *);
    virtual bool HandleProxyResult(RawFetcherResult& fetch_result);

public:
    HttpClient(
        size_t max_req_size    = DEFAULT_REQUEST_SIZE,
        size_t max_result_size = DEFAULT_RESULT_SIZE,
        const char* eth_name   = NULL);
    virtual bool PutRequest(
       const std::string& url,
       void*  contex = NULL,
       MessageHeaders* user_headers = NULL,
       char* content = NULL,
       ResourcePriority prior = BatchConfig::DEFAULT_RES_PRIOR,
       std::string batch_id   = BatchConfig::DEFAULT_BATCH_ID, 
       struct addrinfo * proxy_ai   = NULL);
    //virtual bool PutRequest(Resource* res);
    virtual void Open();
    virtual void Close();
    void SetResultCallback(ResultCallback call_cb);
    void SetDefaultServConfig(ServChannel::ConcurencyMode, 
        double, unsigned, unsigned);
    void SetDefaultBatchConfig(const BatchConfig& batch_cfg);
    void SetFetcherParams(Fetcher::Params params);
    void SetDnsCacheTime(time_t dns_update_time, time_t dns_error_time);

private:
    boost::shared_ptr<ThreadingFetcher> fetcher_;
    boost::shared_ptr<DNSResolver>      dns_resolver_;
    ResultCallback                      result_cb_;

    //抓取等待队列，精度为毫秒
    ServWaitMap  wait_lst_map_;
    //超时队列, 精度为秒
    ResTimedMap  timed_lst_map_;
    RequestQueue request_queue_;
    ResultQueue  result_queue_;
    DnsResultQueue dns_queue_;
    size_t max_req_size_;
    size_t max_result_size_;
    volatile size_t cur_req_size_;
    //当前时间，单位为毫秒
    time_t cur_time_;
    bool stopped_;
    pthread_t tid_;
    ChannelManager* channel_manager_;
    sockaddr * local_addr_;
    SpinLock wait_lst_lock_;

    //serv配置
    ServChannel::ConcurencyMode serv_concurency_mode_;
    double   serv_max_err_rate_;
    unsigned serv_err_delay_sec_;
    unsigned serv_max_err_count_;

    //fetcher配置
    Fetcher::Params fetcher_params_;

    //默认Batch配置
    BatchConfig default_batch_cfg_;
    
    //dns配置
    time_t   dns_update_time_;
    time_t   dns_error_time_;
};

#endif
