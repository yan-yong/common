#include <boost/date_time/posix_time/posix_time.hpp>
#include "httpserver.h"
#include "connection.hpp"
#include "linklist/shared_linked_list.hpp" 

using namespace http::server4;

typedef shared_linked_list_t<connection, &connection::node_> conn_list_t;
static const time_t DNS_CACHE_TIME = 3600; 

HttpServer::HttpServer(): 
    conn_timeout_sec_(0), io_work_(io_service_),
    dns_resolver_(DNS_CACHE_TIME), stop_(false)
{
    conn_lst_ = (void*)new conn_list_t;
    assert(dns_resolver_.Open() == 0);
}

HttpServer::~HttpServer()
{
    stop();
    delete (conn_list_t*)conn_lst_;
    conn_lst_ = NULL;
}

void HttpServer::http_connect_received(sock_ptr_t client_sock, const err_code_t& ec)
{
    sock_ptr_t another_sock(new boost::asio::ip::tcp::socket(io_service_));
    boost::function<void (const err_code_t&)> handler = 
        boost::bind(&HttpServer::http_connect_received, shared_from_this(), another_sock, _1);
    acceptor_->async_accept(*another_sock, handler);

    if(!client_sock)
        return;
    if(ec)
    {
        LOG_ERROR("accept error: %s.\n", ec.message().c_str());
        return;
    }

    conn_ptr_t conn(new connection(client_sock, this));
    ((conn_list_t*)conn_lst_)->add_back(conn);
    conn->read_http_request();
}

void HttpServer::http_request_received(conn_ptr_t conn)
{
    // The request was invalid.
    if(!conn->is_valid_request())
    {
        conn->write_http_bad_request();
        conn->set_keep_alive(0);
        return;
    }

    request_ptr_t p_req = conn->get_request();
    assert(p_req);
    if(p_req->method == "CONNECT")
    {
        //对于CONNECT请求，强制keepalive
        conn->set_keep_alive(1);
        handle_tunnel_request(conn);
        return;
    }
    handle_normal_request(conn);
}

void HttpServer::http_request_finished(conn_ptr_t conn)
{
    if(conn->is_error() || !conn->is_keep_alive())
    {
        remove_connection(conn);
        return;
    }

    //keep_alive 则继续读入数据
    request_ptr_t p_req = conn->get_request();
    assert(p_req);
    conn->read_http_request();
}

// 普通http请求
void HttpServer::handle_normal_request(conn_ptr_t conn)
{
    //如果设置了handler, 则使用该handler
    if(norm_http_handler_)
    {
        norm_http_handler_(conn);
        return;
    }
    //default
    conn->write_http_ok();
}

// 接收到http connect请求
void HttpServer::handle_tunnel_request(conn_ptr_t conn)
{
    if(tunnel_http_handler_)
    {
        tunnel_http_handler_(conn);
        return;
    }
    request_ptr_t p_req = conn->get_request();
    std::string host = p_req->uri;
    uint16_t    port = 443;
    size_t sep_idx   = p_req->uri.find(":");
    if(sep_idx != std::string::npos)
    {
        host = p_req->uri.substr(0, sep_idx);
        port = atoi(p_req->uri.c_str() + sep_idx + 1);
    }
    DNSResolver::ResolverCallback resolver_cb = 
        boost::bind(&HttpServer::fetch_dns_result, shared_from_this(), _1);
    LOG_DEBUG("put to dns resolve: %s %d\n", host.c_str(), port);
    dns_resolver_.Resolve(host, port, resolver_cb, (void*)conn.get());
}

void HttpServer::fetch_dns_result(DNSResolver::DnsResultType dns_result)
{
    io_service_.post(boost::bind(&HttpServer::handle_dns_result, shared_from_this(), dns_result));
}

void HttpServer::tunnel_connect(conn_ptr_t conn, const std::string& addr_str, uint16_t port, bool second_tunnel)
{
    sock_ptr_t tunnel_socket(new boost::asio::ip::tcp::socket(io_service_));
    conn->tunnel_connect(tunnel_socket, boost::asio::ip::address::from_string(addr_str), port, second_tunnel);
}

