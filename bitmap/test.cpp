#include <stdio.h>
#include "src/Judy.h"

int main()                       // Example program of Judy1 macro APIs
{
    Word_t Index;                 // index (or key)
    Word_t Rcount;                // count of indexes (or bits set)
    Word_t Rc_word;               // full word return value
    int    Rc_int;                // boolean values returned (0 or 1)

    printf("%zd\n", sizeof(Word_t));

    Pvoid_t J1Array = (Pvoid_t) NULL; // initialize Judy1 array

    Index = 123456;
    J1S(Rc_int, J1Array, Index);  // set bit at 123456
    Index = 123458;
    J1S(Rc_int, J1Array, Index);  // set bit at 123456
    if (Rc_int == JERR) goto process_malloc_failure;
    if (Rc_int == 1) printf("OK - bit successfully set at %lu\n", Index);
    if (Rc_int == 0) printf("BUG - bit already set at %lu\n", Index);

    Index = 654321;
    J1T(Rc_int, J1Array, Index);  // test if bit set at 654321
    if (Rc_int == 1) printf("BUG - set bit at %lu\n", Index);
    if (Rc_int == 0) printf("OK - bit not set at %lu\n", Index);

    J1C(Rcount, J1Array, 0, -1);  // count all bits set in array
    printf("%lu bits set in Judy1 array\n", Rcount);

    Index = 0;
    J1F(Rc_int, J1Array, Index);  // find first bit set in array
    if (Rc_int == 1) printf("111 OK - first bit set is at %lu\n", Index);
    Index = 123457;
    J1F(Rc_int, J1Array, Index);  // find first bit set in array
    if (Rc_int == 1) printf("222 OK - first bit set is at %lu\n", Index);
    if (Rc_int == 0) printf("BUG - no bits set in array\n");

    J1MU(Rc_word, J1Array);       // how much memory was used?
    printf("%lu Indexes used %lu bytes of memory\n", Rcount, Rc_word);

    Index = 123456;
    J1U(Rc_int, J1Array, Index);  // unset bit at 123456
    if (Rc_int == JERR) goto process_malloc_failure;
    if (Rc_int == 1) printf("OK - bit successfully unset at %lu\n", Index);
    if (Rc_int == 0) printf("BUG - bit was not set at %lu\n", Index);
    J1FA(Rc_word, J1Array);

process_malloc_failure:

    return(0);
}
