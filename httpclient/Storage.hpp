#ifndef __STORAGE_HPP
#define __STORAGE_HPP

#include <openssl/md5.h>
#include <boost/unordered_map.hpp>
#include "singleton/Singleton.h"
#include "Channel.hpp"
#include "lock/lock.hpp"
#include "utility/murmur_hash.h"
#include "linklist/linked_list.hpp"
#include "ChannelManager.hpp"
#include "singleton/Singleton.h"
/*
* Storage负责：
* 1. Resource的创建和销毁;
* 2. HostChannel, ServChannel的创建和销毁
* 3. BatchConfig的创建和销毁
 */

class Storage 
{
    DECLARE_SINGLETON(Storage);
    static const unsigned MAX_CACHE_HOST     = 100000;
    static const unsigned MAX_CACHE_SERV     = 100000;
    static const double   EXCEED_DELETE_RATE = 0.1;
    static const int ADDRINFO_MAX            = 256;
    static const unsigned HOST_LOCK_COUNT    = 1024;
    static const unsigned DEFAULT_HOST_SPEED = 0;

    typedef HostChannel::HostKey HostKey;
    typedef ServChannel::ServKey ServKey;
    typedef boost::unordered_map<HostKey, HostChannel*>  HostMap;
    typedef boost::unordered_map<ServKey, ServChannel*>  ServMap;
    typedef boost::unordered_map<std::string, unsigned>  SpeedMap;
    typedef boost::unordered_map<std::string, BatchConfig*> BatchCfgMap;

    //批次配置查找表
    BatchCfgMap batch_cfg_map_;
    RwLock   batch_map_lock_;
    //hostchannel查找表
    HostMap  host_map_;
    mutable RwLock   host_map_lock_;
    //servchannel查找表
    ServMap  serv_map_;
    mutable RwLock   serv_map_lock_;
    //抓取速度查找表
    SpeedMap host_speed_map_;
    mutable RwLock   host_speed_lock_;

    size_t   host_cache_max_; 
    size_t   serv_cache_max_;
    bool     close_;
    ChannelManager* channel_manager_;

    int __aicmp(const struct addrinfo *ai1, const struct addrinfo *ai2);
    ServKey __aigetkey(const struct addrinfo *addrinfo, char scheme, sockaddr* local_addr);
    HostKey __hostgetkey(const std::string& host, 
        const std::string& scheme = "http", unsigned port = 80);
    void __destroy_resource(Resource* res);
    void __save_unfinish_resource(ResourceList res_lst);

public:
    ~Storage();
    
    void SetMaxCacheCount(unsigned host_cnt, unsigned serv_cnt)
    {
        host_cache_max_ = host_cnt;
        serv_cache_max_ = serv_cnt;
    }
    void Close() 
    {
        close_ = true;
    }

    void UpdateBatchConfig(std::string& batch_id, const BatchConfig& cfg);
    ServChannel* GetServChannel(ServKey serv_key) const;
    unsigned GetHostSpeed(const std::string& host) const;
    void SetHostSpeed(const std::string& host, unsigned fetch_interval_ms);
    void CheckCacheLimit();
    HostChannel* AcquireHostChannel(const URI& uri);

    BatchConfig* AcquireBatchCfg(const std::string& batch_id, 
        time_t timeout_sec          = BatchConfig::DEFAULT_TIMEOUT_SEC,
        unsigned max_retry_times    = BatchConfig::DEFAULT_MAX_RETRY_TIMES,
        unsigned max_redirect_times = BatchConfig::DEFAULT_MAX_REDIRECT_TIMES,
        unsigned max_body_size      = BatchConfig::DEFAULT_MAX_BODY_SIZE,
        unsigned truncate_size      = BatchConfig::DEFAULT_TRUNCATE_SIZE,
        ResourcePriority res_prior  = BatchConfig::DEFAULT_RES_PRIOR,
        const char* user_agent      = BatchConfig::DEFAULT_USER_AGENT,
        const char* accept_encoding = BatchConfig::DEFAULT_ACCEPT_ENCODING,
        const char* accept_language = BatchConfig::DEFAULT_ACCEPT_LANGUAGE,
        const char* accept          = BatchConfig::DEFAULT_ACCEPT
        );

    ServChannel* AcquireServChannel(
        struct addrinfo* ai,
        char   scheme,
        struct sockaddr* local_addr = NULL,
        ServChannel::ConcurencyMode concurency_mode = ServChannel::DEFAULT_CONCURENCY_MODE, 
        double max_err_rate = ServChannel::DEFAULT_MAX_ERR_RATE );

    Resource* CreateResource(
            const   std::string& url,
            void*  contex,
            BatchConfig* batch_cfg, 
            ResourcePriority prior,
            MessageHeaders* user_headers,
            Resource* root_res);
    
    void DestroyResource(Resource* res);
};

#endif
