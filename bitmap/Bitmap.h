#ifndef __BITMAP_H
#define __BITMAP_H
#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_mutex.hpp>

typedef boost::mutex::scoped_lock scoped_lock;
typedef boost::shared_mutex RwMutex;
typedef boost::shared_lock<RwMutex> ReadLocker;
typedef boost::unique_lock<RwMutex> WriteLocker;

class Bitmap{
public:
    virtual int initialize(int order, std::string file_name = "") = 0;
    virtual int get(size_t digit) = 0;
    virtual int get(const std::string& data) = 0;
    virtual int set(size_t digit) = 0;
    virtual int set(const std::string& data) = 0;
    virtual int unset(size_t digit) = 0;
    virtual int unset(const std::string& data) = 0;
    virtual size_t size() = 0;
    virtual size_t bytes()= 0;
};
#endif
