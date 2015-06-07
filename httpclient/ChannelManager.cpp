#include "utility/net_utility.h"
#include "ChannelManager.hpp"
#include "fetcher/Fetcher.hpp"

DEFINE_SINGLETON(ChannelManager);

ChannelManager::ChannelManager()
{
    host_cache_cnt_ = 0;
    serv_cache_cnt_ = 0;
    min_ready_time_ = 0;
}

void ChannelManager::__update_serv_host_state(HostChannel* host_channel)
{
    ServChannel* serv = host_channel->serv_;
    if(serv)
    {
        HostChannelList::del(*host_channel);
        if(!(host_channel->res_wait_queue_.empty()))
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
    if(serv_channel->concurency_mode_ == CONCURENCY_NO_LIMIT)
    {
        serv_channel->conn_storage_.push_back(conn);
        return ThreadingFetcher::CreateConnection(conn);
    }
    return conn;
}

void ChannelManager::check_add_cache(ServChannel* serv_channel)
{
    if(serv_channel && __empty(serv_channel) && 
        (serv_channel->cache_node_).empty())
    {
        //SpinGuard guard(serv_cache_lock_);
        serv_cache_lst_.add_back(*serv_channel);
        serv_cache_cnt_++;
    }
}

void ChannelManager::check_add_cache(HostChannel* host_channel)
{
    if(__empty(host_channel) && host_channel->cache_node_.empty())
    {
        //SpinGuard guard(host_cache_lock_);
        host_cache_lst_.add_back(*host_channel);
        host_cache_cnt_++;
    }
}

void ChannelManager::check_remove_cache(ServChannel* serv_channel)
{
    if(serv_channel && !__empty(serv_channel) &&
        !serv_channel->cache_node_.empty())
    {
        //SpinGuard guard(serv_cache_lock_);
        ServCacheList::del(*serv_channel);
        serv_cache_cnt_--;
    }
}

void ChannelManager::check_remove_cache(HostChannel* host_channel)
{
    if(!__empty(host_channel) && !host_channel->cache_node_.empty())
    {
        //SpinGuard guard(host_cache_lock_);
        HostCacheList::del(*host_channel);
        host_cache_cnt_--;
    }
}

bool ChannelManager::WaitEmpty(ServChannel* serv_channel)
{
    //SpinGuard guard(serv_channel->lock_);
    return __wait_empty(serv_channel);
}

// HostChannel关联ServChannel的接口
void ChannelManager::SetServChannel(HostChannel* host_channel, ServChannel * serv_channel)
{
    ServChannel* last_serv = host_channel->serv_;
    //** 先处理原来的serv
    {
        //SpinGuard serv_guard(__serv_lock(last_serv));
        //SpinGuard host_guard(host_channel->lock_);
        if(last_serv && serv_channel->serv_key_ == last_serv->serv_key_)
            return;
        host_channel->dns_resolving_ = 0;
        HostChannelList::del(*host_channel);
        check_add_cache(last_serv);
        host_channel->serv_ = serv_channel;
    }
    //** 处理新的serv 
    {
        //SpinGuard serv_guard(__serv_lock(serv_channel));
        //SpinGuard host_guard(host_channel->lock_);
        host_channel->update_time_ = current_time_ms();
        if(!serv_channel)
        {
            host_channel->host_error_ = 1;
            host_channel->serv_       = NULL;
            return;
        }
        host_channel->host_error_ = 0;
        //更新抓取速度
        if(host_channel->fetch_interval_ms_ && 
                host_channel->fetch_interval_ms_ < serv_channel->fetch_interval_ms_)
        {
            serv_channel->fetch_interval_ms_ = host_channel->fetch_interval_ms_;
        }
        __update_serv_host_state(host_channel);
        check_remove_cache(serv_channel);
    }
    //** 检查serv是否ready
    check_serv_ready(serv_channel);
}

void ChannelManager::DestroyChannel(HostChannel* host_channel)
{
    //SpinGuard serv_guard(__serv_lock(host_channel->serv_));
    //SpinGuard host_guard(host_channel->lock_);
    assert(__empty(host_channel));
    if(!host_channel->cache_node_.empty())
    {
        //SpinGuard cache_guard(host_cache_lock_);
        HostCacheList::del(*host_channel);
    }
    HostChannelList::del(*host_channel);
    delete host_channel;
}

void ChannelManager::DestroyChannel(ServChannel* serv_channel)
{
    //SpinGuard guard(serv_channel->lock_);
    assert(__empty(serv_channel));
    serv_ready_lst_map_.del(*serv_channel);
    //remove from cache list
    ServCacheList::del(*serv_channel);
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
        //SpinGuard guard(host_channel->lock_);
        HostChannelList::del(*host_channel);
        host_channel->serv_ = NULL;
    }
    if(serv_channel->pres_wait_queue_)
        delete serv_channel->pres_wait_queue_;
    delete serv_channel;
}

