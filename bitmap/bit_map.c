#include "bitops.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "stdint.h" 
#include <string.h>

#define BITOP_WORD(nr)      ((nr) / BITS_PER_LONG)

typedef const unsigned long __attribute__((__may_alias__)) long_alias_t;

int test_bitmap(long nr, void* addr)
{
    if(variable_test_bit(nr, addr) != 0)
        return 1;
    return 0;
}

/* return 0 indicate set success, else duplicate */
int set_bitmap(long nr, void* addr)
{
    return test_and_set_bit(nr, addr);
}

unsigned long find_first_bit(const unsigned long *addr, unsigned long size)
{
    long_alias_t *p = (long_alias_t *) addr;
    unsigned long result = 0;
    unsigned long tmp;

    while (size & ~(BITS_PER_LONG-1)) {
        if ((tmp = *(p++)))
            goto found;
        result += BITS_PER_LONG;
        size -= BITS_PER_LONG;
    }
    if (!size)
        return result;

    tmp = (*p) & (~0UL >> (BITS_PER_LONG - size));
    if (tmp == 0UL)     /* Are any bits set? */
        return result + size;   /* Nope. */
found:
    return result + __ffs(tmp);
}

/*
 * Find the next set bit in a memory region.
 */
unsigned long find_next_bit(const unsigned long *addr, unsigned long size,
                unsigned long offset)
{
    const unsigned long *p = addr + BITOP_WORD(offset);
    unsigned long result = offset & ~(BITS_PER_LONG-1);
    unsigned long tmp;

    if (offset >= size)
        return size;
    size -= result;
    offset %= BITS_PER_LONG;
    if (offset) {
        tmp = *(p++);
        tmp &= (~0UL << offset);
        if (size < BITS_PER_LONG)
            goto found_first;
        if (tmp)
            goto found_middle;
        size -= BITS_PER_LONG;
        result += BITS_PER_LONG;
    }
    while (size & ~(BITS_PER_LONG-1)) {
        if ((tmp = *(p++)))
            goto found_middle;
        result += BITS_PER_LONG;
        size -= BITS_PER_LONG;
    }
    if (!size)
        return result;
    tmp = *p;

found_first:
    tmp &= (~0UL >> (BITS_PER_LONG - size));
    if (tmp == 0UL)     /* Are any bits set? */
        return result + size;   /* Nope. */
found_middle:
    return result + __ffs(tmp);
}

/*
 * This implementation of find_{first,next}_zero_bit was stolen from
 * Linus' asm-alpha/bitops.h.
 */
unsigned long find_next_zero_bit(const unsigned long *addr, unsigned long size,
                 unsigned long offset)
{
    const unsigned long *p = addr + BITOP_WORD(offset);
    unsigned long result = offset & ~(BITS_PER_LONG-1);
    unsigned long tmp;

    if (offset >= size)
        return size;
    size -= result;
    offset %= BITS_PER_LONG;
    if (offset) {
        tmp = *(p++);
        tmp |= ~0UL >> (BITS_PER_LONG - offset);
        if (size < BITS_PER_LONG)
            goto found_first;
        if (~tmp)
            goto found_middle;
        size -= BITS_PER_LONG;
        result += BITS_PER_LONG;
    }
    while (size & ~(BITS_PER_LONG-1)) {
        if (~(tmp = *(p++)))
            goto found_middle;
        result += BITS_PER_LONG;
        size -= BITS_PER_LONG;
    }
    if (!size)
        return result;
    tmp = *p;

found_first:
    tmp |= ~0UL << size;
    if (tmp == ~0UL)    /* Are any bits zero? */
        return result + size;   /* Nope. */
found_middle:
    return result + ffz(tmp);
}

unsigned long find_first_zero_bit(const unsigned long *addr, unsigned long size)
{
    const unsigned long *p = addr;
    unsigned long result = 0;
    unsigned long tmp;

    while (size & ~(BITS_PER_LONG-1)) {
        if (~(tmp = *(p++)))
            goto found;
        result += BITS_PER_LONG;
        size -= BITS_PER_LONG;
    }
    if (!size)
        return result;

    tmp = (*p) | (~0UL << size);
    if (tmp == ~0UL)    /* Are any bits zero? */
        return result + size;   /* Nope. */
found:
    return result + ffz(tmp);
}

unsigned long count_word_bit(void* addr, unsigned long byte_num)
{
    unsigned long cnt = 0;
    while(byte_num > 0){
        unsigned count_byte_cnt = byte_num > 8 ? 8:byte_num;
        uint64_t val = 0;
        memcpy(&val, addr, count_byte_cnt);
        addr += count_byte_cnt;
        byte_num -= count_byte_cnt;
        while(val){
            val &= (val - 1);
            cnt++;
        }
    }
    return cnt;
}

int clear_bitmap(long nr, void* addr)
{
    return test_and_clear_bit(nr, addr) == 0;
}