void HttpServer::handle_dns_result(DNSResolver::DnsResultType dns_result)
{
    connection* raw_ptr = (connection*)dns_result->contex_;
    assert(raw_ptr);
    conn_ptr_t conn = ((conn_list_t*)conn_lst_)->entry(raw_ptr);
    // connection 此时已经超时
    if(!conn)
        return;
    request_ptr_t p_req = conn->get_request();
    if(!dns_result->ai_)
    {
        LOG_ERROR("Http connect %s dns error: %s %s\n", p_req->uri.c_str(), 
            dns_result->err_msg_.c_str(), conn->peer_addr().c_str());
        conn->write_http_service_unavailable();
        conn->set_keep_alive(0);
        return;
    }
    std::string addr_str;
    uint16_t port = 0;
    dns_result->GetAddr(addr_str, port);
    tunnel_connect(conn, addr_str, port);
}

void HttpServer::handle_conn_update(conn_ptr_t conn)
{

    ((conn_list_t*)conn_lst_)->del(conn);
    ((conn_list_t*)conn_lst_)->add_back(conn);
}

void HttpServer::remove_connection(conn_ptr_t conn)
{
    conn->close();
    ((conn_list_t*)conn_lst_)->del(conn);
}

void HttpServer::check_timeout()
{
    conn_list_t* conn_lst = (conn_list_t*)conn_lst_;
    while(!conn_lst->empty())
    {
        conn_ptr_t conn = conn_lst->get_front();
        if(conn->resp_time() + conn_timeout_sec_ > time(NULL))
            break;
        conn_lst->pop_front();
        conn->handle_timeout();
        remove_connection(conn);
    }
    boost::asio::deadline_timer t(io_service_, boost::posix_time::seconds(1));
    t.async_wait(boost::bind(&HttpServer::check_timeout, shared_from_this()));
}

int HttpServer::initialize(std::string ip, std::string port, 
    time_t conn_timeout_sec)
{
    ip_ = ip;
    conn_timeout_sec_ = conn_timeout_sec;

    try
    {
        boost::asio::ip::tcp::resolver resolver(io_service_);
        boost::asio::ip::tcp::resolver::query query(ip, port);
        acceptor_.reset(new boost::asio::ip::tcp::acceptor(io_service_, *resolver.resolve(query)));
        err_code_t err_code;
        http_connect_received(sock_ptr_t(), err_code);
    }
    catch(std::exception& e)
    {
        LOG_ERROR("[HttpServer] open failed at %s:%s: %s\n", 
            ip_.c_str(), port.c_str(), e.what());
        return -1;
    }
    LOG_INFO("[HttpServer] listen at %s:%s\n", ip_.c_str(), port.c_str());

    if(conn_timeout_sec_ > 0)
    {
        boost::asio::deadline_timer t(io_service_, boost::posix_time::seconds(1));
        t.async_wait(boost::bind(&HttpServer::check_timeout, shared_from_this()));
    }
    return 0;
}

int HttpServer::run()
{
    while(true)
    { 
        try
        {
            io_service_.run();
            break;
        }
        catch(std::exception& e)
        {
            LOG_ERROR("[HttpServer] run exception occur: %s\n", e.what());
        }
    }
    return 0;
}

void HttpServer::stop()
{
    if(!stop_)
        return;
    stop_ = true;
    io_service_.stop();
    dns_resolver_.Close();
}

void HttpServer::post(boost::function<void (void)> cb)
{
    io_service_.post(cb);
}

void HttpServer::add_runtine(time_t interval_sec, boost::function< void (void)> runtine)
{
    if(interval_sec == 0)
        return;
    runtine();
    boost::asio::deadline_timer t(io_service_, boost::posix_time::seconds(1));
    t.async_wait(boost::bind(&HttpServer::add_runtine, shared_from_this(), interval_sec, runtine));
}
