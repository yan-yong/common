#include "utility/net_utility.h"
#include "ChannelManager.hpp"

DEFINE_SINGLETON(ChannelManager);

void ChannelManager::__update_serv_host_state(HostChannel* host_channel)
{
    ServChannel* serv_ = host_channel->serv;
    if(serv_)
    {
        HostChannelList::del(*host_channel);
        if(host_channel->res_lst_map_.size())
            serv_->wait_host_lst_.add_back(*host_channel);
        else 
            serv_->idle_host_lst_.add_back(*host_channel);
    }
}

unsigned ChannelManager::__wait_res_cnt(ServChannel* serv_channel)
{
    if(!serv_channel || serv_channel->wait_host_lst_.empty())
        return 0;
    size_t cnt = 0;
    HostChannel* host_channel = serv_channel->wait_host_lst_.get_front();
    while(host_channel)
    {
        cnt += __wait_res_cnt(host_channel);
        host_channel = serv_channel->wait_host_lst_.next(*host_channel); 
    }
    return cnt; 
}

Resource* ChannelManager::__pop_resource(HostChannel* host_channel)
{
    if(__wait_res_cnt(host_channel))
        return NULL;
    Resource* p_res = NULL;
    ResourcePriority prior = RES_PRIORITY_LEVEL_5;
    host_channel->res_lst_map_.get_front(prior, p_res);
    host_channel->res_lst_map_.pop_front();
    // 检查host状态变迁 && host轮转
    __update_serv_host_state(host_channel);
    return p_res;
}

void ChannelManager::__release_connection(ServChannel* serv_channel, Connection* conn)
{
    if(conn)
    {
        serv_channel->conn_storage_.push_back(conn);
        serv_channel->conn_using_cnt_ -= 1;
    }
}

Connection* ChannelManager::__acquire_connection(ServChannel* serv_channel)
{
    Connection* conn = NULL;
    switch(serv_channel->concurency_mode_)
    {
        case NO_CONCURENCY:
        case CONCURENCY_PER_SERV:
            {
                conn = serv_channel->conn_storage_.front();
                serv_channel->conn_storage_.pop();
                break;
            }
        case CONCURENCY_NO_LIMIT:
            {
                assert(serv_channel->conn_storage_.size() > 0);
                conn = serv_channel->conn_storage_.front();
                serv_channel->conn_storage_.pop();
                if(serv_channel->conn_storage_.size() == 0)
                {
                    Connection* new_conn = create_connection(copy_addrinfo(conn->address.remote_addr));
                    serv_channel->conn_storage_.push_back(new_conn);
                }
                break;
            }
        default:
            assert(false);
    }
    serv_channel->conn_using_cnt_ += 1;
    return conn;
}

unsigned ChannelManager::WaitResCnt(ServChannel* serv_channel)
{
    SpinGuard guard(serv_channel->lock_);
    return __wait_res_cnt(serv_channel);
}

unsigned ChannelManager::WaitResCnt(HostChannel* host_channel)
{
    SpinGuard guard(host_channel->lock_);
    return __wait_res_cnt(host_channel);
}

void ChannelManager::SetServChannel(HostChannel* host_channel, ServChannel * serv_channel)
{
    ServChannel* last_serv = host_channel->serv_;
    //** 先处理原来的serv
    {
        SpinGuard serv_guard(__serv_lock(last_serv));
        SpinGuard host_guard(host_channel->lock_);
        if(serv_channel == last_serv)
            return last_serv;
        HostChannelList::del(*host_channel);
        check_add_cache(last_serv);
        host_channel->serv_ = serv_channel;
    }
    //** 处理新的serv 
    SpinGuard serv_guard(__serv_lock(serv_channel));
    SpinGuard host_guard(host_channel->lock_);
    //更新抓取速度
    if(host_channel->fetch_interval_ms_ && 
            fetch_interval_ms < serv_->fetch_interval_ms_)
    {
        serv_channel_->fetch_interval_ms_ = fetch_interval_ms;
    }
    __update_serv_host_state(host_channel);
    host_channel->update_time_ = time(NULL);
    check_remove_cache(serv_channel);
}

void ChannelManager::Destroy(HostChannel* host_channel)
{
    SpinGuard serv_guard(__serv_lock(host_channel->serv_));
    SpinGuard host_guard(host_channel->lock_);
    HostChannelList::del(*host_channel);
    if(!host_channel->cache_node_.empty())
    {
        SpinGuard cache_guard(host_cache_lock_);
        HostCacheList::del(*host_channel);
    }
    host_channel->serv_ = NULL;
    delete host_channel;
}

