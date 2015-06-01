//
// request.hpp
// ~~~~~~~~~~~
//
// Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef HTTP_SERVER4_REQUEST_HPP
#define HTTP_SERVER4_REQUEST_HPP

#include <string>
#include <vector>
#include <boost/algorithm/string/trim.hpp>
#include <boost/lexical_cast.hpp>
#include <cctype>
#include <algorithm>
#include <boost/asio.hpp>
#include "header.hpp"
#include "httpparser/Http.hpp"
#include "httpparser/HttpMessage.hpp"

namespace http {
namespace server4 {

/// A request received from a client.
struct request
{
    /// The request method, e.g. "GET", "POST".
    std::string method;

    /// The requested URI, such as a path to a file.
    std::string uri;

    /// Major version number, usually 1.
    int http_version_major;

    /// Minor version number, usually 0 or 1.
    int http_version_minor;

    /// version string
    std::string http_version;

    /// The headers included with the request.
    //std::vector<header> headers;
    MessageHeaders headers;

    /// The optional content sent with the request.
    std::vector<char> content;

    std::string get_path() const
    {
        size_t beg_idx = uri.find("://");
        if(beg_idx == std::string::npos)
            return uri;
        beg_idx += strlen("://");
        size_t end_idx = uri.find("/", beg_idx);
        if(end_idx == std::string::npos || end_idx == uri.size() - 1)
            return "/";
        return uri.substr(end_idx);
    }

    std::string get_host() const
    {
        std::string host;
        if(headers.FindValue("Host", host))
            return host;
        size_t beg_idx = uri.find("://");
        if(beg_idx == std::string::npos)
            return "";
        beg_idx += strlen("://");
        size_t end_idx = uri.find("/");
        if(end_idx == std::string::npos)
            return uri.substr(beg_idx);
        return uri.substr(beg_idx, end_idx - beg_idx);
    }

    std::vector<std::string> get_header(const std::string& header)
    {
        std::string header_name = header;
        boost::trim(header_name);
        transform(header_name.begin(), header_name.end(), header_name.begin(), tolower);
        return headers.FindAllValue(header_name);
    }

    int decompress()
    {
        std::string encoding;
        if(!headers.FindValue("Content-Encoding", encoding))
            return 0;

        if (encoding == "gzip")
        {   
            std::vector<char> buffer;
            if (GzipUncompress(&content[0], content.size(), buffer) == 0) 
            {
                content.assign(&buffer[0], &buffer[0] + buffer.size());
                return 0;
            }   
            return -1; 
        }

        if(encoding == "deflate")
        {    
            std::vector<char> buffer;
            if (DeflateUncompress(&content[0], content.size(), buffer) == 0)
            {
                content.assign(&buffer[0], &buffer[0] + buffer.size()); 
                return 0;
            }
            return -2;
        }

        return -3;
    }

    std::vector<boost::asio::const_buffer> to_buffers()
    {
        static const char name_value_separator[] = { ':', ' ' };
        static const char crlf[] = { '\r', '\n' };
        std::vector<boost::asio::const_buffer> buffers;
        buffers.push_back(boost::asio::buffer(method));
        buffers.push_back(boost::asio::buffer(uri));
        buffers.push_back(boost::asio::buffer(http_version));
        for (std::size_t i = 0; i < headers.Size(); ++i)
        {
            MessageHeader& h = headers[i];
            buffers.push_back(boost::asio::buffer(h.Name));
            buffers.push_back(boost::asio::buffer(name_value_separator));
            buffers.push_back(boost::asio::buffer(h.Value));
            buffers.push_back(boost::asio::buffer(crlf));
        }
        buffers.push_back(boost::asio::buffer(crlf));
        buffers.push_back(boost::asio::buffer(content));
        return buffers;
    }
};

} // namespace server4
} // namespace http

#endif // HTTP_SERVER4_REQUEST_HPP
