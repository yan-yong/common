#include "httpserver.h"

void HttpServer::recv_request(boost::shared_ptr<http::server4::request> req, boost::shared_ptr<boost::asio::ip::tcp::socket> socket)
{
    boost::shared_ptr<HttpSession> session(new HttpSession(socket));
    LOG_INFO("[HttpServer] Receive request from %s:%s %s %s content_len: %zd\n", 
            session->ip().c_str(), session->port().c_str(),
            req->uri.c_str(), req->method.c_str(), req->content.size());
    boost::function<void (boost::system::error_code, size_t)> handler = 
        boost::bind(&HttpServer::handle_finish_response, shared_from_this(), session,_1, _2);
    session->m_finish_response_handler = handler;
    handle_recv_request(req, session);
}

int HttpServer::initialize(const std::string& ip, const std::string& port)
{
    m_ip = ip;
    m_port = port;
    http::server4::RequestHandler request_handler = 
        boost::bind(&HttpServer::recv_request, shared_from_this(), _1, _2);
    try {
        m_server_fun.reset(new http::server4::server(m_io_service, ip, port, request_handler));
        (*m_server_fun)();
        m_signals.add(SIGINT);
        m_signals.add(SIGTERM);
#if defined(SIGQUIT)
        m_signals.add(SIGQUIT);
#endif // defined(SIGQUIT)
        m_signals.async_wait(boost::bind(&boost::asio::io_service::stop, &m_io_service));
        LOG_INFO("[HttpServer] listen at %s:%s\n", m_ip.c_str(), m_port.c_str());
    }
    catch(std::exception& e)
    {
        LOG_ERROR("[HttpServer] open failed at %s:%s: %s\n", 
                e.what(), m_ip.c_str(), m_port.c_str());
        return -1;
    }
    return 0;
}

int HttpServer::run()
{
    assert(m_server_fun);
    while(true)
    { 
        try
        {
            m_io_service.run();
            break;
        }
        catch(std::exception& e)
        {
            LOG_ERROR("[HttpServer] run exception occur: %s\n", e.what());
        }
    }
    return 0;
}

void HttpServer::handle_recv_request(boost::shared_ptr<http::server4::request> req, 
        boost::shared_ptr<HttpSession> session)
{
    *(session->m_reply) = http::server4::reply::stock_reply(http::server4::reply::ok);
    session->send_response();
}

void HttpServer::handle_finish_response(
        boost::shared_ptr<HttpSession> session, 
        boost::system::error_code ec, std::size_t len)
{
    if(!ec){
        LOG_DEBUG("[HttpServer] Send %zd bytes to %s:%s success.\n", len, 
                session->ip().c_str(), session->port().c_str()); 
    }
    else
    {   
        LOG_ERROR("[HttpServer] Send %zd bytes to %s:%s failed: %s.\n", 
                len, session->ip().c_str(), session->port().c_str(), 
                boost::system::system_error(ec).what());
    }
    //short connection
    session->close();
}
