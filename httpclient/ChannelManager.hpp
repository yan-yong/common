#ifndef __CHANNEL_MANAGER_HPP
#define __CHANNEL_MANAGER_HPP

#include "Channel.hpp" 
#include "singleton/Singleton.h"

/*** function **/
class ChannelManager
{
    DECLARE_SINGLETON(ChannelManager);
    typedef linked_list_map<time_t, ServChannel, &ServChannel::queue_node_> ServWaitMap;

private:
    HostCacheList host_cache_lst_;
    SpinLock      host_cache_lock_; 
    unsigned      host_cache_cnt_;
    ServCacheList serv_cache_lst_;
    SpinLock      serv_cache_lock_; 
    unsigned      serv_cache_cnt_;
    ServWaitMap   serv_wait_lst_map_;
    SpinLock      serv_wait_lock_;  
    time_t        min_ready_time_;

protected:
    void __update_serv_host_state(HostChannel* host_channel);
    Resource* __pop_resource(HostChannel* host_channel);
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
               serv_channel->wait_host_lst_.empty() && 
               (!serv_channel->pres_wait_queue_ || 
               serv_channel->pres_wait_queue_->empty());
    }
    bool __wait_empty(ServChannel* serv_channel)
    {
        return serv_channel->wait_host_lst_.empty() &&
               (!serv_channel->pres_wait_queue_ || 
               serv_channel->pres_wait_queue_->empty());
    }
    bool __wait_empty(HostChannel* host_channel)
    {
        return host_channel->res_wait_queue_.empty();
    }

protected:
    void CheckAddCache(ServChannel* serv_channel);
    void CheckAddCache(HostChannel* host_channel);
    void CheckRemoveCache(ServChannel* serv_channel);
    void CheckRemoveCache(HostChannel* host_channel);
    Resource* PopResource(ServChannel* serv_channel);
    std::vector<Resource*> PopAvailableResources(ServChannel*, 
        std::vector<Resource*>&);
    std::vector<Resource*> PopAvailableResources(unsigned max_count);

public:
    unsigned GetHostCacheSize() { return host_cache_cnt_;}
    unsigned GetServCacheSize() { return serv_cache_cnt_;}

    bool WaitEmpty(ServChannel* serv_channel);
    bool HasAvailableResource(); 
    void SetServChannel(HostChannel*, ServChannel *);
    void DestroyChannel(HostChannel* host_channel);
    void DestroyChannel(ServChannel* serv_channel);
    ServChannel* CreateServChannel(
        char scheme, struct addrinfo* ai, 
        ServChannel::ServKey serv_key, unsigned max_err_rate, 
        ServChannel::ConcurencyMode concurency_mode, 
        struct sockaddr* local_addr);
    HostChannel* CreateHostChannel(
        char scheme, const std::string& host, unsigned port, 
        HostChannel::HostKey host_key, unsigned fetch_interval_ms);

    void AddResource(Resource* res);
    void RemoveResource(Resource*);
    void ReleaseConnection(Resource* res);
    void SetFetchIntervalMs(HostChannel*, unsigned);
    std::string ToString(HostChannel* host_channel) const;
    ResourceList RemoveUnfinishRes(ServChannel* serv_channel);
    ResourceList RemoveUnfinishRes(HostChannel* host_channel);
    HostCacheList PopHostCache(unsigned cnt);
    ServCacheList PopServCache(unsigned cnt);
};

#endif 
