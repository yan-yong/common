#include "ServChannel.hpp"
#include "utility/murmur_hash.h"

/** 静态成员初始化 **/
HostCacheList*   HostChannel::cache_lst_ = NULL;
SpinLock*        HostChannel::cache_lock_= NULL;
unsigned         HostChannel::cache_cnt_ = 0;
ServCacheList*   ServChannel::cache_lst_ = NULL;
SpinLock*        ServChannel::cache_lock_= NULL;
unsigned         ServChannel::cache_cnt_ = 0;

ServChannel::ServChannel(
    struct addrinfo* ai, ServKey serv_key, unsigned max_err_rate, 
    unsigned concurency_mode, struct sockaddr* local_addr):
    fetch_time_ms_(0), is_foreign_(false), 
    concurency_mode_(concurency_mode), 
    conn_using_cnt_(0), fetch_interval_ms_(0), 
    max_err_rate_(max_err_rate), serv_key_(serv_key)
{
    if(local_addr)
    {
        //FIXME: cannot bind on ipv6 address
        local_addr_ = (struct sockaddr*)malloc(sizeof(struct sockaddr));
        memcpy(local_addr_, local_addr, sizeof(struct sockaddr));
    }
    else
        local_addr_ = NULL;
    struct addrinfo * cur_ai = ai;
    while(!cur_ai)
    {
        Connection* conn = __create_connection(__copy_addrinfo(cur_ai));
        if(!conn)
            break;
        conn_storage_.push(conn);
        cur_ai = cur_ai->ai_next;
    }
}

namespace Channel
{
static void __update_serv_host_state(HostChannel* host_channel)
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
//resource出队, 不会修改ref_cnt_
static Resource* __pop_resource(HostChannel* host_channel)
{
    if(host_channel->res_lst_map_.empty())
        return NULL;
    Resource* p_res = NULL;
    ResourcePriority prior = RES_PRIORITY_LEVEL_5;
    host_channel->res_lst_map_.get_front(prior, p_res);
    host_channel->res_lst_map_.pop_front();
    if(!host_channel->serv_)
        return p_res;
    //检查host状态变迁 && host轮转
    __update_serv_host_state(host_channel);
    return p_res;
}
inline bool __empty(HostChannel* host_channel)
{
    return host_channel->ref_cnt_ == 0;
}
inline bool __empty(ServChannel* serv_channel)
{
    return serv_channel->fetching_lst_.empty() && 
        serv_channel->wait_host_lst_.empty();
}
inline unsigned __wait_res_cnt(HostChannel* host_channel)
{
    return host_channel->res_lst_map_.size();
}
static unsigned __wait_res_cnt(ServChannel* serv_channel)
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

Resource* __pop_resource(HostChannel* host_channel)
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

void __release_connection(ServChannel* serv_channel, Connection* conn)
{
    if(conn)
    {
        serv_channel->conn_storage_.push_back(conn);
        serv_channel->conn_using_cnt_ -= 1;
    }
}

struct addrinfo* __copy_addrinfo(struct addrinfo* addr)
{
    if(!addr)
        return NULL;
    unsigned sz = sizeof(struct addrinfo);
    if(ai->ai_family == AF_INET)
        sz += sizeof(struct sockaddr_in);
    else
        sz += sizeof(struct sockaddr_in6);
    struct addrinfo* cur_ai_copy = (struct addrinfo*)malloc(sz);
    memset(cur_ai_copy, 0, sz);
    cur_ai_copy->ai_addr = (struct sockaddr *)(cur_ai_copy + 1);
    memcpy(cur_ai_copy, addr, sz);
    return cur_ai_copy;
}

Connection* __create_connection(struct sockaddr* dst_addr)
{
    Connection* conn = (Connection*)malloc(sizeof(Connection)); 
    assert(conn); 
    INIT_LIST_HEAD(&conn->list);
    conn->state = CS_CLOSED;
    conn->error = 0;
    conn->scheme = HTTP_SCHEME;
    conn->sockfd = -1;
    conn->message = NULL;
    conn->socket_family = AF_INET;
    conn->socket_type = SOCK_STREAM;
    conn->protocol = 0;
    conn->address.remote_addr = dst_addr;
    conn->address.remote_addrlen = sizeof(struct sockaddr);
    conn->address.local_addr = local_addr_;
    if(!local_addr_)
        conn->address.local_addrlen = 0;
    else
        conn->address.local_addrlen = sizeof(struct sockaddr);
    conn->ssl = NULL;
    conn->user_data = NULL;
    return conn;
}

inline SpinLock& __serv_lock(ServChannel * serv_channel)
{
    return serv_channel ? serv_channel->lock_:*((SpinLock*)NULL);
}

Connection* __acquire_connection(ServChannel* serv_channel)
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
                Connection* new_conn = __create_connection(__copy_addrinfo(conn->address.remote_addr));
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

/******************  cache operation ****************/
inline void check_add_cache(ServChannel* serv_channel)
{
    if(__empty(serv_channel) && serv_channel->cache_node_.empty())
    {
        SpinGuard guard(*ServChannel::cache_lock_);
        ServChannel::cache_lst_->add_back(*serv_channel);
        ServChannel::cache_cnt_++;
    }
}
inline void check_add_cache(HostChannel* host_channel)
{
    if(__empty(host_channel) && host_channel->cache_node_.empty())
    {
        SpinGuard guard(*HostChannel::cache_lock_);
        HostChannel::cache_lst_->add_back(*host_channel);
        HostChannel::cache_cnt_++;
    }
    if(host_channel->serv_)
        check_add_cache(host_channel->serv_);
}
inline void check_remove_cache(ServChannel* serv_channel)
{
    if(!__empty(serv_channel) && !serv_channel->cache_node_.empty())
    {
        SpinGuard guard(*ServChannel::cache_lock_);
        ServCacheList::del(*serv_channel);
        ServChannel::cache_cnt_--;
    }
}
inline void check_remove_cache(HostChannel* host_channel)
{
    if(!__empty(host_channel) && !host_channel->cache_node_.empty())
    {
        SpinGuard guard(*HostChannel::cache_lock_);
        HostCacheList::del(*host_channel);
        HostChannel::cache_cnt_--;
    }
    if(host_channel->serv_)
        check_remove_cache(host_channel->serv_);
}

unsigned WaitResCnt(ServChannel* serv_channel)
{
    SpinGuard guard(serv_channel->lock_);
    return __wait_res_cnt(serv_channel);
}

unsigned WaitResCnt(HostChannel* host_channel)
{
    SpinGuard guard(host_channel->lock_);
    return __wait_res_cnt(host_channel);
}

void SetServChannel(HostChannel* host_channel, ServChannel * serv_channel)
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

void Destroy(HostChannel* host_channel)
{
    SpinGuard serv_guard(__serv_lock(host_channel->serv_));
    SpinGuard host_guard(host_channel->lock_);
    HostChannelList::del(*host_channel);
    if(!host_channel->cache_node_.empty())
    {
        SpinGuard cache_guard(*HostChannel::cache_lock_);
        HostCacheList::del(*host_channel);
    }
    host_channel->serv_ = NULL;
    delete host_channel;
}

void Destroy(ServChannel* serv_channel)
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
        SpinGuard cache_guard(*ServChannel::cache_lock_);
        ServCacheList::del(*serv_channel);
    }
    delete serv_channel;
}