ResourceListPtr ChannelManager::RemoveUnfinishRes(ServChannel* serv_channel)
{
    //SpinGuard guard(serv_channel->lock_);
    ResourceListPtr unfinish_lst(new ResourceList);
    HostChannel * host_channel = NULL;
    while(!serv_channel->wait_host_lst_.empty())
    {
        host_channel = serv_channel->wait_host_lst_.get_front();
        //SpinGuard guard(host_channel->lock_);
        ResourceListPtr wait_lst = (host_channel->res_wait_queue_).splice();
        unfinish_lst->splice_front(*wait_lst);
        HostChannelList::del(*host_channel);
        serv_channel->idle_host_lst_.add_back(*host_channel);
    }
    unfinish_lst->splice_front(serv_channel->fetching_lst_);
    if(serv_channel->pres_wait_queue_)
    {
        ResourceListPtr wait_lst = serv_channel->pres_wait_queue_->splice();
        unfinish_lst->splice_front(*wait_lst);
    }
    //Resource* res = unfinish_lst.get_front();
    //while(!res)
    //{
    //    ResTimedMap::del(*res);
    //    res = unfinish_lst.next(*res);
    //}
    return unfinish_lst;
}

ResourceListPtr ChannelManager::RemoveUnfinishRes(HostChannel* host_channel)
{
    //SpinGuard guard(host_channel->lock_);
    ResourceListPtr unfinish_lst = host_channel->res_wait_queue_.splice();
    __update_serv_host_state(host_channel);
    //Resource* res = unfinish_lst.get_front();
    //while(!res)
    //{
    //    ResTimedMap::del(*res);
    //    res = unfinish_lst.next(*res);
    //}
    return unfinish_lst;
}

//放入resource, 如果res->serv_不为空，则表示使用该固定的serv进行抓取
//否则使用host_channel的serv
void ChannelManager::AddResource(Resource* res)
{
    assert(res->queue_node_.empty());
    HostChannel* host_channel = res->host_;
    ServChannel* serv_channel = host_channel->serv_;
    if(res->serv_)
        serv_channel = res->serv_;
    //** 将res链入队列 
    {
        //SpinGuard serv_guard(__serv_lock(serv_channel));
        //SpinGuard host_guard(host_channel->lock_);
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
            //res->serv_ = host_channel->serv_;
            host_channel->res_wait_queue_.add_back(res->prior_, *res);
            if(host_wait_empty) 
                __update_serv_host_state(host_channel);
        }
        if(serv_channel)
            check_remove_cache(serv_channel);
    }
    //** 检查serv是否ready
    check_serv_ready(serv_channel);
}

//删除resource
void ChannelManager::RemoveResource(Resource* res)
{
    ServChannel * serv_channel = res->serv_;
    {
        //SpinGuard serv_guard(__serv_lock(serv_channel));
        ResourceList::del(*res);
        if(serv_channel)
        {
            if(res->conn_)
            {
                (serv_channel->conn_storage_).push_back(res->conn_);
                res->conn_ = NULL;
            }
            check_add_cache(serv_channel);
        }
        //SpinGuard host_guard(res->host_->lock_);
        --(res->host_->ref_cnt_);
        check_add_cache(res->host_); 
        res->host_ = NULL;
        res->serv_ = NULL;
    }
    check_serv_ready(serv_channel);
}

