
#include "DataQueue.h"

CDataQueue g_downloadDataQueue;

CDataQueue::CDataQueue() : m_semaphore(0), m_size(0)
{
}

CDataQueue::~CDataQueue()
{
}

int CDataQueue::GetSize()
{
	scoped_lock lock(m_mutex);
	return m_size;
}

void CDataQueue::PushDataFront(string& data)
{
	scoped_lock lock(m_mutex);
	m_data.push_front(data);
	m_semaphore.post();
}

void CDataQueue::PushDataBack(string& data)
{
	scoped_lock lock(m_mutex);
	m_data.push_back(data);
	m_semaphore.post();
}

string CDataQueue::GetData()
{
	m_semaphore.wait();
	scoped_lock lock(m_mutex);
	string content = m_data.front();
	m_data.pop_front();
	return content;
}

