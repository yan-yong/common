#ifndef __SERV_CHANNEL_HPP
#define __SERV_CHANNEL_HPP

#include "SchedulerTypes.hpp"
#include "linklist/linked_list_map.hpp"
#include "TUtility.hpp"

/**
    由于HostChannel与ServChannel相互引用，
    确保加锁顺序：先加ServChannel锁，再加HostChannel锁
**/

class ServChannel; 
class HostChannel
{
public:
    typedef linked_list_map<ResourcePriority, Resource, &Resource::queue_node_> ResWaitMap;
    typedef long long HostKey;

protected:
    char protocal_; 
    unsigned short port_;
    std::string host_;
    HostKey     host_key_;
    ServChannel *serv_;
    //serv的host状态队列链接指针
    linked_list_node_t queue_node_;
    linked_list_node_t cache_node_;
     //等待的Resource列表
    ResWaitMap  res_lst_map_;
    unsigned    fetch_interval_ms_;
    //该Host的引用数目
    unsigned    ref_cnt_;
    //dns更新时间
    time_t      update_time_;
    SpinLock    lock_;

    /** static member **/
    static HostCacheList * cache_lst_;
    static SpinLock * cache_lock_; 
    static unsigned cache_cnt_;

public:
    HostChannel(char protocal, const std::string& host, unsigned port, 
    HostKey host_key, unsigned fetch_interval_ms): 
        protocal_(protocal), port_(port), host_(host), host_key_(host_key),
        serv_(NULL), fetch_interval_ms_(fetch_interval_ms), 
        ref_cnt_(0), update_time_(0)
    {}
};

typedef linked_list<HostChannel, &HostChannel::queue_node_> HostChannelList;
typedef linked_list<HostChannel, &HostChannel::cache_node_> HostCacheList;

class ServChannelList;
class ServChannel
{
public:
    //正在抓取的resource
    typedef linked_list<Resource, &Resource::queue_node_> ResFetchingList; 
    typedef long long ServKey;
    enum ConcurencyMode
    {
        //该serv抓取时，不允许并发，一个抓完才抓下一个
        NO_CONCURENCY,
        //如果该serv有多个ip时，允许对不同ip间并发抓取
        CONCURENCY_PER_SERV, 
        //对并发没有限制
        CONCURENCY_NO_LIMIT
    };
    enum State
    {
        SERVER_IDLE,
        SERVER_WAITING,
        SERVER_CACHE  
    };

    static const double DEFAULT_MAX_ERR_RATE = 0.8;
    static const ConcurencyMode DEFAULT_CONCURENCY_MODE = CONCURENCY_PER_SERV;

protected:
    //统计错误率
    StasticCount<double, 10> err_rate_;
    //记录最近的抓取时间
    time_t   fetch_time_ms_;
    //统计对端服务器答复的快慢
    StasticCount<double, 10> resp_time_;
    //该serv的国内外属性
    bool     is_foreign_;
    //并发模式
    ConcurencyMode concurency_mode_;
    //连接池
    std::queue<Connection*> conn_storage_;
    //指定本地网卡
    sockaddr* local_addr_;
    //当前使用的连接数目
    unsigned conn_using_cnt_;
    //流控队列的链接指针
    linked_list_node_t queue_node_;
    //cache队列的链接指针
    linked_list_node_t cache_node_;
    //抓取间隔时间 
    unsigned fetch_interval_ms_;
    //该host允许的最大错误率 
    double max_err_rate_;
    //正在抓取的resource列表
    ResFetchingList fetching_lst_;
    ServKey serv_key_;
    SpinLock    lock_;
    //待抓取的host列表
    HostChannelList wait_host_lst_;
    //空闲的host列表
    HostChannelList idle_host_lst_;

    /** static member **/
    static ServCacheList * cache_lst_;
    static SpinLock cache_lock_; 
    static unsigned cache_cnt_;

public:
    //fetch_interval_ms为抓取的间隔时间, 单位为毫秒
    ServChannel(struct addrinfo* ai, ServKey serv_key,
        unsigned max_err_rate, unsigned concurency_mode, 
        struct sockaddr* local_addr);

    time_t GetReadyTime() const
    {
        return fetch_time_ms_ + fetch_interval_ms_;
    }
    void SetFetchTime(time_t cur_time)
    {
        fetch_time_ms_ = cur_time;
    }
    void AddSucc()
    {
        err_rate_.Add(0.0);
    }
    void AddFail()
    {
        err_rate_.Add(1.0);
    }
    void AddRespTime(time_t resp_time)
    {
        resp_time_.Add(resp_time); 
    }
    void SetForeign(bool is_foreign)
    {
        is_foreign_ = is_foreign;
    }
    double GetAvgRespTime() const
    {
        return resp_time_.Average();
    }
    double GetSuccRate() const
    {
        return 1.0 - err_rate_.Average(); 
    }
    double GetFetchIntervalMs() const
    {
        return fetch_interval_ms_; 
    }
    bool IsForeign() const
    {
        return is_foreign_;
    }
    bool IsServErr() const
    {
        return err_rate_.Average() > max_err_rate_;
    } 
    ServKey GetServKey() const
    {
        return serv_key_;
    }
};
typedef linked_list<ServChannel, &ServChannel::queue_node_> ServChannelList;
typedef linked_list<ServChannel, &ServChannel::cache_node_> ServCacheList;

/*** function **/
namespace Channel
{
    unsigned WaitResCnt(ServChannel* serv_channel);
    unsigned WaitResCnt(HostChannel* host_channel);
    ServChannel* SetServChannel(HostChannel*, ServChannel *);
    void DestroyChannel(HostChannel* host_channel);
    void DestroyChannel(ServChannel* serv_channel);
    void AddResource(HostChannel* host_channel, Resource* res);
    inline unsigned GetHostCacheSize(unsigned cnt);
    inline unsigned GetServCacheSize(unsigned cnt);
    HostCacheList PopHostCache(unsigned cnt);
    ServCacheList PopServCache(unsigned cnt);
    void SetFetchIntervalMs(HostChannel*, unsigned);
    void RemoveResource(HostChannel*, Resource*);
    Resource* PopResource(HostChannel* host_channel);
    Resource* PopResource(ServChannel* serv_channel);
    Resource* PopAvailableResource(ServChannel* serv_channel);
    std::string ToString(HostChannel* host_channel) const;
    bool ConnectionAvailable(ServChannel* serv_channel);
    bool InitializeCache();
    bool DestroyCache();
}
#endif
