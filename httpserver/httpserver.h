#ifndef __HTTP_SERVER_H
#define __HTTP_SERVER_H

#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <signal.h>
#include "server.hpp"
#include "reply.hpp"
#include <string>
#include "request.hpp"
#include "boost/lexical_cast.hpp"
#include "log/log.h"
#include <boost/enable_shared_from_this.hpp>

class HttpSession
{
    boost::shared_ptr<boost::asio::ip::tcp::socket> m_socket;
    boost::function<void (boost::system::error_code, std::size_t)> m_finish_response_handler;
    //private construct method
    HttpSession(boost::shared_ptr<boost::asio::ip::tcp::socket> socket)
        :  m_socket(socket), m_reply(new http::server4::reply())
    {
    }
public:
    boost::shared_ptr<http::server4::reply> m_reply;
     
    std::string ip()
    {
        return m_socket->remote_endpoint().address().to_string();
    }
    std::string port()
    {
        return boost::lexical_cast<std::string>(m_socket->remote_endpoint().port()); 
    }
    void close()
    {
        boost::system::error_code ec;
        if(m_socket){
            m_socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            m_socket.reset();
            m_reply.reset();
        }
    }
    void send_response()
    {
        assert(m_socket);
        boost::asio::async_write(*m_socket, m_reply->to_buffers(), m_finish_response_handler); 
    }
    ~HttpSession()
    {
        close();
    }
    friend class HttpServer;
};

class HttpServer: public boost::enable_shared_from_this<HttpServer>
{
    std::string m_ip;
    std::string m_port;
    boost::asio::io_service m_io_service;
    boost::shared_ptr<http::server4::server> m_server_fun;
    boost::asio::signal_set m_signals;

    void recv_request(boost::shared_ptr<http::server4::request> req, boost::shared_ptr<boost::asio::ip::tcp::socket> socket);

public:
    HttpServer(): m_signals(m_io_service)
    {
    }

    ~HttpServer()
    {
    }

    int initialize(const std::string& ip, const std::string& port);
    int run();

    /**************** interface to be rewrite ***************/
    virtual void handle_recv_request(boost::shared_ptr<http::server4::request> req, 
        boost::shared_ptr<HttpSession> session);
    virtual void handle_finish_response( boost::shared_ptr<HttpSession> session, 
        boost::system::error_code ec, std::size_t len);
    /********************************************************/
 };

#endif