std::vector<HostChannel*> ChannelManager::PopHostCache(unsigned cnt)
{
    std::vector<HostChannel*> sub_vec;
    //SpinGuard guard(host_cache_lock_);
    for(unsigned i = 0; i < cnt && !host_cache_lst_.empty(); i++)
    {
        HostChannel * host_channel = host_cache_lst_.get_front(); 
        host_cache_lst_.pop_front();
        sub_vec.push_back(host_channel);
    }
    return sub_vec;
}

std::vector<ServChannel*> ChannelManager::PopServCache(unsigned cnt)
{
    std::vector<ServChannel*> sub_vec;
    //SpinGuard guard(serv_cache_lock_);
    for(unsigned i = 0; i < cnt && !serv_cache_lst_.empty(); i++)
    { 
        ServChannel * serv_channel = serv_cache_lst_.get_front();
        serv_cache_lst_.pop_front();
        sub_vec.push_back(serv_channel);
    }
    return sub_vec;
}

void ChannelManager::SetFetchIntervalMs(HostChannel* host_channel, 
    unsigned fetch_interval_ms)
{
    ServChannel * serv_channel = host_channel->serv_;
    //SpinGuard serv_guard(__serv_lock(serv_channel));
    //SpinGuard host_guard(host_channel->lock_);
    host_channel->fetch_interval_ms_ = fetch_interval_ms;
    if(fetch_interval_ms && fetch_interval_ms < serv_channel->fetch_interval_ms_)
        serv_channel->fetch_interval_ms_ = fetch_interval_ms;
}

bool ChannelManager::CheckResolveDns(HostChannel* host_channel, 
    time_t dns_update_time, time_t dns_error_time)
{
    //SpinGuard host_guard(host_channel->lock_);
    time_t cur_time   = current_time_ms();
    // dns错误, 但是还未超过错误的缓存时间, 这时不应该再去尝试解dns
    if(host_channel->host_error_ && cur_time < host_channel->update_time_ + dns_error_time)
        return false;
    // 正在解dns
    if(host_channel->dns_resolving_)
        return false;
    // 有dns, 但还不到更新的时候
    if(host_channel->serv_ && cur_time < host_channel->update_time_ + dns_update_time)
        return false;
    host_channel->dns_resolving_ = 1;
    host_channel->host_error_    = 0;
    return true;
}

void ChannelManager::ReleaseConnection(Resource* res)
{
    //SpinGuard serv_guard(__serv_lock(res->serv_));
    if(res->serv_ && res->conn_)
    {
        //reset back to HTTPS
        if(res->proxy_state_ == Resource::PROXY_CONNECT)
            ThreadingFetcher::SetConnectionScheme(res->conn_, PROTOCOL_HTTPS);
        if(res->serv_->concurency_mode_ != CONCURENCY_NO_LIMIT)
            (res->serv_->conn_storage_).push_back(res->conn_);
        else
            ThreadingFetcher::FreeConnection(res->conn_);
        res->conn_ = NULL;
    }
    check_serv_ready(res->serv_);
}

Resource* ChannelManager::pop_resource(ServChannel* serv_channel)
{
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
        //SpinGuard host_guard(host_channel->lock_);
        res = __pop_resource(host_channel);
    }
    return res;
}

void ChannelManager::pop_available_resources(
    ServChannel* serv_channel, std::vector<Resource*>& res_vec, 
    unsigned max_count)
{
    time_t cur_time = current_time_ms(); 
    //没有可抓的 或者 当前无连接可用
    while(!__wait_empty(serv_channel) 
        && serv_channel->conn_storage_.size() > 0
        && res_vec.size() < max_count)
    {
        time_t ready_time = serv_channel->GetReadyTime();
        if(ready_time >= cur_time)
        {
            serv_ready_lst_map_.add_back(ready_time, *serv_channel);
            if(min_ready_time_ > ready_time)
                min_ready_time_ = ready_time;
            break;
        }
        Connection* conn = __acquire_connection(serv_channel);
        Resource*    res = pop_resource(serv_channel);
        serv_channel->SetFetchTime(cur_time);
        res->conn_       = conn;
        // proxy connect时, 使用http协议
        if(res->proxy_state_ == Resource::PROXY_CONNECT)
            ThreadingFetcher::SetConnectionScheme(res->conn_, PROTOCOL_HTTP);
        res->serv_       = serv_channel;
        serv_channel->fetching_lst_.add_back(*res);
        res_vec.push_back(res);
    }
}

