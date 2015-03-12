#ifndef GZIP_H
#define GZIP_H
#include "zlib.h"
 
/* Compress gzip data */
/* data 原数据 ndata 原数据长度 zdata 压缩后数据 nzdata 压缩后长度 */
inline int gzcompress(const void *src, size_t src_length, std::vector<char>& result)
{
    const int DEFAULT_RATE = 2;
    Bytef * data = (Bytef*)const_cast<void *>(src);
    uLong ndata = (uLong)src_length;
    result.clear();
    result.resize(DEFAULT_RATE * src_length);
    Bytef *zdata = (Bytef*)&result[0]; 

    z_stream c_stream;
    int err = 0;
    if(data && ndata > 0) {
        c_stream.zalloc = NULL;
        c_stream.zfree = NULL;
        c_stream.opaque = NULL;
        //只有设置为MAX_WBITS + 16才能在在压缩文本中带header和trailer
        if(deflateInit2(&c_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                        MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) return -1;
        c_stream.next_in   = data;
        c_stream.avail_in  = ndata;
        c_stream.next_out  = zdata;
        c_stream.avail_out = result.size();
        while(c_stream.avail_in != 0 && c_stream.total_out < result.size()) {
            if(deflate(&c_stream, Z_NO_FLUSH) != Z_OK) return -1;
        }
        if(c_stream.avail_in != 0) return c_stream.avail_in;
        for(;;) {
            if((err = deflate(&c_stream, Z_FINISH)) == Z_STREAM_END) break;
            if(err != Z_OK) return -1;
        }
        if(deflateEnd(&c_stream) != Z_OK) return -1;
        result.resize(c_stream.total_out);
        return 0;
    }
    return -1;
}

static int __inflate_data(z_stream& stream, const void *src, size_t src_length, std::vector<char>& result)
{
    const int DEFAULT_RATE = 5;
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<void*>(src));
    stream.avail_in = src_length;

    result.clear();
    result.resize(DEFAULT_RATE * src_length);

    for (;;)
    {
        stream.next_out = (Bytef*)&result[stream.total_out];
        stream.avail_out = result.size() - stream.total_out;

        int code = inflate(&stream, Z_NO_FLUSH);

        if (code == Z_STREAM_END)
            break;

        // if uncompress complete, ignore error
        if (stream.avail_in == 0)
            break;

        if (code < 0 || code == Z_NEED_DICT)
            return code;

        int rate = stream.total_in ? stream.total_out / stream.total_in : DEFAULT_RATE;
        result.resize(result.size() + (rate + 2) * stream.avail_in);
    }

    result.resize(stream.total_out);

    return Z_OK;
}
 
/* Uncompress gzip data */
/* zdata 数据 nzdata 原数据长度 data 解压后数据 ndata 解压后长度 */
inline int gzdecompress(const void *src, size_t src_length, std::vector<char>& result)
{
    z_stream stream = { 0 };

    int code = inflateInit2(&stream, 15 + 16);
    if (code == Z_OK)
    {
        code = __inflate_data(stream, src, src_length, result);
        inflateEnd(&stream);
    }

    return code;
}

#endif
