#include <stdio.h>

template<class T , int n>
class Test
{
public:
    T t[n];
};

int main()
{
    Test<int, 5> test;
    printf("%f %d\n", float(), test.t[2]);
}
