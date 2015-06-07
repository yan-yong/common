#include "Storage.hpp"

DEFINE_SINGLETON(Storage);

int Storage::__aicmp(const struct addrinfo *ai1, const struct addrinfo *ai2) 
{
    if (ai1->ai_addrlen == ai2->ai_addrlen)
        return memcmp(ai1->ai_addr, ai2->ai_addr, ai1->ai_addrlen);
    else if (ai1->ai_addrlen < ai2->ai_addrlen)
        return -1;
    else 
        return 1;
}

Storage::ServKey Storage::__aigetkey(const struct addrinfo *addrinfo, char scheme, sockaddr* local_addr) 
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
    return *(ServKey *)digest + scheme;
}

Storage::HostKey Storage::__hostgetkey(const std::string& host, 
        const std::string& scheme, unsigned port)
{
    HostKey host_key = 0;
    char buf[1024] = {0};
    int len = snprintf(buf, 1024, "%s://%s:%u", 
            scheme.c_str(), host.c_str(), port);
    MurmurHash_x64_64(buf, len, &host_key);
    return host_key; 
}

void Storage::__destroy_resource(Resource* res)
{
    Resource* root_res = res->RootResource();
    //如果资源被重定向资源所引用，则不能删除
    if(res->root_ref_ == 0)
    {
        channel_manager_->RemoveResource(res);
        res->Destroy();
        free(res);
    }
    if(root_res != res && root_res->root_ref_ == 0)
    {
        channel_manager_->RemoveResource(root_res);
        root_res->Destroy();
        free(root_res);
    }
} 

void Storage::__save_unfinish_resource(ResourceListPtr res_lst)
{
    while(!res_lst->empty())
    {
        Resource * res = res_lst->get_front();
        //TODO: save unfinish
        free(res);
        res_lst->pop_front();
    }
}

Storage::Storage():
    host_cache_max_(MAX_CACHE_HOST), 
    serv_cache_max_(MAX_CACHE_SERV), close_(false)
{
    channel_manager_ = ChannelManager::Instance();
}

Storage::~Storage()
{
    //WriteGuard serv_guard(serv_map_lock_);
    for(ServMap::iterator it = serv_map_.begin(); 
            it != serv_map_.end(); )
    {
        ServChannel* serv_channel = it->second;
        __save_unfinish_resource(channel_manager_->RemoveUnfinishRes(serv_channel));
        //erase fetching resource
        serv_map_.erase(it++);
        channel_manager_->DestroyChannel(serv_channel);
    }

    //WriteGuard host_guard(host_map_lock_);
    for(HostMap::iterator it = host_map_.begin(); 
            it != host_map_.end(); )
    {
        HostChannel * host_channel = it->second;
        host_map_.erase(it++);
        channel_manager_->DestroyChannel(host_channel);
    }
}

BatchConfig* Storage::AcquireBatchCfg(const std::string& batch_id, 
    const BatchConfig& default_batch)
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
    BatchConfig* batch_cfg = new BatchConfig(default_batch);
    batch_cfg_map_.insert(BatchCfgMap::value_type(batch_id, batch_cfg));
    return batch_cfg;
}

void Storage::UpdateBatchConfig(std::string& batch_id, const BatchConfig& cfg)
{
    WriteGuard guard(batch_map_lock_);
    BatchConfig * cur_cfg = NULL;
    BatchCfgMap::iterator it = batch_cfg_map_.find(batch_id);
    if(it == batch_cfg_map_.end())
    {
        cur_cfg = new BatchConfig;
        batch_cfg_map_.insert(BatchCfgMap::value_type(batch_id, cur_cfg));
    }
    else
        cur_cfg = it->second; 
    memcpy(cur_cfg, &cfg, sizeof(cfg));
}

ServChannel* Storage::AcquireServChannel(
        char   scheme, struct addrinfo* ai, 
        ConcurencyMode concurency_mode, 
        double max_err_rate, unsigned max_err_count,
        unsigned err_delay_sec, struct sockaddr* local_addr )
{
    ServKey serv_key = __aigetkey(ai, scheme, local_addr); 
    ServMap::iterator it = serv_map_.find(serv_key); 
    if(it != serv_map_.end())
        return it->second;

    //WriteGuard guard(serv_map_lock_);
    ServChannel* serv_channel = channel_manager_->CreateServChannel(
        scheme, ai, serv_key, concurency_mode, max_err_rate, 
        max_err_count, err_delay_sec, local_addr);
    serv_map_.insert(ServMap::value_type(serv_key, serv_channel));
    return serv_channel;
}

ServChannel* Storage::GetServChannel(ServKey serv_key) const
{
    //ReadGuard guard(serv_map_lock_);
    ServMap::const_iterator it = serv_map_.find(serv_key);
    if(it != serv_map_.end())
        return it->second;
    return NULL; 
}

