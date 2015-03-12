#ifndef __STORAGE_HPP
#define __STORAGE_HPP

#include <boost/unordered_map>
#include "singleton/Singleton.h"
#include "Channel.h"
#include "lock/lock.hpp"
#include "utility/murmur_hash.h"
#include "linklist/linked_list.hpp"

/*
* Storage负责：
* 1. Resource的创建和销毁;
* 2. HostChannel, ServChannel的创建和销毁
* 3. BatchConfig的创建和销毁
 */

class Storage 
{
    static const unsigned MAX_CACHE_HOST     100000
    static const unsigned MAX_CACHE_SERV     100000
    static const double   EXCEED_DELETE_RATE 0.1
    static const unsigned ADDRINFO_MAX       256
    static const unsigned HOST_LOCK_COUNT    1024
    static const unsigned DEFAULT_HOST_SPEED 0
    
    DECLARE_SINGLETON(Storage);

    typedef HostChannel::HostKey HostKey;
    typedef ServChannel::ServKey ServKey;
    typedef boost::unordered_map<HostKey, HostChannel>  HostMap;
    typedef boost::unordered_map<ServKey, ServChannel>  ServMap;
    typedef boost::unordered_map<std::string, unsigned> SpeedMap;
    typedef boost::unordered_map<std::string, BatchConfig*> BatchCfgMap;

    //批次配置查找表
    BatchCfgMap batch_cfg_map_;
    RwLock   batch_map_lock_;
    //hostchannel查找表
    HostMap  host_map_;
    RwLock   host_map_lock_;
    //servchannel查找表
    ServMap  serv_map_;
    RwLock   serv_map_lock_;
    //抓取速度查找表
    SpeedMap host_speed_map_;
    RwLock   host_speed_lock_;

    size_t   host_cache_max_; 
    size_t   serv_cache_max_;
    bool     close_;

    ServKey __aigetkey(const struct addrinfo *addrinfo, socekaddr* local_addr) 
    {
        const struct addrinfo *sorted[ADDRINFO_MAX];
        const struct addrinfo *p;
        MD5_CTX ctx; 
        unsigned char digest[16];
        int i, j;

        for (p = addrinfo, i = 0; p && i < ADDRINFO_MAX; p = p->ai_next, i++) 
        {    
            for (j = i; j > 0; j--) 
            {    
                if (__aicmp(p, sorted[j - 1]) < 0) 
                    sorted[j] = sorted[j - 1];
                else 
                    break;
            }    
            sorted[j] = p; 
        }    
        MD5_Init(&ctx);
        for (j = 0; j < i; j++) 
        {    
            MD5_Update(&ctx, sorted[j]->ai_addr,
                    sorted[j]->ai_addrlen);
        }
        if(local_addr)
            MD5_Update(&ctx, local_addr, sizeof(struct sockaddr)); 
        MD5_Final(digest, &ctx);
        return *(ServKey *)digest;
    }

    HostKey __hostgetkey(const std::string& host, 
        const std::string& scheme = "http", unsigned port = 80)
    {
        HostKey host_key = 0;
        char buf[1024] = {0};
        int len = snprintf(buf, 1024, "%s://%s:%u", 
            scheme.c_str(), host.c_str(), port);
        MurmurHash_x64_64(buf, len, &host_key);
        return host_key; 
    }

