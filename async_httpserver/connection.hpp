#ifndef HTTP_SERVER4_CONNECTION_HPP
#define HTTP_SERVER4_CONNECTION_HPP
#include <boost/lexical_cast.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/enable_shared_from_this.hpp>
#include "request.hpp"
#include "reply.hpp"
#include "httpserver.h"
#include "request_parser.hpp"

typedef boost::shared_ptr<boost::asio::ip::tcp::socket> sock_ptr_t;
typedef boost::shared_ptr<http::server4::request>   request_ptr_t;
typedef boost::system::error_code                    err_code_t;
typedef boost::shared_ptr<boost::array<char, 8192> > rd_buffer_t;
typedef boost::shared_ptr< std::vector<char> >       wt_buffer_t;
typedef boost::shared_ptr<http::server4::reply>      reply_ptr_t;

class HttpServer;

namespace http {
namespace server4 {

class connection: public boost::enable_shared_from_this<connection>
{
    sock_ptr_t    peer_socket_;
    request_ptr_t http_request_;
    reply_ptr_t   http_reply_;
    rd_buffer_t   peer_rd_buffer_;
    wt_buffer_t   peer_wt_buffer_;

    sock_ptr_t    tunnel_socket_;
    rd_buffer_t   tunnel_rd_buffer_;
    wt_buffer_t   tunnel_wt_buffer_; 

    err_code_t    errno_;
    HttpServer*   serv_;
    char keep_alive_ :   1;
    char closed_     :   1;
    char is_timeout_ :   1;
    boost::tribool valid_request_;
    boost::shared_ptr<request_parser> request_parse_;
    void* context_;

    std::string peer_ip_;
    uint16_t    peer_port_;
    std::string tunnel_ip_;
    uint16_t    tunnel_port_;
    time_t      resp_time_;

public:
    linked_list_node_t node_;

private:
    void __read_tunnel_data_handler(bool is_peer, const err_code_t& ec, std::size_t bytes_transferred);

    void __write_tunnel_data_handler(bool is_peer, const err_code_t& ec, std::size_t bytes_transferred);

    void __tunnel_connect_handler(bool second_tunnel, const err_code_t& ec);

    void __read_http_data_handler(const err_code_t& ec, std::size_t bytes_transferred);

    void __write_http_data_handler(const err_code_t& ec, std::size_t bytes_transferred);

    void __write_http_reply(boost::shared_ptr<reply> resp);

public:
    connection(sock_ptr_t sock, HttpServer* serv, 
        bool keep_alive = 0, void* context = NULL);

    ~connection();
    
    void close();

    void read_http_request();

    void write_http_reply(boost::shared_ptr<reply> reply);

    void write_http_ok();

    void write_http_bad_request(const std::string & cont = std::string());

    void write_http_service_unavailable(const std::string & cont = std::string());

    void tunnel_connect(sock_ptr_t, const boost::asio::ip::address& addr, uint16_t port, bool second_tunnel = false);

    void handle_timeout();

    std::string peer_ip() const
    {
        return peer_ip_;
    }

    std::string peer_port() const
    {
        return boost::lexical_cast<std::string>(peer_port_); 
    }

    std::string peer_addr() const
    {
        return peer_ip() + ":" + peer_port();
    }

    std::string tunnel_ip() const
    {
        return tunnel_ip_;
    }

    std::string tunnel_port() const
    {
        return boost::lexical_cast<std::string>(tunnel_port_);
    }

    std::string tunnel_addr() const
    {
        return tunnel_ip() + ":" + tunnel_port(); 
    }

    bool is_valid_request() const
    {
        return valid_request_; 
    }

    bool is_error() const
    {
        return is_timeout_ || errno_ || closed_;
    }

    bool is_keep_alive() const
    {
        return keep_alive_;
    }

    void set_keep_alive(int keep_alive)
    {
        keep_alive_ = keep_alive;
    }

    bool is_closed() const
    {
        return closed_;
    }

    std::string error_msg() const
    {
        //return errno_.message();
        if(is_timeout_)
            return "socket timeout.";
        return boost::system::system_error(errno_).what();
    }

    request_ptr_t get_request() const
    {
        return http_request_;
    }

    time_t resp_time() const
    {
        return resp_time_;
    }

    friend class HttpServer;
};

}
}
#endif
