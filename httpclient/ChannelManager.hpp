#ifndef __CHANNEL_MANAGER_HPP
#define __CHANNEL_MANAGER_HPP

#include "Channel.hpp" 
#include "singleton/Singleton.h"

/*** function **/
class ChannelManager
{
    DECLARE_SINGLETON(ChannelManager)
private:
    HostCacheList host_cache_lst_;
    SpinLock host_cache_lock_; 
    unsigned host_cache_cnt_;
    ServCacheList serv_cache_lst_;
    SpinLock serv_cache_lock_; 
    unsigned serv_cache_cnt_;

protected:
    void __update_serv_host_state(HostChannel* host_channel);
    Resource* __pop_resource(HostChannel* host_channel);
    unsigned __wait_res_cnt(ServChannel* serv_channel);
    void __release_connection(ServChannel* , Connection* );
    Connection* __acquire_connection(ServChannel* serv_channel);

    /**** inline operation ****/
    SpinLock& __serv_lock(ServChannel * serv_channel)
    {
        return serv_channel ? serv_channel->lock_:*((SpinLock*)NULL);
    }
    bool __empty(HostChannel* host_channel)
    {
        return host_channel->ref_cnt_ == 0;
    }
    bool __empty(ServChannel* serv_channel)
    {
        return serv_channel->fetching_lst_.empty() && 
            serv_channel->wait_host_lst_.empty();
    }
    unsigned __wait_res_cnt(HostChannel* host_channel)
    {
        return host_channel->res_lst_map_.size();
    }
    //cache operation
    void check_add_cache(ServChannel* serv_channel)
    {
        if(__empty(serv_channel) && (serv_channel->cache_node_).empty())
        {
            SpinGuard guard(serv_cache_lock_);
            serv_cache_lst_.add_back(*serv_channel);
            serv_cache_cnt_++;
        }
    }
    void check_add_cache(HostChannel* host_channel)
    {
        if(__empty(host_channel) && host_channel->cache_node_.empty())
        {
            SpinGuard guard(host_cache_lock_);
            host_cache_lst_.add_back(*host_channel);
            host_cache_cnt_++;
        }
    }
    void check_remove_cache(ServChannel* serv_channel)
    {
        if(!__empty(serv_channel) && !serv_channel->cache_node_.empty())
        {
            SpinGuard guard(serv_cache_lock_);
            ServCacheList::del(*serv_channel);
            serv_cache_cnt_--;
        }
    }
    void check_remove_cache(HostChannel* host_channel)
    {
        if(!__empty(host_channel) && !host_channel->cache_node_.empty())
        {
            SpinGuard guard(host_cache_lock_);
            HostCacheList::del(*host_channel);
            host_cache_cnt_--;
        }
    }

public:
    unsigned WaitResCnt(ServChannel* serv_channel);
    unsigned WaitResCnt(HostChannel* host_channel);
    void SetServChannel(HostChannel*, ServChannel *);
    void DestroyChannel(HostChannel* host_channel);
    void DestroyChannel(ServChannel* serv_channel);
    void AddResource(HostChannel* host_channel, Resource* res);
    unsigned GetHostCacheSize() { return host_cache_cnt_;}
    unsigned GetServCacheSize() { return serv_cache_cnt_;}
    HostCacheList PopHostCache(unsigned cnt);
    ServCacheList PopServCache(unsigned cnt);
    void SetFetchIntervalMs(HostChannel*, unsigned);
    void RemoveResource(Resource*);
    //Resource* PopResource(HostChannel* host_channel);
    //Resource* PopResource(ServChannel* serv_channel);
    Resource* PopAvailableResource(ServChannel* serv_channel);
    std::string ToString(HostChannel* host_channel) const;
    bool ConnectionAvailable(ServChannel* serv_channel);
    ServChannel* CreateServChannel(
        char scheme, struct addrinfo* ai, 
        ServChannel::ServKey serv_key, unsigned max_err_rate, 
        ServChannel::ConcurencyMode concurency_mode, 
        struct sockaddr* local_addr);
    HostChannel* CreateHostChannel(
        char scheme, const std::string& host, unsigned port, 
        HostChannel::HostKey host_key, unsigned fetch_interval_ms);
    ResourceList RemoveUnfinishRes(ServChannel* serv_channel);
    void ReleaseConnection(Resource* res);
};

#endif 
