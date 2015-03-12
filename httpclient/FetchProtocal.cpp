#include "FetchProtocal.hpp"

bool FetcherResponse::IsKeepAlive() const
{
    return true;
}

void FetcherRequest::Close()
{
    RequestData::vector = &iov_[0];
    RequestData::count = iov_.size();
    // DEBUG: output request
    // writev(STDOUT_FILENO, vector, count);
}

void FetcherRequest::Clear()
{
    iov_.clear();
}

FetcherRequest & FetcherRequest::FillVector(const void* data, size_t length)
{
    iovec iov;
    iov.iov_base = reinterpret_cast<char*>(const_cast<void*>(data));
    iov.iov_len = length;
    iov_.push_back(iov);

    return *this;
}

FetcherRequest & FetcherRequest::FillVector(const char* str)
{
    return FillVector(str, strlen(str));
}

FetcherRequest & FetcherRequest::FillVector(const std::string& str)
{
    return FillVector(str.data(), str.length());
}
