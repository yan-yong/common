#include "utility/net_utility.h"
#include "ChannelManager.hpp"
#include "fetcher/Fetcher.hpp"

DEFINE_SINGLETON(ChannelManager);

ChannelManager::ChannelManager()
{
    host_cache_cnt_ = 0;
    serv_cache_cnt_ = 0;
}

void ChannelManager::__update_serv_host_state(HostChannel* host_channel)
{
    ServChannel* serv = host_channel->serv_;
    if(serv)
    {
        HostChannelList::del(*host_channel);
        if(host_channel->res_wait_queue_.size())
            serv->wait_host_lst_.add_back(*host_channel);
        else 
            serv->idle_host_lst_.add_back(*host_channel);
    }
}

Resource* ChannelManager::__pop_resource(HostChannel* host_channel)
{
    if(__wait_empty(host_channel))
        return NULL;
    Resource* p_res = NULL;
    ResourcePriority prior = RES_PRIORITY_LEVEL_5;
    host_channel->res_wait_queue_.get_front(prior, p_res);
    host_channel->res_wait_queue_.pop_front();
    // 检查host状态变迁 && host轮转
    __update_serv_host_state(host_channel);
    return p_res;
}

Connection* ChannelManager::__acquire_connection(ServChannel* serv_channel)
{
    Connection* conn = serv_channel->conn_storage_.front();
    serv_channel->conn_storage_.pop_front();
    if(serv_channel->concurency_mode_ == ServChannel::CONCURENCY_NO_LIMIT)
        serv_channel->conn_storage_.push_back(conn);
    return conn;
}

bool ChannelManager::WaitEmpty(ServChannel* serv_channel)
{
    SpinGuard guard(serv_channel->lock_);
    return __wait_empty(serv_channel);
}

void ChannelManager::SetServChannel(HostChannel* host_channel, ServChannel * serv_channel)
{
    ServChannel* last_serv = host_channel->serv_;
    //** 先处理原来的serv
    {
        SpinGuard serv_guard(__serv_lock(last_serv));
        SpinGuard host_guard(host_channel->lock_);
        if(serv_channel->serv_key_ == last_serv->serv_key_)
            return;
        HostChannelList::del(*host_channel);
        check_add_cache(last_serv);
        host_channel->serv_ = serv_channel;
    }
    //** 处理新的serv 
    SpinGuard serv_guard(__serv_lock(serv_channel));
    SpinGuard host_guard(host_channel->lock_);
    //更新抓取速度
    if(host_channel->fetch_interval_ms_ && 
        host_channel->fetch_interval_ms_ < serv_channel->fetch_interval_ms_)
    {
        serv_channel->fetch_interval_ms_ = host_channel->fetch_interval_ms_;
    }
    __update_serv_host_state(host_channel);
    host_channel->update_time_ = time(NULL);
    check_remove_cache(serv_channel);
}

void ChannelManager::DestroyChannel(HostChannel* host_channel)
{
    SpinGuard serv_guard(__serv_lock(host_channel->serv_));
    SpinGuard host_guard(host_channel->lock_);
    assert(__empty(host_channel));
    if(!host_channel->cache_node_.empty())
    {
        SpinGuard cache_guard(host_cache_lock_);
        HostCacheList::del(*host_channel);
    }
    HostChannelList::del(*host_channel);
    delete host_channel;
}

void ChannelManager::DestroyChannel(ServChannel* serv_channel)
{
    SpinGuard guard(serv_channel->lock_);
    assert(__empty(serv_channel));
    //remove from cache list
    if(!serv_channel->cache_node_.empty())
    {
        SpinGuard cache_guard(serv_cache_lock_);
        ServCacheList::del(*serv_channel);
    }
    //erase connection
    while(!serv_channel->conn_storage_.empty())
    {
        Connection * conn = serv_channel->conn_storage_.front();
        serv_channel->conn_storage_.pop_front();
        ThreadingFetcher::FreeConnection(conn); 
    }
    //remove host channel && resource
    HostChannel * host_channel = NULL;
    while(!serv_channel->idle_host_lst_.empty())
    {
        host_channel = serv_channel->idle_host_lst_.get_front();
        SpinGuard guard(host_channel->lock_);
        HostChannelList::del(*host_channel);
        host_channel->serv_ = NULL;
    }
    delete serv_channel;
}

