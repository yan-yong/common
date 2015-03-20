/**
 *    \file   HttpFetchProtocal.cpp
 *    \brief  
 *    \date   2011年08月09日
 *    \author guoshiwei (guoshiwei@gmail.com)
 */

#include <assert.h>
#include "HttpFetchProtocal.hpp"

uint16_t GetHttpDefaultPort(int protocol)
{
    switch(protocol)
    {
        case PROTOCOL_HTTP:
            return HTTP_DEFAULT_PORT;
        case PROTOCOL_HTTPS:
            return HTTPS_DEFAULT_PORT;
        default:
            assert(false);
    }
}

bool IsHttpDefaultPort(int protocol, uint16_t port)
{
    return GetHttpDefaultPort(protocol) == port;
}

void HttpFetcherRequest::Close()
{
    // Request Line
    FillVector(Method);
    FillVector(" ");
    FillVector(Uri);
    if (!Version.empty())
    {
	FillVector(" ");
	FillVector(Version);
    }
    FillVector("\r\n");

    // Fill headers
    for (size_t i = 0; i < Headers.Size(); ++i)
    {
	FillVector(Headers[i].Name);
	FillVector(": ");
	FillVector(Headers[i].Value);
	FillVector("\r\n");
    }
    FillVector("\r\n");

    if (!Body.empty())
	FillVector(&Body[0], Body.size());
    FetcherRequest::Close();
}

void HttpFetcherRequest::Dump()
{
    static unsigned idx = 0;
    char file_name[100];
    snprintf(file_name, 100, "%u.dump", idx++);
    FILE* fid = fopen(file_name, "w"); 
    for(int i = 0; i < count; i++) 
        fwrite(vector[i].iov_base, vector[i].iov_len, 1, fid);
    fclose(fid);
}

int HttpFetcherResponse::OnHeadersComplete()
{
    int n = Headers.Find("Transfer-Encoding");
    if (n >= 0)
    {
	m_Chunked = strcasestr(Headers[n].Value.c_str(), "chunked");
	if (m_Chunked)
	{
	    for (size_t i = 0; i < Body.size(); ++i)
	    {
		if (Body[i] == '\r' || Body[i] == '\n' || Body[i] == ';'|| Body[i] == ' ')
		    break;

		// invalid chunked size
		if (!isxdigit(Body[i]))
		{
		    m_Chunked = false;
		    break;
		}
	    }
	}
    }

    n = Headers.Find("Content-Length");
    if (n >= 0)
	m_ContentLength = atoi(Headers[n].Value.c_str());
    return 1;
}

int HttpFetcherResponse::AppendBody(const void *buf, size_t length)
{
    if (m_Chunked)
    {
	m_UnparsedData.append((const char*)buf, length);
	if (!m_UnparsedData.empty())
	{
	    const char* begin = m_UnparsedData.c_str();
	    size_t size = m_UnparsedData.size();

	    bool parsed = false;
	    const void* data;
	    size_t data_size = 0;
	    size_t parsed_size = 0;
	    while ((parsed_size = ExtractChunkedData(begin, size, data, data_size)) > 0)
	    {
		Response::AppendBody(data, data_size);
		begin += parsed_size;
		size -= parsed_size;
		parsed = true;
	    }
	    if (parsed)
	    {
		m_UnparsedData.assign(begin, size);
		if (data_size == 0)
		    return 0;
	    }
	}
    }
    else
    {
	Response::AppendBody(buf, length);
	if ((int)Body.size() == m_ContentLength)
	    return 0;
    }

    return 1;
}

int HttpFetcherResponse::__Append(const void *buf, size_t length)
{
    m_DumpResponseData.append((const char*)buf, length);
    if (length == 0)
    {
	if (Response::Empty())
	{
	    errno = ECONNRESET;
	    return -1;
	}

	return OnTransferComplete();
    }

    const char* data = reinterpret_cast<const char*>(buf);
    // header not acquired
    if (Response::Empty())
    {
	size_t parsed_size = 0;
	// first time
	if (m_UnparsedData.empty())
	{
	    parsed_size = ParseResponse(data, length, *this);
	    if (!parsed_size)
		m_UnparsedData.assign(data, length);
	}
	else
	{
	    // merge previous data
	    m_UnparsedData.append(data, length);
	    parsed_size = ParseResponse(m_UnparsedData.data(), m_UnparsedData.length(), *this);
	}

	if (!Response::Empty())
	{
	    m_HeadersSize = parsed_size - Body.size();
	    m_UnparsedData.clear();
	    if (OnHeadersComplete() == 0)
		return 0;
	    if (m_Chunked)
	    {
		m_UnparsedData.assign(&Body[0], Body.size());
		Body.clear();
	    }
	    return AppendBody("", 0);
	}
	else
	{
	    // headers too large
	    const size_t MaxHeadersSize = 16 * 1024;
	    if (m_UnparsedData.size() > MaxHeadersSize)
	    {
		errno = EINVAL;
		return -1;
	    }
	}
    }
    else
    {
	return AppendBody(data, length);
    }
    return 1;
}

int HttpFetcherResponse::Append(const void *buf, size_t length)
{
    int result = __Append(buf, length);
    if (result < 0)
	return result;

    size_t body_size = Body.size();

    // if connection is closed by remote server, check response integrality
    // if (length == 0 && m_ContentLength > 0 && body_size < m_ContentLength)
    if (length == 0 && m_ContentLength > 0 && body_size == 0)
    {
	errno = ECONNRESET;
	return -1;
    }

    if (SizeExceeded())
    {
	//m_SizeExceeded = true;
	return 0;
    }

    if (body_size > m_TruncateSize)
    {
	Body.resize(m_TruncateSize);
	m_Truncated = true;
	return 0;
    }

    return result;
}

int HttpFetcherResponse::ContentEncoding(char error_msg[50]) {
    //NO Content-Encoding
    int index = Headers.Find("Content-Encoding");
    if (index < 0)
	return 0;
    //在解压之前，保存原始数据大小，用于记录抓取流量
    m_OriginalSize = m_HeadersSize + Body.size(); 
    //EMPTY_BODY
    if(Body.size() == 0){
	snprintf(error_msg, 50, "%s", "EMPTY_BODY"); 
	return -2;
    }
	
    /// handle Content-Encoding
    std::vector<char> buffer;
    if (Headers[index].Value == "gzip")
    {   
	if (GzipUncompress(&Body[0], Body.size(), buffer) == 0) {
	    std::swap(buffer, Body);
	    return 0;
	}
	snprintf(error_msg, 50, "%s", "GUNZIP_ERROR"); 
	return -3; 
    }    
    else if(Headers[index].Value == "deflate")
    {    
	if (DeflateUncompress(&Body[0], Body.size(), buffer) == 0){
	    std::swap(buffer, Body);
	    return 0;
	}
	//INFLATE_ERROR
	snprintf(error_msg, 50, "%s", "INFLATE_ERROR"); 
	return -4;
    }    
    // do nothing
    else if(Headers[index].Value == "none")
	return 0;

    //UNKNOWN_CONTENT_ENCODING
    snprintf(error_msg, 50, "%s", "UNKNOWN_CONTENT_ENCODING"); 
    return -5;
}

