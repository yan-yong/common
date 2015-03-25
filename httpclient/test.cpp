#include <stdio.h>
#include <unistd.h>

int main()
{
    while(true)
    {
        fprintf(stderr, "1");
        sleep(1);
    }
}
