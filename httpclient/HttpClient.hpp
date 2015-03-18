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
    struct FetchResult 
    {
        FetchErrorType  error_;
        IFetchMessage * resp_;
        Resource      * res_;
        void          * contex_;

        FetchResult(FetchErrorType error, IFetchMessage *resp,
            Resource* res, void* contex): 
            error_(error), resp_(resp), 
            res_(res),  contex_(contex)
        {}
        ~FetchResult()
        {
            if(resp_)
                delete resp_;
            if(res_)
                Storage::Instance()->DestroyResource(res_); 
        }
    };

    typedef boost::shared_ptr<FetchResult> ResultPtr;
    typedef boost::function<void (ResultPtr) > ResultCallback;
    typedef CQueue<ResultPtr > ResultQueue;

private:
    static const unsigned DEFAULT_REQUEST_SIZE = 1000000;
    static const unsigned DEFAULT_RESULT_SIZE  = 1000000;
    typedef linked_list_map<time_t, ServChannel, &ServChannel::queue_node_> ServWaitMap;
    typedef linked_list_map<time_t, Resource, &Resource::timed_lst_node_> ResTimedMap;

private:
    void __fetch_resource(Resource* p_res);
    void __fetch_serv(ServChannel* serv);
    void __handle_request(Resource*);
    time_t __handle_timeout_list();
    void __update_curent_time();
    REDIRECT_TYPE __get_redirect_type(int status_code);

protected:
    static  void* RunThread(void *context);
    void UpdateBatchConfig(std::string&, const BatchConfig&);
    void FetchServ(ServChannel* serv_channel);
    void Pool();
    void PutResult(FetchErrorType, IFetchMessage*, Resource*, void*);
    time_t CheckWaitList();

    virtual struct RequestData* CreateRequestData(void *);
    virtual void FreeRequestData(struct RequestData *);
    virtual IFetchMessage* CreateFetchResponse(const FetchAddress&, void *);
    virtual void FreeFetchMessage(IFetchMessage *);

    virtual void ProcessResult(RawFetcherResult&);
    virtual void ProcessSuccResult(Resource*, IFetchMessage*);
    virtual void ProcessFailResult(FetchErrorType, Resource*, IFetchMessage*);
    virtual void HandleDnsResult(std::string, struct addrinfo*, const void*);
    virtual void HandleRedirectResult(Resource*, HttpFetcherResponse*, RedirectInfo);
    virtual void HandleHttpResponse3xx(Resource*, HttpFetcherResponse *);
    virtual void HandleHttpResponse2xx(Resource*, HttpFetcherResponse *);

public:
    HttpClient(size_t max_conn_size, size_t max_req_size, 
        size_t max_result_size, const char* eth_name = NULL);
    void SetResultCallback(ResultCallback call_cb);
    virtual bool PutRequest(
       const std::string& url,
       void*  contex = NULL,
       MessageHeaders* user_headers = NULL,
       ResourcePriority prior = BatchConfig::DEFAULT_RES_PRIOR,
       std::string batch_id   = BatchConfig::DEFAULT_BATCH_ID);
    virtual bool PutRequest(Resource* res);
    virtual void Close();

private:
    boost::shared_ptr<ThreadingFetcher> fetcher_;
    boost::shared_ptr<DNSResolver>      dns_resolver_;
    ResultCallback                      result_cb_;

    //抓取等待队列，精度为毫秒
    ServWaitMap  wait_lst_map_;
    //超时队列, 精度为秒
    ResTimedMap  timed_lst_map_;
    ResultQueue  result_queue_;
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
};

#endif
