#ifndef PTHREAD_HPP_INCLUDED
#define PTHREAD_HPP_INCLUDED

#include <errno.h>
#include <assert.h>

#include <stdexcept>

#include <pthread.h>
#include <string.h>
class pthread_error : public std::runtime_error
{
public:
	pthread_error(int error, const std::string& info = "pthread"):
		std::runtime_error(info + std::string(": ") + strerror(error)),
		m_error(error)
	{
	}
	int code() const { return m_error; }
private:
	int m_error;
};

class pthread_mutex
{
public:
	class attribute
	{
	public:
		attribute()
		{
			int r = pthread_mutexattr_init(&m_attr);
			if (r!=0)
				throw pthread_error(r, "pthread_mutex::attribute::attribute()");
		}
		void set_type(int type)
		{
			int r = pthread_mutexattr_settype(&m_attr, type);
			if (r!=0)
				throw pthread_error(r, "pthread_mutex::attribute::set_type()");
		}
		int get_type() const
		{	
			int type;
			int r = pthread_mutexattr_gettype(&m_attr, &type);
			if (r!=0)
				throw pthread_error(r, "pthread_mutex::attribute::get_type()");
			return type;
		}
		~attribute()
		{
		#ifndef NDEBUG
			int r = pthread_mutexattr_destroy(&m_attr);
			assert(r==0); // dtor can't throw exception
		#else
			pthread_mutexattr_destroy(&m_attr);
		#endif
		}
	private: // forbid copying
		attribute(const attribute& src);
		attribute& operator=(const attribute& rhs);
	private:
		pthread_mutexattr_t m_attr;
	};
public:
	pthread_mutex()
	{
		pthread_mutex_init(&m_mutex, NULL);
	}
	pthread_mutex(int type)
	{
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, type);
		pthread_mutex_init(&m_mutex, &attr);
		pthread_mutexattr_destroy(&attr);
	}
	~pthread_mutex()
	{
		pthread_mutex_destroy(&m_mutex);
	}
	void lock()
	{
		pthread_mutex_lock(&m_mutex);
	}
	void unlock()
	{
		pthread_mutex_unlock(&m_mutex);
	}
	bool trylock()
	{
		int n = pthread_mutex_trylock(&m_mutex);
		if (n==0)
			return true;
		if (n==EBUSY)
			return false;
		throw pthread_error(n, "pthread_mutex::trylock");
	}
private: // forbid copying
	pthread_mutex(const pthread_mutex& src);
	pthread_mutex& operator=(const pthread_mutex& rhs);
private:
	pthread_mutex_t m_mutex;
};

template <typename Lock>
class auto_lock
{
public:
	auto_lock(Lock& lock) : m_lock(lock) { lock.lock(); }
	~auto_lock() { m_lock.unlock(); }
private: // forbid copying
	auto_lock(const auto_lock& src);
	auto_lock& operator=(const auto_lock& rhs);
private:
	Lock& m_lock;
};

template <typename Lock>
class try_lock
{
public:
	try_lock(Lock& lock) : m_lock(lock), m_locked(lock.trylock()) {}
	~try_lock() { if (m_locked) m_lock.unlock(); }
	bool locked() const { return m_locked; }
private: // forbid copying
	try_lock(const try_lock& src);
	try_lock& operator=(const try_lock& rhs);
private:
	Lock& m_lock;
	bool m_locked;
};

typedef auto_lock<pthread_mutex> pthread_auto_lock;
typedef try_lock<pthread_mutex> pthread_try_lock;


//////////////////////////////////////////////////////////////////////////
// Reader/Writer lock
class pthread_rwlock
{
public:
	pthread_rwlock()
	{
		pthread_rwlock_init(&m_lock, NULL);
	}
	~pthread_rwlock()
	{
		pthread_rwlock_destroy(&m_lock);
	}
	void reader_lock()
	{
		int n = pthread_rwlock_rdlock(&m_lock);
		if (n)
			throw pthread_error(n, "pthread_rwlock::reader_lock");
	}
	void reader_unlock()
	{
		pthread_rwlock_unlock(&m_lock);
	}
	void writer_lock()
	{
		int n = pthread_rwlock_wrlock(&m_lock);
		if (n)
			throw pthread_error(n, "pthread_rwlock::writer_lock");
	}
	void writer_unlock()
	{
		pthread_rwlock_unlock(&m_lock);
	}
private: // forbid copying
	pthread_rwlock(const pthread_rwlock& src);
	pthread_rwlock& operator=(const pthread_rwlock& rhs);
private:
	pthread_rwlock_t m_lock;
};

class pthread_auto_reader_lock
{
public:
	pthread_auto_reader_lock(pthread_rwlock& lock) : m_lock(lock)
	{
		m_lock.reader_lock();
	}
	~pthread_auto_reader_lock()
	{
		m_lock.reader_unlock();
	}
private: // forbid copying
	pthread_auto_reader_lock(const pthread_auto_reader_lock& src);
	pthread_auto_reader_lock& operator=(const pthread_auto_reader_lock& rhs);
private:
	pthread_rwlock& m_lock;
};

class pthread_auto_writer_lock
{
public:
	pthread_auto_writer_lock(pthread_rwlock& lock) : m_lock(lock)
	{
		m_lock.writer_lock();
	}
	~pthread_auto_writer_lock()
	{
		m_lock.writer_unlock();
	}
private: // forbid copying
	pthread_auto_writer_lock(const pthread_auto_writer_lock& src);
	pthread_auto_writer_lock& operator=(const pthread_auto_writer_lock& rhs);
private:
	pthread_rwlock& m_lock;
};


//////////////////////////////////////////////////////////////////////////
// pthread
class pthread
{
	pthread();
	~pthread();
	void join(void*&);
	void join();
	void yield();
	void suspend();
	void resume();
private:
	pthread_t m_tid;
};

#endif//PTHREAD_HPP_INCLUDED

