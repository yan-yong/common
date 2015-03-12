/**
 *    \file   HttpFetchProtocal.hpp
 *    \brief  Handle http protocal issues.
 *    \date   2011年08月09日
 *    \author guoshiwei (guoshiwei@gmail.com)
 */

#ifndef  HTTPFETCHPROTOCAL_INC
#define  HTTPFETCHPROTOCAL_INC

#include <sys/socket.h>
#include <errno.h>
#include <stdint.h>
#include "Http.hpp"
#include "FetchProtocal.hpp"

enum
{
    PROTOCOL_HTTP = 0,
    PROTOCOL_HTTPS = 1
};

#define protocal2str(protocal) (protocal == PROTOCOL_HTTPS ? "https":"http");
#define str2protocal(scheme)   (scheme == "https" ? PROTOCOL_HTTPS:PROTOCOL_HTTP)

class HttpFetcherRequest : public FetcherRequest, public Http::Request
{
    public:
	virtual ~HttpFetcherRequest(){};
	virtual void Clear()
	{   
	    FetcherRequest::Clear();
	    Http::Request::Clear();
	}
	virtual void Close();
};

class HttpFetcherResponse : public FetcherResponse, public Http::Response
{
    public:
	HttpFetcherResponse(
		const sockaddr* remote_addr,
		size_t remote_addrlen,
		const sockaddr* local_addr,
		size_t local_addrlen,
		size_t max_body_size,
		size_t truncate_size
		):
	    m_OriginalSize(0),
	    m_MaxBodySize(max_body_size),
	    m_TruncateSize(truncate_size),
	    m_Truncated(false),
	    m_RemoteAddress(),
	    m_LocalAddress(),
	    m_HeadersSize(0),
	    m_ContentLength(-1),
	    m_Chunked(false)
	{
	    assert(remote_addrlen <= sizeof(m_RemoteAddress));
	    memcpy(&m_RemoteAddress, remote_addr, remote_addrlen);
	    if (local_addrlen > 0) {
		memcpy(&m_LocalAddress, local_addr, local_addrlen);
	    }
	}

	int ContentEncoding(char error_msg[50]);
	virtual int Append(const void *buf, size_t length);
	int __Append(const void *buf, size_t length);

	int AppendBody(const void *buf, size_t length);

	virtual int OnHeadersComplete();

	virtual int OnTransferComplete()
	{
	    return 0;
	}
	//原始大小
	size_t MessageSize() const
	{
	    return m_OriginalSize ? m_OriginalSize:m_HeadersSize + Body.size();
	}

	void SetRemoteAddress(const sockaddr* addr, size_t addrlen) 
	{
	    assert(addrlen <= sizeof(m_RemoteAddress));
	    memcpy(&m_RemoteAddress, addr, addrlen);
	}

	const sockaddr& RemoteAddress() const
	{
	    return reinterpret_cast<const sockaddr&>(m_RemoteAddress);
	}

	void SetLocalAddress(const sockaddr* addr, size_t addrlen) 
	{
	    assert(addrlen <= sizeof(m_LocalAddress));
	    memcpy(&m_LocalAddress, addr, addrlen);
	}

	const sockaddr& LocalAddress() const
	{
	    return reinterpret_cast<const sockaddr&>(m_LocalAddress);
	}

	bool SizeExceeded() const
	{
    	    return (m_ContentLength > 0 && (size_t)m_ContentLength > m_MaxBodySize) || Body.size() > m_MaxBodySize || m_UnparsedData.size() > m_MaxBodySize;
	}

	bool IsTruncated()
	{
	    return m_Truncated;
	}

	std::string DumpResponseData() const {
	    return m_DumpResponseData;
	}

    private:
	size_t m_OriginalSize;
	size_t m_MaxBodySize;
	size_t m_TruncateSize;
	bool m_Truncated;
	sockaddr_storage m_RemoteAddress;
	sockaddr_storage m_LocalAddress;
	std::string m_UnparsedData;
	std::string m_DumpResponseData;
	size_t m_HeadersSize;
	int m_ContentLength;
	bool m_Chunked;
};

bool IsHttpDefaultPort(int protocol, uint16_t port);
uint16_t GetHttpDefaultPort(int protocol, uint16_t port);

#endif   /* ----- #ifndef HTTPFETCHPROTOCAL_INC  ----- */