    void CheckCacheLimit()
    {
        //check host cache limit
        unsigned serv_cache_cnt = GetServCacheSize();
        if(serv_cache_cnt > serv_cache_max_)
        {
            WriteGuard guard(host_map_lock_);
            unsigned delete_cnt = EXCEED_DELETE_RATE * serv_cache_cnt;
            if(!delete_cnt)
                delete_cnt = 1;
            HostCacheList cache_lst = PopHostCache(delete_cnt);
            HostChannel * cur_host = NULL;
            while(!cache_lst.empty())
            {
                cur_host = cache_lst.get_front();
                cache_lst.pop_front();
                host_map_.erase(cur_host->GetHostKey());
                Destroy(cur_host);
            }
        }
        //check serv cache limit
        unsigned serv_cache_cnt = GetServCacheSize();
        if(serv_cache_cnt > serv_cache_max_)
        {
            WriteGuard guard(serv_map_lock_);
            unsigned delete_cnt = EXCEED_DELETE_RATE * serv_cache_cnt;
            if(!delete_cnt)
                delete_cnt = 1;
            ServCacheList cache_lst = PopServCache(delete_cnt);
            ServChannel *cur_serv = NULL;
            while(!cache_lst.empty())
            {
                cur_serv = cache_lst.get_front();
                cache_lst.pop_front();
                serv_map_.erase(cur_serv->GetServKey());
                Destroy(cur_serv);
            }
        }
    } 

public:
    Storage(size_t host_cache_max = MAX_CACHE_HOST, size_t serv_cache_max = MAX_CACHE_SERV):
        host_cache_max_(host_cache_max), serv_cache_max_(serv_cache_max), close_(false)
    {
        InitializeCache();
    }

    ~Storage()
    {
        WriteGuard guard(serv_map_lock_);
        for(ServMap::iterator it = serv_map_lock_.begin(); 
            it != serv_map_lock_.end(); )
        {
            ServChannel* serv_channel = it->second;
            while(WaitResCnt(serv_channel))
            {
                Resource * res = PopResource(serv_channel);
                FreeResource(res);
            }
            serv_map_lock_.erase(it++);
            delete serv_channel;
        }
 
        WriteGuard guard(host_map_lock_);
        for(HostMap::iterator it = host_map_lock_.begin(); 
            it != host_map_lock_.end(); )
        {
            HostChannel * host_channel = it->first;
            host_map_lock_.erase(it++);
            delete host_channel;
        }
        DestroyCache(); 
    }

    void Close() 
    {
        close_ = true;
    }

    BatchCfg* AcquireBatchCfg(const std::string& batch_id, 
        time_t timeout_sec          = BatchConfig::DEFAULT_TIMEOUT_SEC,
        unsigned max_retry_times    = BatchConfig::DEFAULT_MAX_RETRY_TIMES,
        unsigned max_redirect_times = BatchConfig::DEFAULT_MAX_REDIRECT_TIMES,
        unsigned max_body_size      = BatchConfig::DEFAULT_MAX_BODY_SIZE,
        unsigned truncate_size      = BatchConfig::DEFAULT_TRUNCATE_SIZE,
        ResourcePriority res_prior  = BatchConfig::DEFAULT_RES_PRIOR,
        const char* user_agent      = BatchConfig::DEFAULT_USER_AGENT,
        const char* accept_encoding = BatchConfig::DEFAULT_ACCEPT_ENCODING,
        const char* accept          = BatchConfig::DEFAULT_ACCEPT
        )
    {
        if(close_)
            return NULL;
        {
            ReadGuard guard(batch_map_lock_);
            BatchCfgMap::iterator batch_it = batch_cfg_map_.find(batch_id);
            if(batch_it != batch_cfg_map_.end())
                return batch_it->second;
        }
        WriteGuard guard(batch_map_lock_);
        BatchConfig* batch_cfg = new BatchConfig(batch_id, timeout_sec, 
            max_retry_times, max_redirect_times, max_body_size, 
            truncate_size, res_prior, user_agent, accept_encoding, 
            accept_language, accept);
        batch_cfg_map_.insert(BatchCfgMap::value_type(batch_id, cur_cfg));
        return batch_cfg;
    }

    void HttpClient::UpdateBatchConfig(std::string& batch_id, const BatchConfig& cfg)
    {
        WriteGuard guard(batch_map_lock_);
        BatchConfig * cur_cfg = NULL;
        BatchCfgMap::iterator it = batch_cfg_map_.find(batch_id);
        if(it == batch_cfg_.end())
        {
            cur_cfg = new BatchConfig;
            batch_cfg_.insert(BatchCfgMap::value_type(batch_id, cur_cfg));
        }
        else
            cur_cfg = it->second; 
        memcpy(cur_cfg, &cfg, sizeof(cfg));
    }

