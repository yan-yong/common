#include "CQueue.h"
#include "CPriorityQueue.h"
#include <iostream>

int main(){
    CQueue<int> test_queue;
    LOG_DEBUG("create CQueue %p\n", &test_queue);
    boost::posix_time::time_duration timeout = boost::posix_time::microseconds(10000000);
    int a;
    test_queue.timed_dequeue(a, timeout) ;
    /*
    for(int i = 0; i < 10; i++)
        test_queue.enqueue(i);
    int val = 0;
    
    boost::posix_time::time_duration timeout = boost::posix_time::microseconds(1000000);
    while(test_queue.timed_dequeue(val, timeout)){
        fprintf(stderr, "CQueueTest: %d\n", val);
    }
    while(test_queue.timed_dequeue(val, timeout)){
        fprintf(stderr, "CPriorityTest: %d\n", val);
    }

    CPriorityQueue<int> test_prioiry_queue(50);
    LOG_DEBUG("create CPriorityQueue %p\n", &test_prioiry_queue);
    for(int i = 0; i < 10; i++)
        test_prioiry_queue.enqueue(rand()%5, i);

    while(test_prioiry_queue.timed_dequeue(val, timeout)){
        fprintf(stderr, "CPriorityTest: %d\n", val);
    }
    */
    return 0;
}
