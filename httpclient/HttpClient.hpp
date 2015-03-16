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

struct FetchResult 
{
    FetchErrorType error_;
    IFetchMessage * resp_;
    void *        contex_;
    
    FetchResult(FetchErrorType error, IFetchMessage *resp, 
        void* contex): 
    error_(error), resp_(resp)
    {}
};

class HttpClient: protected IMessageEvents
{
    static const unsigned DEFAULT_REQUEST_SIZE = 1000000;
    static const unsigned DEFAULT_RESULT_SIZE  = 1000000;

    typedef linked_list_map<time_t, ServChannel, &ServChannel::queue_node_> ServWaitMap;
    typedef linked_list_map<time_t, Resource, &Resource::timed_lst_node_> ResTimedMap;
    typedef boost::shared_ptr<FetchResult> ResultPtr;
    typedef CQueue<ResultPtr > ResultQueue;
    typedef boost::function<void (Resource*, IFetchMessage*)> SucCallback;
    typedef boost::function<void (FetchErrorType, Resource*, IFetchMessage*)> FailCallback;

private:
    void __pool();
    void __fetch_resource(Resource* p_res);
    void __fetch_serv(ServChannel* serv);
    void __put_request(Resource*);
    time_t __handle_timeout_list();
    time_t __handle_wait_list();
    void __update_curent_time();
    REDIRECT_TYPE __get_redirect_type(int status_code);

protected:
    virtual struct RequestData* CreateRequestData(void * request_context);
    virtual void FreeRequestData(struct RequestData * request_data);
    virtual IFetchMessage* CreateFetchResponse(const FetchAddress& address, void * request_context);
    virtual void FreeFetchMessage(IFetchMessage *fetch_message);

    void UpdateBatchConfig(std::string& batch_id, const BatchConfig&);
    virtual void ProcessResult(RawFetcherResult& result);
    virtual void ProcessSuccResult(Resource*, IFetchMessage*);
    virtual void ProcessFailResult(FetchErrorType, Resource*, IFetchMessage*);
    virtual void HandleRedirectResult(Resource*, HttpFetcherResponse*, RedirectInfo);
    virtual void HandleHttpResponse3xx(Resource* res, HttpFetcherResponse *resp);
    virtual void HandleDnsResult(std::string err_msg, struct addrinfo* ai, const void* contex);
    virtual void HandleHttpResponse2xx(Resource* res, HttpFetcherResponse *resp);

public:
    HttpClient(size_t max_conn_size, size_t max_req_size, size_t max_result_size, const char* eth_name = NULL);
    virtual bool PutRequest(
       const   std::string& url,
       void*  contex = NULL,
       MessageHeaders* user_headers = NULL,
       ResourcePriority prior = BatchConfig::DEFAULT_RES_PRIOR,
       std::string batch_id = BatchConfig::DEFAULT_BATCH_ID
    );
    virtual bool PutRequest(Resource* res);
    virtual void Close();
    void SetProcCallback(SucCallback suc_cb, FailCallback fail_cb);
    static  void* RunThread(void *context);

private:
    boost::shared_ptr<ThreadingFetcher> fetcher_;
    boost::shared_ptr<Storage>          storage_; 
    boost::shared_ptr<DNSResolver>      dns_resolver_;
    SucCallback                         suc_cb_;
    FailCallback                        fail_cb_;

    //抓取等待队列，精度为毫秒
    ServWaitMap  wait_lst_map_;
    //超时队列, 精度为秒
    ResTimedMap timed_lst_map_;
    ResultQueue  result_queue_;
    size_t max_req_size_;
    size_t cur_req_size_;
    size_t max_result_size_;
    //当前时间，单位为毫秒
    time_t cur_time_;
    bool stopped_;
    pthread_t tid_;
    ChannelManager* channel_manager_;
    sockaddr * local_addr_;
};
