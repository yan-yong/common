#ifndef _DATAQUEUE_H_
#define _DATAQUEUE_H_

#include <list>
#include <string>
#include <boost/thread/mutex.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>

typedef boost::mutex::scoped_lock scoped_lock;
using namespace std;

class CDataQueue
{
public:
	CDataQueue();
	~CDataQueue();

	int GetSize();
	void PushDataFront(string& data);
	void PushDataBack(string& data);
	string GetData();

	int m_size;
	list<string> m_data;
	boost::mutex m_mutex;
	boost::interprocess::interprocess_semaphore m_semaphore;
};

#endif