ResourceList ChannelManager::RemoveUnfinishRes(ServChannel* serv_channel)
{
    SpinGuard guard(serv_channel->lock_);
    ResourceList unfinish_lst;
    HostChannel * host_channel = NULL;
    while(!serv_channel->wait_host_lst_.empty())
    {
        host_channel = serv_channel->wait_host_lst_.get_front();
        SpinGuard guard(host_channel->lock_);
        ResourceList wait_lst = (host_channel->res_wait_queue_).splice();
        unfinish_lst.splice_front(wait_lst);
    }
    unfinish_lst.splice_front(serv_channel->fetching_lst_);
    if(serv_channel->pres_wait_queue_)
    {
        ResourceList wait_lst = serv_channel->pres_wait_queue_->splice();
        unfinish_lst.splice_front(wait_lst);
    }
    return unfinish_lst;
}

ResourceList ChannelManager::RemoveUnfinishRes(HostChannel* host_channel)
{
    SpinGuard guard(host_channel->lock_);
    ResourceList unfinish_lst = host_channel->res_wait_queue_.splice();
    __update_serv_host_state(host_channel);
    return unfinish_lst;
}

//resource挂到HostChannel下
void ChannelManager::AddResource(Resource* res)
{
    assert(res->queue_node_.empty());
    HostChannel* host_channel = res->host_;
    ServChannel* serv_channel = host_channel->serv_;
    if(res->serv_)
        serv_channel = res->serv_;

    SpinGuard serv_guard(__serv_lock(serv_channel));
    SpinGuard host_guard(host_channel->lock_);
    ++host_channel->ref_cnt_;
    check_remove_cache(host_channel);
    //如果Resource指定了ServChannel，则res直接挂到ServChannel下
    if(res->serv_)
    {
        if(!serv_channel->pres_wait_queue_)
            serv_channel->pres_wait_queue_ = new ResPriorQueue;
        serv_channel->pres_wait_queue_->add_back(res->prior_, *res);
    }
    //否则，res挂到HostChannel下
    else
    {
        bool host_wait_empty = __wait_empty(host_channel);
        res->serv_ = host_channel->serv_;
        host_channel->res_wait_queue_.add_back(res->prior_, *res);
        if(host_wait_empty) 
            __update_serv_host_state(host_channel);
    }
    if(serv_channel)
        check_remove_cache(serv_channel);
}

HostCacheList ChannelManager::PopHostCache(unsigned cnt)
{
    HostCacheList cache_sub_lst;
    SpinGuard guard(host_cache_lock_);
    for(unsigned i = 0; i < cnt && !host_cache_lst_.empty(); i++)
    { 
        cache_sub_lst.add_back(*host_cache_lst_.get_front());
        host_cache_lst_.pop_front();
    }
    return cache_sub_lst;
}

ServCacheList ChannelManager::PopServCache(unsigned cnt)
{
    ServCacheList cache_sub_lst;
    SpinGuard guard(serv_cache_lock_);
    for(unsigned i = 0; i < cnt && !serv_cache_lst_.empty(); i++)
    { 
        cache_sub_lst.add_back(*serv_cache_lst_.get_front());
        serv_cache_lst_.pop_front();
    }
    return cache_sub_lst;
}

void ChannelManager::SetFetchIntervalMs(HostChannel* host_channel, 
    unsigned fetch_interval_ms)
{
    ServChannel * serv_channel = host_channel->serv_;
    SpinGuard serv_guard(__serv_lock(serv_channel));
    SpinGuard host_guard(host_channel->lock_);
    host_channel->fetch_interval_ms_ = fetch_interval_ms;
    if(fetch_interval_ms && fetch_interval_ms < serv_channel->fetch_interval_ms_)
        serv_channel->fetch_interval_ms_ = fetch_interval_ms;
}

void ChannelManager::ReleaseConnection(Resource* res)
{
    SpinGuard serv_guard(__serv_lock(res->serv_));
    if(res->serv_ && res->conn_)
    {
        if(res->serv_->concurency_mode_ != ServChannel::CONCURENCY_NO_LIMIT)
            (res->serv_->conn_storage_).push_back(res->conn_);
        res->conn_ = NULL;
    }
}

void ChannelManager::RemoveResource(Resource* res)
{
    SpinGuard serv_guard(__serv_lock(res->serv_));
    if(res->serv_)
    {
        ResourceList::del(*res);
        if(res->conn_)
        {
            (res->serv_->conn_storage_).push_back(res->conn_);
            res->conn_ = NULL;
        }
        check_add_cache(res->serv_);
    }
    SpinGuard host_guard(res->host_->lock_);
    --res->host_->ref_cnt_;
    check_add_cache(res->host_); 
    res->host_ = NULL;
    res->serv_ = NULL;
}