    ServChannel* AcquireServChannel(
        struct addrinfo* ai, 
        struct sockaddr* local_addr = NULL,
        ServChannel::ConcurencyMode concurency_mode = ServChannel::DEFAULT_CONCURENCY_MODE, 
        unsigned max_err_rate = ServChannel::DEFAULT_MAX_ERR_RATE )
    {
        ServKey serv_key = __aigetkey(ai, local_addr); 
        {
            ReadGuard guard(serv_map_lock_);
            ServMap::iterator it = serv_map_.find(serv_key); 
            if(it != serv_map_.end())
                return it->second;
        }

        WriteGuard guard(serv_map_lock_);
        ServChannel* serv_channel = new ServChannel(ai, serv_key, 
            max_err_rate, concurency_mode, local_addr);
        serv_map_.insert(ServChannel::value_type(serv_key, serv_channel));
        return serv_channel;
    }

    ServChannel* GetServChannel(ServKey serv_key) const
    {
        ReadGuard guard(serv_map_lock_);
        ServMap::iterator it = serv_map_.find(serv_key);
        if(it != serv_map_.end())
            return it->second;
        return NULL; 
    }

    HostChannel* AcquireHostChannel(const URI& uri)
    {
        const std::string& host = uri.GetHost();
        const std::string& scheme = uri.GetScheme();
        assert(scheme == "http" || scheme == "https");
        char protocal = str2protocal(scheme);
        uint16_t port = GetHttpDefaultPort(protocal);
        if(uri.HasPort())
            port = (uint16_t)atoi(uri.GetPort().c_str());
        HostKey host_key = __hostgetkey(host, scheme, port);

        {
            ReadGuard guard(host_map_lock_);
            HostMap::iterator it = host_map_.find(host_key);
            if(it != host_map_.end())
                return it->second;
        }
        unsigned fetch_interval = GetHostSpeed(host);
        WriteGuard guard(host_map_lock_);
        HostChannel* host_channel = new HostChannel(protocal, host, port, 
            host_key, fetch_interval);
        host_map_[host_key] = host_channel;
        return host_channel;
    }

    unsigned GetHostSpeed(const std::string& host) const
    {
        ReadGuard guard(host_speed_lock_);
        SpeedMap::const_iterator it = host_speed_map_.find(host);
        if(it == host_speed_map_.end())
            return 0;
        return it->second; 
    } 

    void SetHostSpeed(const std::string& host, unsigned fetch_interval_ms)
    {
        {
            WriteGuard guard(host_speed_lock_);
            host_speed_map_[host] = fetch_interval_ms;
        }
        //FIXME: calculate host_key only by host && default port && default scheme
        HostKey host_key = __hostgetkey(host);
        HostMap::iterator it = host_map_.find(host_key);
        if(it != host_map_.end())
            it->second->SetFetchIntervalMs(fetch_interval_ms); 
    }

    Resource* CreateResource(
            const   std::string& url,
            void*  contex,            
            std::string batch_id,
            ResourcePriority prior,
            MessageHeaders* user_headers,
            Resource* root_res)
    {
        if(close_)
            return NULL;
        BatchCfg* batch_cfg = AcquireBatchCfg(batch_id);
        URI uri;
        if(UriParse(url.c_str(), url.length(), uri) 
                && HttpUriNormalize(uri))
        {
            size_t res_size = sizeof(Resource);
            if(root_res)
                res_size += sizeof(ResExtend);
            Resource* res = (Resource*)malloc(res_size);
            HostChannel* host_channel = AcquireHostChannel(uri);
            std::string suffix = uri.Path();
            if(uri.HasQuery())
                suffix += "?" + uri.Query();
            res->Initialize(host_channel, suffix, prior, contex, 
                user_headers, root_res, batch_cfg);
            AddResource(host_channel, res);
            return res;
        }
        return NULL; 
    }

    void DestroyResource(Resource* res)
    {
        HostChannel* host_channel = res->host_;
        Resource* root_res = res->RootResource();
        //如果资源被重定向资源所引用，则不能删除
        if(res->root_ref_ == 0)
        {
            Channel::RemoveResource(res);
            res->Destroy();
            free(res);
        }
        if(root_res->root_ref_ == 0)
        {
            Channel::RemoveResource(res);
            root_res->Destroy();
            free(root_res);
        }
        CheckCacheLimit();
    }
};

#endif
