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
    std::vector<header> headers;

    /// The optional content sent with the request.
    std::string content;

    std::vector<std::string> get_header(const std::string& header)
    {
        std::vector<std::string> res;
        std::string header_name = header;
        boost::trim(header_name);
        transform(header_name.begin(), header_name.end(), header_name.begin(), tolower);
        for(unsigned i = 0; i < headers.size(); i++){
            std::string name = headers[i].name;
            boost::trim(name);
            transform(name.begin(), name.end(), name.begin(), tolower);
            if(name == header_name)
                res.push_back(headers[i].value);
        }
        return res;
    }

    int decompress()
    {
        std::vector<std::string> hd_vec = get_header("Content-Encoding");
        if(hd_vec.size() == 0)
            return 0;

        if (hd_vec[0] == "gzip")
        {   
            std::vector<char> buffer;
            if (GzipUncompress(&content[0], content.size(), buffer) == 0) 
            {
                content.assign(&buffer[0], &buffer[0] + buffer.size());
                return 0;
            }   
            return -1; 
        }

        if(hd_vec[0] == "deflate")
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
        for (std::size_t i = 0; i < headers.size(); ++i)
        {
            header& h = headers[i];
            buffers.push_back(boost::asio::buffer(h.name));
            buffers.push_back(boost::asio::buffer(name_value_separator));
            buffers.push_back(boost::asio::buffer(h.value));
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
