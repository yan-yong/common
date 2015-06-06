#ifndef __PROXY_HPP
#define __PROXY_HPP

#include "utility/murmur_hash.h"
#include "jsoncpp/include/json/json.h"
#include "utility/ip_location.h"

struct Proxy
{
    static const unsigned PROXY_SIZE = 50; 
    enum State
    {
        SCAN_IDLE,
        SCAN_HTTP,
        SCAN_CONNECT,
        SCAN_HTTPS,
        SCAN_JUDGE
    } state_;
    char ip_[16];
    uint16_t port_;
    unsigned err_num_;
    unsigned request_cnt_;
    time_t   request_time_;
    unsigned char http_enable_ :1;
    unsigned char https_enable_:1;
    // 0 表示国内IP
    unsigned char is_foreign_  :1;
    //代理类型
    enum Type
    {
        TYPE_UNKNOWN,
        TRANSPORT,
        ANONYMOUS,
        HIGH_ANONYMOUS
    } type_;

    char fill_buf_[PROXY_SIZE - sizeof(state_) - sizeof(type_) -
        sizeof(ip_) - sizeof(port_) - sizeof(err_num_) - 
        sizeof(request_cnt_) - sizeof(request_time_) - 1];

    Proxy()
    {
        memset(this, 0, sizeof(Proxy));
    }

    Proxy(std::string ip, uint16_t port)
    {
        SetAddress(ip, port);
    }

    ~Proxy()
    {
    }

    void SetAddress(const std::string& ip, uint16_t port)
    {
        memset(this, 0, sizeof(Proxy));
        strcpy(ip_, ip.c_str());
        port_ = port;
        is_foreign_ = IpLocation::Instance()->location(ip_) == 0;
    }

    std::string ToString() const
    {
        char buf[100];
        snprintf(buf, 100, "%s:%hu", ip_, port_);
        return buf;
    }
    bool FromJson(const Json::Value& val)
    {
        std::string empty_val;
        // address
        Json::Value addr_obj = val.get("addr", empty_val);
        if(addr_obj.empty())
            return false;
        std::string addr_val = addr_obj.asString();
        size_t sep_idx = addr_val.find(":");
        if(sep_idx == std::string::npos)
            return false;
        strncpy(ip_, addr_val.substr(0, sep_idx).c_str(), 16);
        port_ = (uint16_t)atoi(addr_val.substr(sep_idx + 1).c_str());
        // https 
        std::string https_val = val.get("https", empty_val).asString();
        if(!addr_val.empty() && https_val == "1")
            https_enable_ = 1;
        else
            https_enable_ = 0;
        // foreign
        Json::Value foreign_obj = val.get("fr", empty_val);
        if(foreign_obj.empty() || foreign_obj.asString() == "1")
            is_foreign_ = 1;
        else
            is_foreign_ = 0;
        // type
        std::string type_val = val.get("type", empty_val).asString();
        if(!type_val.empty())
            type_ = (Type)atoi(type_val.c_str());
        // avail
        std::string avail_val = val.get("avail", empty_val).asString();
        if(!avail_val.empty()) 
            request_cnt_ = (unsigned)atoi(avail_val.c_str());
        return true;
    }
    Json::Value ToJson() const
    {
        Json::Value json_val;
        json_val["addr"] = ToString();
        if(https_enable_)
            json_val["https"] = "1";
        if(type_)
            json_val["type"]  = type_;
        char available_cnt_str[10];
        snprintf(available_cnt_str, 10, "%u", 
            request_cnt_ - err_num_);
        json_val["avail"] = available_cnt_str;
        if(is_foreign_)
            json_val["fr"] = "1";
        else
            json_val["fr"] = "0";
        return json_val;
    }
    bool operator < (const Proxy& other) const
    {
        int ret = strncmp(ip_, other.ip_, 16);
        return ret < 0 || (ret == 0 && port_ < other.port_);
    }
    struct sockaddr * AcquireSockAddr() const
    {
        return get_sockaddr_in(ip_, port_);
    }
} __attribute__((packed));

struct HashFunctor
{
    uint64_t operator () (const Proxy& proxy)
    {
        uint64_t val = 0;
        std::string proxy_str = proxy.ToString();
        MurmurHash_x64_64(proxy_str.c_str(), proxy_str.size(), &val);
        return val; 
    }
};

#endif