void AddResource(HostChannel* host_channel, Resource* res)
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

unsigned GetHostCacheSize(unsigned cnt)
{
    return HostChannel::cache_cnt_;
}

unsigned GetServCacheSize(unsigned cnt)
{
    return ServChannel::cache_cnt_;
}

HostCacheList PopHostCache(unsigned cnt)
{
    HostCacheList cache_sub_lst;
    SpinGuard guard(*HostChannel::cache_lock_);
    for(unsigned i = 0; i < cnt && !HostChannel::cache_lst_->empty(); i++)
    { 
        cache_sub_lst.add_back(*cache_lst_->get_front());
        cache_lst_->pop_front();
    }
    return cache_sub_lst;
}

ServCacheList PopServCache(unsigned cnt)
{
    ServCacheList cache_sub_lst;
    SpinGuard guard(*ServChannel::cache_lock_);
    for(unsigned i = 0; i < cnt && !ServChannel::cache_lst_->empty(); i++)
    { 
        cache_sub_lst.add_back(*cache_lst_->get_front());
        cache_lst_->pop_front();
    }
    return cache_sub_lst;
}

void SetFetchIntervalMs(HostChannel* host_channel, unsigned fetch_interval_ms)
{
    ServChannel* serv_channel = __serv_lock(host_channel->serv_); 
    SpinGuard serv_guard(serv_channel->lock_);
    SpinGuard host_guard(host_channel->lock_);
    host_channel->fetch_interval_ms_ = fetch_interval_ms;
    if(fetch_interval_ms && fetch_interval_ms < serv_channel->fetch_interval_ms_)
        serv_channel->fetch_interval_ms_ = fetch_interval_ms;
}

void ReleaseConnection(Resource* res)
{
    SpinGuard serv_guard(__serv_lock(res->serv_));
    if(res->serv_ && res->conn_)
    {
        __release_connection(res->serv_, res->conn_);
        res->conn_ = NULL;
    }
}

void RemoveResource(Resource* res)
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
            res->conn_ = NULL；
        }
        if(__empty(res->serv_))
            check_add_cache(res->serv_);
        res->serv_ = NULL;
    }
    res->host_ = NULL;
    if(res->host_->ref_cnt == 0)
        check_add_cache(res->host_); 
}

Resource* PopResource(HostChannel* host_channel)
{
    SpinGuard serv_guard(__serv_lock(host_channel->serv_));
    SpinGuard host_guard(host_channel->lock_);
    return __pop_resource(host_channel);
}

Resource* PopResource(ServChannel* serv_channel)
{
    SpinGuard serv_guard(serv_channel->lock_);
    if(serv_channel->wait_host_lst_.empty())
        return NULL;
    HostChannel* host_channel = 
        serv_channel->wait_host_lst_.get_front();
    SpinGuard host_guard(host_channel->lock_);
    return host_channel->__pop_resource();
}

Resource* PopAvailableResource(ServChannel* serv_channel)
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

std::string ToString(HostChannel* host_channel) const
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

bool ConnectionAvailable(ServChannel* serv_channel)
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

bool InitializeCache()
{
    ServChannel::cache_lst_ =  new ServCacheList();
    ServChannel::cache_lock_=  new SpinLock();
    ServChannel::cache_cnt_ = 0;
    HostChannel::cache_lst_ =  new HostCacheList();
    HostChannel::cache_lock_=  new SpinLock();
    HostChannel::cache_cnt_ = 0;
}

bool DestroyCache()
{
    delete ServChannel::cache_lst_;
    ServChannel::cache_lst_ = NULL;
    delete ServChannel::cache_lock_;
    ServChannel::cache_lock_= NULL;
    delete HostChannel::cache_lst_;
    HostChannel::cache_lst_ = NULL;
    delete HostChannel::cache_lock_;
    HostChannel::cache_lock_ = NULL;
}

}
