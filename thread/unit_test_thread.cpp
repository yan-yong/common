#include "ProcessPool.h"
#include "Thread.h"
#include "DataPool.h"

void data_pool_runtine(DataPool<int>* pool)
{
    int aa;
    while(pool->dequeue(aa))
       fprintf(stderr, "data_pool_runtine: %d\n", aa);
}

int main()
{
    //MyThread thread;
    //sleep(5);
    //ProcessPool* thread_pool = ProcessPool::Instance();
    //thread_pool->set_thread_num(20);
    //thread_pool->open();
    DataPool<int> data_pool;
    data_pool.initialize(5,100);
    boost::function<void (void)> runtine = boost::bind(data_pool_runtine, &data_pool);
    data_pool.set_runtine(runtine);
    for(int i = 0; i < 10; i++)
        data_pool.enqueue(i);
    data_pool.open();
    data_pool.exit();
    return 0;
}
