#include <string.h>
#include "connection.hpp"
#include "httpserver.h"

namespace http
{
namespace server4
{

void connection::__read_data_handler(const err_code_t& ec, 
        std::size_t bytes_transferred)
{
    //对端主动关闭
    if(bytes_transferred == 0)
    {
        LOG_DEBUG("%s closed.\n", to_string().c_str());
        close();
    }
    if(ec)
        errno_ = ec;
    if(closed_ || errno_)
    {
        serv_->remove_timeout(this);
        serv_->handle_recv_request(this);
        if(request_)
        {
            delete request_;
            request_ = NULL;
            delete request_parse_;
            request_parse_ = NULL;
        }
        return;
    }
    // Parse the data we just received.
    boost::tie(valid_request_, boost::tuples::ignore)
        = request_parse_->parse(*request_, buffer_->data(), buffer_->data() + bytes_transferred);
    if(boost::indeterminate(valid_request_))
    {
        serv_->update_timeout(this);
        socket_->async_read_some(boost::asio::buffer(*buffer_), 
            boost::bind(&connection::__read_data_handler, this, _1, _2));
        return;
    }

    do
    {
        if(!valid_request_)
            break;
        int ret = request_->decompress();
        if(ret < 0)
        {
            LOG_ERROR("%s decompress error: %d\n", to_string().c_str(), ret);
            valid_request_ = false;
            break;
        }
        //对于CONNECT请求，强制keepalive
        if(request_->method == "CONNECT")
            keep_alive_ = 1;
        if(!keep_alive_)
            break;
        std::vector<std::string> vec = request_->get_header("Connection");
        if(vec.size() == 0)
            break;
        if(strcasestr(vec[0].c_str(), "keep-alive"))
            keep_alive_ = 1;
        if(strcasestr(vec[0].c_str(), "close"))
            keep_alive_ = 0;
    }while(false);

    serv_->remove_timeout(this);
    serv_->handle_recv_request(this);
    if(request_)
    {
        delete request_;
        request_ = NULL;
        delete request_parse_;
        request_parse_ = NULL;
    }
}

void connection::__write_data_handler(const err_code_t& ec,
        std::size_t bytes_transferred)
{
    if(ec)
        errno_ = ec;
    serv_->remove_timeout(this);
    serv_->handle_finish_response(this);
}

void connection::read_data()
{
    if(!buffer_)
        buffer_ = new boost::array<char, 8192>;
    if(!request_)
    {
        request_ = new request();
        request_parse_ = new request_parser();
    }
    serv_->update_timeout(this);
    socket_->async_read_some(boost::asio::buffer(*buffer_), 
        boost::bind(&connection::__read_data_handler, this, _1, _2));
}

void connection::write_data(boost::shared_ptr<reply> resp)
{
    static const unsigned GZIP_MIN_LEN = 1024;
    if(resp->content.size() > GZIP_MIN_LEN)
    {
        std::vector<std::string> header_values= request_->get_header("Accept-Encoding");
        if(header_values.size() && header_values[0].find("gzip") > 0)
            resp->compress();
    }
    std::string keep_alive_value = keep_alive_ ? "keep-alive":"close";
    resp->set_header("Connection", keep_alive_value);
    serv_->update_timeout(this);
    boost::asio::async_write(*socket_, resp->to_buffers(), 
        boost::bind(&connection::__write_data_handler, this, _1, _2));
}

void connection::close()
{
    if(closed_)
        return;
    err_code_t ec;
    socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    if(ec)
        errno_ = ec;
    closed_ = 1;
}

connection::connection(boost::asio::ip::tcp::socket* socket, 
    HttpServer* serv, bool keep_alive): 
    socket_(socket), request_(NULL), buffer_(NULL), 
    serv_(serv), keep_alive_(keep_alive), 
    closed_(0), request_parse_(NULL)
{
}

connection::~connection()
{
    close();
    if(socket_)
    {
        delete socket_;
        socket_ = NULL;
    }
    if(request_)
    {
        delete request_;
        request_ = NULL;
        delete request_parse_;
        request_parse_ = NULL;
    }
    if(buffer_)
    {
        delete buffer_;
        buffer_ = NULL;
    }
}

}
}