HostChannel* Storage::GetHostChannel(HostKey host_key) const
{
    //ReadGuard guard(host_map_lock_);
    HostMap::const_iterator it = host_map_.find(host_key);
    if(it != host_map_.end())
        return it->second;
    return NULL; 
}

HostChannel* Storage::AcquireHostChannel(const URI& uri)
{
    const std::string& host = uri.Host();
    const std::string& scheme_str = uri.Scheme();
    assert(scheme_str == "http" || scheme_str == "https");
    char scheme = str2protocal(scheme_str);
    uint16_t port = GetHttpDefaultPort(scheme);
    if(uri.HasPort())
        port = (uint16_t)atoi(uri.Port().c_str());
    HostKey host_key = __hostgetkey(host, scheme_str, port);

    {
        //ReadGuard guard(host_map_lock_);
        HostMap::iterator it = host_map_.find(host_key);
        if(it != host_map_.end())
            return it->second;
    }
    unsigned fetch_interval = GetHostSpeed(host);
    //WriteGuard guard(host_map_lock_);
    HostChannel* host_channel = channel_manager_->CreateHostChannel(
            scheme, host, port, host_key, fetch_interval);
    host_map_[host_key] = host_channel;
    return host_channel;
}

unsigned Storage::GetHostSpeed(const std::string& host) const
{
    ReadGuard guard(host_speed_lock_);
    SpeedMap::const_iterator it = host_speed_map_.find(host);
    if(it == host_speed_map_.end())
        return 0;
    return it->second; 
} 

void Storage::SetHostSpeed(const std::string& host, unsigned fetch_interval_ms)
{
    {
        WriteGuard guard(host_speed_lock_);
        host_speed_map_[host] = fetch_interval_ms;
    }
    //FIXME: calculate host_key only by host && default port && default scheme
    HostKey host_key = __hostgetkey(host);
    HostMap::iterator it = host_map_.find(host_key);
    if(it != host_map_.end())
        channel_manager_->SetFetchIntervalMs(it->second, fetch_interval_ms);
}

Resource* Storage::CreateResource(
        const  URI& uri,
        void*  contex,
        BatchConfig* batch_cfg,  
        ResourcePriority prior,
        const MessageHeaders* user_headers,
        const std::vector<char>* post_content,
        Resource* root_res, ServChannel* proxy_serv)
{
    if(close_)
        return NULL;
    size_t res_size = sizeof(Resource);
    if(root_res || user_headers || post_content)
        res_size += sizeof(ResExtend);
    Resource* res = (Resource*)malloc(res_size);
    HostChannel* host_channel = AcquireHostChannel(uri);
    std::string suffix = uri.Path();
    if(uri.HasQuery())
        suffix += "?" + uri.Query();
    res->Initialize(host_channel, suffix, prior, contex, 
            user_headers, post_content, root_res, batch_cfg);
    if(proxy_serv)
        res->SetProxyServ(proxy_serv);
    channel_manager_->AddResource(res);
    return res;
}

void Storage::CheckCacheLimit()
{
    //check host cache limit
    unsigned host_cache_cnt = channel_manager_->GetHostCacheSize();
    if(host_cache_max_ && host_cache_cnt > host_cache_max_)
    {
        //WriteGuard guard(host_map_lock_);
        unsigned delete_cnt = EXCEED_DELETE_RATE * host_cache_cnt;
        if(!delete_cnt)
            delete_cnt = 1;
        std::vector<HostChannel*> host_cache_vec = 
            channel_manager_->PopHostCache(delete_cnt);
        for(unsigned i = 0; i < host_cache_vec.size(); i++)
        {
            host_map_.erase(host_cache_vec[i]->GetHostKey());
            channel_manager_->DestroyChannel(host_cache_vec[i]);
        }
    }
    //check serv cache limit
    unsigned serv_cache_cnt = channel_manager_->GetServCacheSize();
    if(serv_cache_max_ && serv_cache_cnt > serv_cache_max_)
    {
        //WriteGuard guard(serv_map_lock_);
        unsigned delete_cnt = EXCEED_DELETE_RATE * serv_cache_cnt;
        if(!delete_cnt)
            delete_cnt = 1;
        std::vector<ServChannel*> serv_cache_vec = 
            channel_manager_->PopServCache(delete_cnt);
        for(unsigned i = 0; i < serv_cache_vec.size(); i++)
        {
            serv_map_.erase(serv_cache_vec[i]->GetServKey());
            channel_manager_->DestroyChannel(serv_cache_vec[i]);
        }
    }
}

void Storage::DestroyResource(Resource* res)
{
    __destroy_resource(res);
    CheckCacheLimit();
}

