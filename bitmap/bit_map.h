#ifndef BITMAP_H
#define BITMAP_H

extern "C" int test_bitmap(long nr, void* addr);
extern "C" int set_bitmap(long nr, void* addr);
extern "C" int clear_bitmap(long nr, void* addr);
extern "C" unsigned long find_first_bit(void *addr, unsigned long size);
extern "C" unsigned long find_next_bit(void *addr, unsigned long size, unsigned long offset);
extern "C" unsigned long find_next_zero_bit(void *addr, unsigned long size, unsigned long offset);
extern "C" unsigned long find_first_zero_bit(void *addr, unsigned long size);
extern "C" unsigned long count_word_bit(void* addr, unsigned long byte_num);
#endif