Resource* ChannelManager::PopAvailableResource(ServChannel* serv_channel)
{
    SpinGuard serv_guard(serv_channel->lock_);
    if(__wait_empty(serv_channel) || serv_channel->conn_storage_.size() == 0)
    {
        return NULL;
    }
    Connection* conn = __acquire_connection(serv_channel);
    Resource* res = NULL;
    //如果pres_wait_queue_和HostChannel里都有Resource排队，
    //各按50%的比例
    if(serv_channel->pres_wait_queue_ &&
       !serv_channel->pres_wait_queue_->empty() && 
       (serv_channel->wait_host_lst_.empty() || rand() % 2))
    {
        ResourcePriority prior = RES_PRIORITY_NOUSE;
        serv_channel->pres_wait_queue_->get_front(prior, res);
        serv_channel->pres_wait_queue_->pop_front();
    }
    else
    {
        HostChannel* host_channel = serv_channel->wait_host_lst_.get_front();
        SpinGuard host_guard(host_channel->lock_);
        res = __pop_resource(host_channel);
        //conn->scheme = host_channel->GetScheme();
        if(res)
        {
            res->conn_ = conn;
            res->serv_ = serv_channel;
        }
    }
    (serv_channel->fetching_lst_).add_back(*res);
    return res;
}

std::string ChannelManager::ToString(HostChannel* host_channel) const
{
    std::string cont = protocal2str(host_channel->scheme_) + 
        "://" + host_channel->host_;
    if(!IsHttpDefaultPort(host_channel->scheme_, 
                host_channel->port_))
    {
        char port_str[10];
        snprintf(port_str, 10, ":%hu", host_channel->port_);
        cont += port_str;
    }
    return cont;
}

bool ChannelManager::ConnectionAvailable(ServChannel* serv_channel)
{
    SpinGuard guard(serv_channel->lock_);
    return serv_channel->conn_storage_.size() > 0;
}

ServChannel* ChannelManager::CreateServChannel(
    char scheme, struct addrinfo* ai, 
    ServChannel::ServKey serv_key, unsigned max_err_rate, 
    ServChannel::ConcurencyMode concurency_mode, 
    struct sockaddr* local_addr)
{
    ServChannel * serv = new ServChannel();
    serv->concurency_mode_ = concurency_mode;
    serv->serv_key_ = serv_key;
    serv->max_err_rate_ = max_err_rate;
    struct addrinfo * cur_ai = ai;
    while(!cur_ai)
    {
        FetchAddress fetch_addr;
        fetch_addr.remote_addr    = cur_ai->ai_addr;
        fetch_addr.remote_addrlen = sizeof(struct sockaddr);
        fetch_addr.local_addr     = local_addr;
        fetch_addr.local_addr     = local_addr;
        if(!fetch_addr.local_addr)
            fetch_addr.local_addrlen = 0;
        else
            fetch_addr.local_addrlen = sizeof(struct sockaddr);
        Connection* conn = ThreadingFetcher::CreateConnection(
            (int)scheme, cur_ai->ai_family, cur_ai->ai_socktype, 
            cur_ai->ai_protocol, fetch_addr);
        serv->conn_storage_.push_back(conn); 
        cur_ai = cur_ai->ai_next;
    }
    return serv;
}

HostChannel* ChannelManager::CreateHostChannel(
    char scheme, const std::string& host, unsigned port, 
    HostChannel::HostKey host_key, unsigned fetch_interval_ms)
{
    HostChannel* host_channel = new HostChannel();
    host_channel->scheme_     = scheme;
    host_channel->host_       = host;
    host_channel->port_       = port;
    host_channel->host_key_   = host_key;
    host_channel->fetch_interval_ms_ = fetch_interval_ms;
    return host_channel;
}

/*
Resource* ChannelManager::PopResource(HostChannel* host_channel)
{
    SpinGuard serv_guard(__serv_lock(host_channel->serv_));
    SpinGuard host_guard(host_channel->lock_);
    return __pop_resource(host_channel);
}

Resource* ChannelManager::PopResource(ServChannel* serv_channel)
{
    SpinGuard serv_guard(serv_channel->lock_);
    if(serv_channel->wait_host_lst_.empty())
        return NULL;
    HostChannel* host_channel = 
        serv_channel->wait_host_lst_.get_front();
    SpinGuard host_guard(host_channel->lock_);
    return __pop_resource(host_channel);
}
*/