std::vector<Resource*> ChannelManager::PopAvailableResources(unsigned max_count)
{
    time_t cur_time = current_time_ms();
    std::vector<Resource*> res_vec;
    if(min_ready_time_ > cur_time)
        return res_vec;
    //SpinGuard ready_guard(serv_ready_lock_);
    while(!serv_ready_lst_map_.empty() && res_vec.size() < max_count)
    {
        time_t ready_time    = 0;
        ServChannel* serv_channel = NULL; 
        serv_ready_lst_map_.get_front(ready_time, serv_channel);
        if(ready_time > cur_time)
        {
            min_ready_time_ = ready_time;
            break;
        }
        //SpinGuard serv_guard(serv_channel->lock_);
        serv_ready_lst_map_.pop_front();
        pop_available_resources(serv_channel, res_vec, max_count);
    }
    return res_vec;
}

//注意： 这个函数不能被其它锁包围
void ChannelManager::check_serv_ready(ServChannel * serv_channel)
{
    if(serv_channel && !__wait_empty(serv_channel) && 
        serv_channel->conn_storage_.size() && 
        (serv_channel->queue_node_).empty() )
    {
        if(!__wait_empty(serv_channel) && 
          serv_channel->conn_storage_.size() && 
            (serv_channel->queue_node_).empty() )
        {
            //SpinGuard ready_guard(serv_ready_lock_);
            //SpinGuard serv_guard(serv_channel->lock_);
            time_t ready_time = serv_channel->GetReadyTime();
            serv_ready_lst_map_.add_back(ready_time, *serv_channel);
            if(ready_time < min_ready_time_)
                min_ready_time_ = ready_time;
        }
    }
}

std::string ChannelManager::ToString(HostChannel* host_channel) const
{
    char buf[2048];
    size_t sz = snprintf(buf, 2048, "%s://%s", protocal2str(host_channel->scheme_), (host_channel->host_).c_str());
    if(!IsHttpDefaultPort(host_channel->scheme_, 
                host_channel->port_))
    {
        snprintf(buf + sz, 2048 - sz, ":%hu", host_channel->port_);
    }
    return buf;
}

std::string ChannelManager::ToString(ServChannel* serv_channel) const
{
    Connection* conn = NULL;
    std::string str;
    if(serv_channel->conn_storage_.size() > 0)
        conn = *serv_channel->conn_storage_.begin();
    else
        conn = serv_channel->fetching_lst_.get_front()->conn_;
    if(!conn)
        return "0.0.0.0";
    sockaddr* addr = NULL;
    ThreadingFetcher::GetSockAddr(conn, addr);
    char addr_str[20];
    uint16_t port = 0;
    get_addr_string(addr, addr_str, 10, port);
    size_t addr_len = strlen(addr_str);
    snprintf(addr_str + addr_len, 20 - addr_len, ":%hu", port);
    return addr_str;
}

ServChannel* ChannelManager::CreateServChannel(
    char scheme, struct addrinfo* ai, 
    ServChannel::ServKey serv_key, 
    ConcurencyMode concurency_mode,
    unsigned max_err_rate, unsigned max_err_count,
    unsigned err_delay_sec, struct sockaddr* local_addr)
{
    ServChannel * serv     = new ServChannel();
    serv->concurency_mode_ = concurency_mode;
    serv->serv_key_        = serv_key;
    serv->max_err_rate_  = max_err_rate;
    serv->max_err_count_ = max_err_count;
    serv->err_delay_sec_ = err_delay_sec;
    struct addrinfo * cur_ai = ai;
    while(cur_ai)
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

bool ChannelManager::HasAvailableResource()
{
    return min_ready_time_ <= current_time_ms(); 
}
