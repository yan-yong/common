#ifndef __HTTP_SERVER_H
#define __HTTP_SERVER_H

#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <signal.h>
#include <string>
#include "boost/lexical_cast.hpp"
#include <boost/enable_shared_from_this.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/function.hpp>
#include "reply.hpp"
#include "request.hpp"
#include "log/log.h"
#include "linklist/linked_list_map.hpp"

namespace http
{
    namespace server4
    {
        class connection;
    }
}

class HttpServer: public boost::enable_shared_from_this<HttpServer>
{

public:
    typedef boost::function<void (http::server4::connection*)> RecvReqHandler;
    typedef boost::function<void (http::server4::connection*)> FinishRespHandler;
    typedef boost::asio::ip::tcp::socket* sock_ptr_t;

private:
    time_t conn_timeout_sec_;
    std::string ip_;

    /// Acceptor used to listen for incoming connections.
    boost::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor_;

    boost::asio::io_service io_service_;
    boost::asio::io_service::work io_work_;
    RecvReqHandler recv_req_handler_;
    FinishRespHandler fin_resp_handler_;
    void* timeout_map_;

protected:
    /// 接收到请求时的处理函数
    virtual void handle_recv_request(http::server4::connection*);

    /// 答复完成时的处理函数
    virtual void handle_finish_response(http::server4::connection*);

    /// 接收到客户端的连接
    virtual void handle_recv_connect(sock_ptr_t client_sock, const boost::system::error_code&);

    void update_timeout(http::server4::connection*);

    void remove_timeout(http::server4::connection*);
    
    void check_timeout();
 
    virtual void handle_conn_timeout(http::server4::connection* conn);

public:
    HttpServer();

    ~HttpServer()
    {
    }

    virtual int initialize(std::string ip, 
        std::string port, time_t conn_timeout_sec = 30);

    virtual int run();

    void set_recv_request_handler(RecvReqHandler handler);

    void set_finish_response_handler(FinishRespHandler handler);

    void stop();

    friend class http::server4::connection;
 };

#endif
