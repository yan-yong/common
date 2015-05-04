#include <string.h>
#include <boost/typeof/typeof.hpp>
#include "connection.hpp"
#include "httpserver.h"

namespace http
{
namespace server4
{

//read_peer: true表示读取peer_socket_, 否则读取tunnel_socket_
void connection::__read_tunnel_data_handler(bool is_peer, 
    const err_code_t& ec, std::size_t bytes_transferred)
{
    if(closed_)
        return;
    std::string addr_str   = is_peer ? peer_addr() : tunnel_addr();
    sock_ptr_t  wt_sock    = is_peer ? tunnel_socket_ : peer_socket_;
    sock_ptr_t  rd_sock    = is_peer ? peer_socket_ : tunnel_socket_;
    rd_buffer_t& rd_buffer = is_peer ? peer_rd_buffer_ : tunnel_rd_buffer_;
    wt_buffer_t& wt_buffer = is_peer ? peer_wt_buffer_ : tunnel_wt_buffer_;
    wt_buffer.reset(new std::vector<char>(rd_buffer->begin(), rd_buffer->begin() + bytes_transferred));

    if(ec)
        errno_ = ec;
    if(is_error())
    {
        LOG_ERROR("read_tunnel_data from %s error:%s\n", 
            addr_str.c_str(), error_msg().c_str());
        serv_->http_request_finished(shared_from_this());
        return;
    }
    serv_->handle_conn_update(shared_from_this());

    //从一方读取，写入另一方
    BOOST_AUTO(write_cb, boost::bind(&connection::__write_tunnel_data_handler, 
        shared_from_this(), !is_peer, _1, _2));
    boost::asio::async_write(*wt_sock, boost::asio::buffer(*wt_buffer), write_cb);

    if(!rd_buffer)
        rd_buffer.reset(new boost::array<char, 8192>());
    BOOST_AUTO(read_cb, boost::bind(&connection::__read_tunnel_data_handler, 
        shared_from_this(), is_peer, _1, _2));
    rd_sock->async_read_some(boost::asio::buffer(*rd_buffer), read_cb);
}

void connection::__write_tunnel_data_handler(bool is_peer, 
    const err_code_t& ec, std::size_t bytes_transferred)
{
    if(closed_)
        return;
    std::string addr_str  = is_peer ? peer_addr() : tunnel_addr();
    sock_ptr_t  rd_sock   = is_peer ? peer_socket_ : tunnel_socket_;
    rd_buffer_t& rd_buffer= is_peer ? peer_rd_buffer_ : tunnel_rd_buffer_;
    if(!rd_buffer)
        rd_buffer.reset(new boost::array<char, 8192>());

    if(ec)
        errno_ = ec;
    if(is_error())
    {
        LOG_ERROR("write_tunnel_data from %s error:%s\n", 
            addr_str.c_str(), error_msg().c_str());
        serv_->http_request_finished(shared_from_this());
        return;
    }
    serv_->handle_conn_update(shared_from_this());

    BOOST_AUTO(read_cb, boost::bind(&connection::__read_tunnel_data_handler, 
        shared_from_this(), is_peer, _1, _2));
    rd_sock->async_read_some(boost::asio::buffer(*rd_buffer), read_cb);
}

/// http tunnel连接完成
void connection::__tunnel_connect_handler(bool second_tunnel, const err_code_t& ec)
{
    if(closed_)
        return;
    serv_->handle_conn_update(shared_from_this());
    http_reply_.reset(new reply());
    if(ec)
    {
        errno_ = ec;
        LOG_ERROR("http tunnel connect %s error: %s for %s\n", 
            tunnel_addr().c_str(), error_msg().c_str(), peer_addr().c_str());
        *http_reply_ = reply::stock_reply(reply::service_unavailable);
        boost::asio::async_write(*peer_socket_, http_reply_->to_buffers(), 
            boost::bind(&connection::__write_tunnel_data_handler, shared_from_this(), true, _1, _2) );
        return;
    }
    if(!second_tunnel)
    {
        LOG_DEBUG("http tunnel %s establish success for %s\n", tunnel_addr().c_str(), peer_addr().c_str());
        http_reply_->status = reply::establish_ok;
        boost::asio::async_write(*peer_socket_, http_reply_->to_buffers(), 
            boost::bind(&connection::__write_tunnel_data_handler, shared_from_this(), true, _1, _2) );
        return;
    }
    LOG_DEBUG("http tunnel connect success to proxy %s for %s\n", tunnel_addr().c_str(), peer_addr().c_str());
    // 二级代理则直接转发请求
    boost::asio::async_write(*peer_socket_, http_request_->to_buffers(), 
        boost::bind(&connection::__write_tunnel_data_handler, shared_from_this(), false, _1, _2) );
}

void connection::tunnel_connect(sock_ptr_t tunnel_socket, 
    const boost::asio::ip::address& addr, uint16_t port, bool second_tunnel)
{
    boost::asio::ip::tcp::endpoint end_point(addr, port);
    tunnel_socket_ = tunnel_socket;
    tunnel_ip_  = addr.to_string();
    tunnel_port_= port;
    LOG_INFO("try connecting to tunnel: %s %hu for %s\n", 
        tunnel_ip_.c_str(), tunnel_port_, peer_addr().c_str());
    tunnel_socket_->async_connect(end_point, 
        boost::bind(&connection::__tunnel_connect_handler, shared_from_this(), second_tunnel, _1));
}


void connection::__read_http_data_handler(const err_code_t& ec, 
        std::size_t bytes_transferred)
{
    if(closed_)
        return;
    if(ec)
        errno_ = ec;
    if(is_error())
    {
        LOG_ERROR("read_http_data from %s error:%s\n", 
            peer_addr().c_str(), error_msg().c_str());
        serv_->http_request_finished(shared_from_this());
        return;
    }

    serv_->handle_conn_update(shared_from_this());
    // Parse the http request which we just received.
    boost::tie(valid_request_, boost::tuples::ignore)
        = request_parse_->parse(*http_request_, peer_rd_buffer_->data(), 
            peer_rd_buffer_->data() + bytes_transferred);
    if(boost::indeterminate(valid_request_))
    {
        peer_socket_->async_read_some(boost::asio::buffer(*peer_rd_buffer_), 
            boost::bind(&connection::__read_http_data_handler, shared_from_this(), _1, _2));
        return;
    }

    do
    {
        if(!valid_request_)
        {
            LOG_ERROR("read_http_data recv invalid request %s\n", peer_addr().c_str());
            keep_alive_    = 0;
            break;
        }
        int ret = http_request_->decompress();
        if(ret < 0)
        {
            LOG_ERROR("read_http_data content decompress error: %d %s\n", ret, peer_addr().c_str());
            valid_request_ = false;
            keep_alive_    = 0;
            break;
        }
        std::vector<std::string> vec = http_request_->get_header("Connection");
        if(vec.size() == 0)
            break;
        if(strcasestr(vec[0].c_str(), "keep-alive"))
            keep_alive_ = 1;
        if(strcasestr(vec[0].c_str(), "close"))
            keep_alive_ = 0;
    }while(false);

    serv_->http_request_received(shared_from_this());
}

void connection::__write_http_data_handler(const err_code_t& ec,
        std::size_t bytes_transferred)
{
    if(closed_)
        return;
    if(ec)
        errno_ = ec;
    if(is_error())
    {
        LOG_ERROR("write http data from %s error:%s\n", 
            peer_addr().c_str(), error_msg().c_str());
    }
    serv_->handle_conn_update(shared_from_this());
    serv_->http_request_finished(shared_from_this());
}

void connection::read_http_request()
{
    if(!peer_rd_buffer_)
        peer_rd_buffer_.reset(new boost::array<char, 8192>);
    http_request_.reset(new request());
    request_parse_.reset(new request_parser());
    if(!peer_rd_buffer_)
        peer_rd_buffer_.reset(new boost::array<char, 8192>());
    peer_socket_->async_read_some(boost::asio::buffer(*peer_rd_buffer_), 
        boost::bind(&connection::__read_http_data_handler, shared_from_this(), _1, _2));
}

void connection::__write_http_reply(boost::shared_ptr<reply> resp)
{
    http_reply_ = resp;
    static const unsigned GZIP_MIN_LEN = 1024;
    if(resp->content.size() > GZIP_MIN_LEN)
    {
        std::vector<std::string> header_values= http_request_->get_header("Accept-Encoding");
        if(header_values.size() && header_values[0].find("gzip") > 0)
            resp->compress();
    }
    std::string keep_alive_value = keep_alive_ ? "keep-alive":"close";
    resp->set_header("Connection", keep_alive_value);
    boost::asio::async_write(*peer_socket_, http_reply_->to_buffers(), 
        boost::bind(&connection::__write_http_data_handler, shared_from_this(), _1, _2));
}

void connection::write_http_ok()
{
    http_reply_.reset(new reply(http::server4::reply::stock_reply(http::server4::reply::ok)));
    __write_http_reply(http_reply_);
}

void connection::write_http_bad_request()
{
    http_reply_.reset(new reply(reply::stock_reply(reply::bad_request)));
    __write_http_reply(http_reply_);
}

void connection::write_http_service_unavailable()
{
    http_reply_.reset(new reply(reply::stock_reply(reply::service_unavailable)));
    __write_http_reply(http_reply_);
}

void connection::write_http_reply(boost::shared_ptr<reply> resp)
{
    http_reply_ = resp;
    __write_http_reply(http_reply_);
}

void connection::handle_timeout()
{
    if(closed_)
        return;
    is_timeout_ = 1;
    keep_alive_ = 0;
    LOG_ERROR("connection timeout %s\n", peer_addr().c_str());
}

void connection::close()
{
    if(closed_)
        return;
    err_code_t ec;
    peer_socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    if(ec)
        errno_  = ec;
    if(tunnel_socket_)
    {
        tunnel_socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        if(ec)
            errno_ = ec;
    }
    keep_alive_ = 0;
    closed_ = 1;
}

connection::connection(sock_ptr_t sock, HttpServer* serv, 
    bool keep_alive, void* context):
    peer_socket_(sock), serv_(serv), keep_alive_(keep_alive), 
    closed_(0), is_timeout_(0), 
    valid_request_(true), context_(context), resp_time_(time(NULL))
{
    peer_ip_   = peer_socket_->remote_endpoint().address().to_string();
    peer_port_ = (uint16_t)peer_socket_->remote_endpoint().port();
}

connection::~connection()
{
    close();
    fprintf(stderr, "destroy ...\n");
}

}
}
