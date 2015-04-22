#include <boost/date_time/posix_time/posix_time.hpp>
#include "httpserver.h"
#include "connection.hpp" 

using namespace http::server4;

typedef linked_list_map<time_t, connection, &connection::node_> timeout_map_t;

HttpServer::HttpServer(): 
    conn_timeout_sec_(0), io_work_(io_service_)
{
    timeout_map_ = (void*)new timeout_map_t;
}

void HttpServer::stop()
{
    io_service_.stop();
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
        handle_recv_connect(NULL, err_code);
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
        t.async_wait(boost::bind(&HttpServer::check_timeout, this));
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

void HttpServer::check_timeout()
{
    timeout_map_t* tm_map = (timeout_map_t*)timeout_map_;
    while(!tm_map->empty())
    {
        time_t timeout_stamp = 0;
        connection* conn = NULL;
        tm_map->get_front(timeout_stamp, conn);
        if(timeout_stamp > time(NULL))
            break;
        tm_map->pop_front();
        handle_conn_timeout(conn);
    }
    boost::asio::deadline_timer t(io_service_, boost::posix_time::seconds(1));
    t.async_wait(boost::bind(&HttpServer::check_timeout, this));
}

void HttpServer::handle_recv_connect(
    sock_ptr_t client_sock, const err_code_t& ec)
{
    sock_ptr_t another_sock = new boost::asio::ip::tcp::socket(io_service_);
    boost::function<void (const err_code_t&)> handler = 
        boost::bind(&HttpServer::handle_recv_connect, this, another_sock, _1);
    acceptor_->async_accept(*another_sock, handler);

    if(!client_sock)
        return;

    if(ec)
    {
        LOG_ERROR("accept error: %s.\n", ec.message().c_str());
        delete client_sock;
        return;
    }

    connection* conn = new connection(client_sock, this);
    conn->read_data();
}

void HttpServer::handle_recv_request(connection* conn)
{
    if(conn->is_error())
    {
        //close
        if(!conn->is_closed())
        {
            LOG_ERROR("recv request error %s from %s\n", 
                conn->error_msg().c_str(), conn->to_string().c_str());
        }
        delete conn;
        return;
    }
    // The request was invalid.
    if(!conn->is_valid_request())
    {
        LOG_ERROR("recv invalid request from %s\n", conn->to_string().c_str());
        boost::shared_ptr<reply> resp(new reply(reply::stock_reply(reply::bad_request)));
        conn->write_data(resp);
        return;
    }
    if(recv_req_handler_)
    {
        recv_req_handler_(conn);
        return;
    }
    // default action
    boost::shared_ptr<reply> resp(new 
        reply(http::server4::reply::stock_reply(http::server4::reply::ok)));
    conn->write_data(resp);
}

void HttpServer::handle_finish_response(connection* conn)
{
    if(conn->is_error())
    {
        LOG_ERROR("[HttpServer] Send to %s failed: %s.\n", 
            conn->to_string().c_str(), conn->error_msg().c_str());
        delete conn; 
        return;
    }

    LOG_DEBUG("[HttpServer] Send to %s success.\n", conn->to_string().c_str());
    if(!conn->is_keep_alive())
    {
        delete conn;
        return;
    }
    conn->read_data();
}

void HttpServer::handle_conn_timeout(connection* conn)
{
    LOG_DEBUG("connection timeout: %s\n", conn->to_string().c_str());
    conn->close();
}

void HttpServer::update_timeout(connection* conn)
{
    if(conn_timeout_sec_)
    {
        ((timeout_map_t*)timeout_map_)->del(*conn);
        ((timeout_map_t*)timeout_map_)->add_back(
            time(NULL) + conn_timeout_sec_, *conn);
    }
}

void HttpServer::remove_timeout(connection* conn)
{
    if(conn_timeout_sec_)
        ((timeout_map_t*)timeout_map_)->del(*conn);
}
