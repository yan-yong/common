#ifndef HTTP_SERVER4_CONNECTION_HPP
#define HTTP_SERVER4_CONNECTION_HPP
#include <boost/lexical_cast.hpp>
#include "request.hpp"
#include "reply.hpp"
#include "httpserver.h"
#include <boost/logic/tribool.hpp>
#include <boost/tuple/tuple.hpp>
#include "request_parser.hpp"

class HttpServer;

namespace http {
namespace server4 {

typedef boost::asio::ip::tcp::socket* sock_ptr_t;
typedef boost::system::error_code err_code_t;

class connection
{
    sock_ptr_t socket_;
    request* request_;
    boost::array<char, 8192> *buffer_;
    err_code_t errno_;
    HttpServer* serv_;
    char keep_alive_:   1;
    bool closed_    :   1;
    boost::tribool valid_request_;
    request_parser* request_parse_;

    void __read_data_handler(const err_code_t& ec, 
        std::size_t bytes_transferred);

    void __write_data_handler(const err_code_t& ec,
        std::size_t bytes_transferred);
public:
    linked_list_node_t node_;

public:
    connection(boost::asio::ip::tcp::socket* socket, 
        HttpServer* serv, bool keep_alive = NULL);

    ~connection();
    
    void close();

    void read_data();

    void write_data(boost::shared_ptr<reply> reply);

    std::string peer_ip() const
    {
        return socket_->remote_endpoint().address().to_string();
    }

    std::string peer_port() const
    {
        return boost::lexical_cast<std::string>(socket_->remote_endpoint().port()); 
    }

    std::string to_string() const
    {
        return peer_ip() + ":" + peer_port();
    }

    bool is_valid_request() const
    {
        return valid_request_; 
    }

    bool is_error() const
    {
        return errno_;
    }

    bool is_keep_alive() const
    {
        return keep_alive_;
    }

    bool is_closed() const
    {
        return closed_;
    }
 
    std::string error_msg() const
    {
        //return errno_.message();
        return boost::system::system_error(errno_).what();
    }

    friend class HttpServer;
};

}
}
#endif
