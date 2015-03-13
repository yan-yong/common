#include "Channel.hpp"
#include "utility/murmur_hash.h"
#include "utility/net_utility.h"

Connection* create_connection(struct sockaddr* dst_addr)
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

ServChannel::ServChannel(
    struct addrinfo* ai, ServKey serv_key, unsigned max_err_rate, 
    ConcurencyMode concurency_mode, struct sockaddr* local_addr):
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
        Connection* conn = create_connection(copy_addrinfo(cur_ai));
        if(!conn)
            break;
        conn_storage_.push(conn);
        cur_ai = cur_ai->ai_next;
    }
}
