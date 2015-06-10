#ifndef __HTTP_SERVER_H
#define __HTTP_SERVER_H

#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <signal.h>
#include <string>
#include "boost/lexical_cast.hpp"
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/function.hpp>
#include "reply.hpp"
#include "request.hpp"
#include "log/log.h"
#include "linklist/shared_linked_list.hpp"
#include "dnsresolver/DNSResolver.hpp"

namespace http
{
    namespace server4
    {
        class connection;
    }
}

typedef boost::shared_ptr<boost::asio::ip::tcp::socket> sock_ptr_t;
typedef boost::shared_ptr<http::server4::connection> conn_ptr_t;

class HttpServer: public boost::enable_shared_from_this<HttpServer>
{

public:
    typedef boost::function<void (conn_ptr_t)> NormHttpHandler;
    typedef boost::function<void (conn_ptr_t)> TunnelHttpHandler;

private:
    time_t conn_timeout_sec_;
    std::string ip_;

    /// Acceptor used to listen for incoming connections.
    boost::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor_;

    boost::asio::io_service io_service_;
    boost::asio::io_service::work io_work_;
    NormHttpHandler    norm_http_handler_;
    TunnelHttpHandler  tunnel_http_handler_;
    void* conn_lst_;
    boost::shared_ptr<DNSResolver> dns_resolver_;
    bool stop_;

protected:
    /// 接收到http请求时的处理函数
    void http_request_received(conn_ptr_t);

    /// http完成时的处理函数
    void http_request_finished(conn_ptr_t);

    /// 接收到客户端的连接
    void http_connect_received(sock_ptr_t client_sock, const boost::system::error_code&);

    void fetch_dns_result(DNSResolver::DnsResultType dns_result);

    virtual void handle_normal_request(conn_ptr_t);

    virtual void handle_tunnel_request(conn_ptr_t);

    virtual void handle_dns_result(DNSResolver::DnsResultType dns_result);

    void handle_conn_update(conn_ptr_t);

    void remove_connection(conn_ptr_t);

    /// 超时处理
    void check_timeout(); 

public:
    HttpServer(boost::shared_ptr<DNSResolver> dns_resolver = boost::shared_ptr<DNSResolver>() );

    virtual ~HttpServer();

    int initialize(std::string ip, 
        std::string port, time_t conn_timeout_sec = 30);

    int run();

    void stop();

    void post(boost::function<void (void)> cb);

    void set_norm_http_handler(NormHttpHandler handler)
    {
        norm_http_handler_ = handler;
    }

    void set_tunnel_http_handler(TunnelHttpHandler handler)
    {
        tunnel_http_handler_ = handler; 
    }

    void add_runtine(time_t interval_sec, boost::function< void (void)> runtine);

    void tunnel_connect(conn_ptr_t conn, const std::string& addr_str, uint16_t port, bool second_tunnel = false);

    friend class http::server4::connection;
 };

#endif
