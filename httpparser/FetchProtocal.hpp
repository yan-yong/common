#ifndef FETCHKERNEL_FETCHPROTOCAL_
#define FETCHKERNEL_FETCHPROTOCAL_

#include <string>
#include <vector>
#include "fetcher/Fetcher.hpp"

class FetcherResponse : public IFetchMessage
{
    public:
	FetcherResponse()
	{
	}
	virtual ~FetcherResponse()
	{
	}
    	/**
	 * Called when new data recieved on the cresponding connection,
	 * this is a chance when the application can save data of the cresponding fetch request.
	 *
	 * @param nbytes Number of bytes recieved. If nbytes == 0, the connection is closed.
	 *
	 * @return >0 The Fetch kernel should continue recieve data
	 * 	   ==0 A complete message has recieved, 
	 * 	   	the kernel can call KeepAlive to determin if the connection can be closed.
	 * 	   <0 Some error happend, implement of this method must set errno probably.
	 */

	virtual int Append(const void *buf, size_t length) = 0;
	virtual bool IsKeepAlive() const;
};

class FetcherRequest : public RequestData
{
     public:
	virtual ~FetcherRequest()
	{
	}
	virtual void Close();
	virtual void Clear();

	FetcherRequest & FillVector(const void* data, size_t length);
	FetcherRequest & FillVector(const char* str);
	FetcherRequest & FillVector(const std::string& str);

    private:
	friend class Fetcher;
	std::vector<iovec> iov_;
};

#endif