void ChannelManager::Destroy(ServChannel* serv_channel)
{
    SpinGuard guard(serv_channel->lock_);
    while(!serv_channel->conn_storage_.empty())
    {
        Connection * conn = serv_channel->conn_storage_.front();
        serv_channel->conn_storage_.pop();
        freeaddrinfo(conn->address.remote_addr);
        free(conn);
    }
    if(serv_channel->local_addr_)
        free(serv_channel->local_addr_);
    HostChannel * host_channel = NULL;
    while(!serv_channel->idle_host_lst_.empty())
    {
        host_channel = idle_host_lst_.get_front();
        SpinGuard guard(host_channel->lock_);
        HostChannelList::del(*host_channel);
        host_channel->serv_ = NULL;
    }
    if(!serv_channel->cache_node_.empty())
    {
        SpinGuard cache_guard(serv_cache_lock_);
        ServCacheList::del(*serv_channel);
    }
    delete serv_channel;
}

void ChannelManager::AddResource(HostChannel* host_channel, Resource* res)
{
    SpinGuard serv_guard(__serv_lock(host_channel->serv_));
    SpinGuard host_guard(host_channel->lock_);
    HostChannel serv_channel = host_channel->serv_;
    assert(res->queue_node_.empty());
    res->host_ = host_channel;
    host_channel->res_lst_map_.add_back(res->prior_, *res);
    ++host_channel->ref_cnt_;
    res->serv_ = host_channel->serv_;
    check_remove_cache(host_channel);
    if(__wait_res_cnt(host_channel) == 1) 
        __update_serv_host_state(host_channel);
}

HostCacheList ChannelManager::PopHostCache(unsigned cnt)
{
    HostCacheList cache_sub_lst;
    SpinGuard guard(host_cache_lock_);
    for(unsigned i = 0; i < cnt && !host_cache_lst_.empty(); i++)
    { 
        cache_sub_lst.add_back(host_cache_lst_.get_front());
        host_cache_lst_.pop_front();
    }
    return cache_sub_lst;
}

ServCacheList ChannelManager::PopServCache(unsigned cnt)
{
    ServCacheList cache_sub_lst;
    SpinGuard guard(serv_cache_lock_);
    for(unsigned i = 0; i < cnt && !serv_cache_lst_->empty(); i++)
    { 
        cache_sub_lst.add_back(serv_cache_lst_->get_front());
        serv_cache_lst_->pop_front();
    }
    return cache_sub_lst;
}

void ChannelManager::SetFetchIntervalMs(HostChannel* host_channel, unsigned fetch_interval_ms)
{
    ServChannel* serv_channel = __serv_lock(host_channel->serv_); 
    SpinGuard serv_guard(serv_channel->lock_);
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
        __release_connection(res->serv_, res->conn_);
        res->conn_ = NULL;
    }
}

void ChannelManager::RemoveResource(Resource* res)
{
    SpinGuard serv_guard(__serv_lock(res->serv_));
    SpinGuard host_guard(res->host_->lock_);
    res->host_->ref_cnt_--;
    if(res->serv_)
    {
        res->serv_->fetching_lst_.del(*res);
        if(res->conn_)
        {
            __release_connection(res->serv_, res->conn_);
            res->conn_ = NULL;
        }
        if(__empty(res->serv_))
            check_add_cache(res->serv_);
        res->serv_ = NULL;
    }
    res->host_ = NULL;
    if(res->host_->ref_cnt == 0)
        check_add_cache(res->host_); 
}

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
    return host_channel->__pop_resource();
}

Resource* ChannelManager::PopAvailableResource(ServChannel* serv_channel)
{
    SpinGuard serv_guard(serv_channel->lock_);
    if(serv_channel->wait_host_lst_.empty())
        return NULL;
    Connection* conn = __acquire_connection(serv_channel);
    assert(conn);
    HostChannel* host_channel = serv_channel->wait_host_lst_.get_front();
    SpinGuard host_guard(host_channel->lock_);
    conn->scheme = host_channel->GetScheme();
    Resource* res = __pop_resource(host_channel);
    if(res)
    {
        res->conn_ = conn;
        res->serv_ = this;
        fetching_lst_.add_back(*res);
    }
    return res;
}

std::string ChannelManager::ToString(HostChannel* host_channel) const
{
    std::string cont = protocal2str(host_channel->protocal_) + 
        "://" + host_channel->host_;
    if(!IsHttpDefaultPort(host_channel->protocal_, 
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
    size_t cur_size = serv_channel->conn_storage_.size();
    switch(serv_channel->concurency_mode_)
    {
        case NO_CONCURENCY:
            return conn_using_cnt_ == 0;
        case CONCURENCY_PER_SERV:
            return cur_size > 0;
        case CONCURENCY_NO_LIMIT:
            return true;
        default:
            assert(false);
    }
    return false;
}
