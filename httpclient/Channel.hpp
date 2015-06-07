#ifndef __CHANNEL_HPP
#define __CHANNEL_HPP

#include "linklist/linked_list.hpp"
#include "linklist/linked_list_map.hpp"
#include "SchedulerTypes.hpp"
#include "httpparser/TUtility.hpp"
#include "lock/lock.hpp"
#include "httpparser/HttpFetchProtocal.hpp"
#include "Resource.hpp"
#include "utility/stastic_count.h"

/**
    由于HostChannel与ServChannel相互引用，
    确保加锁顺序：先加ServChannel锁，再加HostChannel锁
**/
typedef linked_list_map<ResourcePriority, Resource, &Resource::queue_node_> ResPriorQueue;
class ServChannel;

struct HostChannel
{
    typedef long long   HostKey;
    static const unsigned DEFAULT_DNS_UPDATE_TIME  = 86400;
    static const unsigned DEFAULT_DNS_ERROR_TIME   = 3600;

    unsigned char scheme_;
    unsigned char dns_resolving_:1;
    unsigned char host_error_:1;
    uint16_t port_;
    std::string host_;
    HostKey     host_key_;
    ServChannel *serv_;
    //排队链接指针
    linked_list_node_t queue_node_;
    linked_list_node_t cache_node_;
     //等待的Resource列表
    ResPriorQueue res_wait_queue_;
    unsigned      fetch_interval_ms_;
    //该Host的引用数目
    unsigned    ref_cnt_;
    //dns更新时间
    time_t      update_time_;
    SpinLock    lock_;

    HostChannel(): 
        scheme_(PROTOCOL_HTTP), dns_resolving_(0),
        host_error_(0), port_(80), host_key_(0), 
        serv_(NULL), fetch_interval_ms_(0), 
        ref_cnt_(0), update_time_(0)
    {}
    HostKey GetHostKey() const
    {
        return host_key_;
    }
};

typedef linked_list_t<HostChannel, &HostChannel::queue_node_> HostChannelList;
typedef linked_list_t<Resource, &Resource::queue_node_> ResourceList; 
typedef boost::shared_ptr<ResourceList> ResourceListPtr; 

struct ServChannel
{
    //正在抓取的resource
    typedef long long ServKey;

    static const double DEFAULT_MAX_ERR_RATE  = 0.8;
    static const ConcurencyMode DEFAULT_CONCURENCY_MODE = CONCURENCY_PER_SERV;
    static const double DEFAULT_ERR_DELAY_SEC = 0;
    static const unsigned DEFAULT_MAX_ERR_NUM = 20;

    //统计错误率
    StasticCount<double, 100> err_rate_;
    //记录最近的抓取时间
    time_t   fetch_time_ms_;
    //统计对端服务器答复的快慢
    StasticCount<double, 100> resp_time_;
    //该serv的国内外属性
    unsigned char is_foreign_: 1;
    //该serv连续失败的次数
    unsigned char err_count_ : 1;
    uint16_t err_delay_sec_;
    //并发模式
    ConcurencyMode concurency_mode_;
    //连接池
    std::deque<Connection*> conn_storage_;
    //流控队列的链接指针
    linked_list_node_t queue_node_;
    //cache队列的链接指针
    linked_list_node_t cache_node_;
    //抓取间隔时间 
    unsigned fetch_interval_ms_;
    //连续失败的最大次数
    unsigned max_err_count_;
    //该serv允许的最大错误率 
    double   max_err_rate_;
    //正在抓取的resource列表
    ResourceList fetching_lst_;
    ServKey serv_key_;
    SpinLock    lock_;
    //不固定serv的res --> 待抓取的host列表
    HostChannelList wait_host_lst_;
    //不固定serv的res --> 空闲的host列表
    HostChannelList idle_host_lst_;
    //固定serv的res
    ResPriorQueue * pres_wait_queue_;

    //fetch_interval_ms为抓取的间隔时间, 单位为毫秒
    ServChannel():
        fetch_time_ms_(0), is_foreign_(0), err_count_(0), 
        err_delay_sec_(DEFAULT_ERR_DELAY_SEC), 
        concurency_mode_(DEFAULT_CONCURENCY_MODE), 
        fetch_interval_ms_(0), 
        max_err_count_(DEFAULT_MAX_ERR_NUM),
        max_err_rate_(DEFAULT_MAX_ERR_RATE), 
        serv_key_(0), pres_wait_queue_(NULL)
    {}

    time_t GetReadyTime() const
    {
        unsigned delay_time = err_delay_sec_*(2 << err_count_)*1000;
        if(delay_time < fetch_interval_ms_)
            delay_time = fetch_interval_ms_;
        return fetch_time_ms_ + delay_time;
    }
    void SetFetchTime(time_t cur_time)
    {
        fetch_time_ms_ = cur_time;
    }
    void AddSucc()
    {
        if(err_count_)
            err_count_ = 0;
        err_rate_.Add(0.0);
    }
    void AddFail()
    {
        ++err_count_;
        err_rate_.Add(1.0);
    }
    void AddRespTime(time_t resp_time)
    {
        resp_time_.Add(resp_time); 
    }
    void SetForeign(bool is_foreign)
    {
        is_foreign_ = (char)is_foreign;
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
        return is_foreign_ != 0;
    }
    bool IsServErr() const
    {
        return err_rate_.Average() > max_err_rate_ || 
            err_count_ > max_err_count_;
    } 
    ServKey GetServKey() const
    {
        return serv_key_;
    }
};

typedef linked_list_t<ServChannel, &ServChannel::cache_node_> ServCacheList;
typedef linked_list_t<HostChannel, &HostChannel::cache_node_> HostCacheList;

#endif
