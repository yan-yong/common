include $(top_srcdir)/common.mk

AM_CPPFLAGS= -I$(boost_path)/include -I$(top_srcdir)
AM_LDFLAGS=-rdynamic -lpthread -L$(top_srcdir)/_lib -lJudy -llog -lrt

LDADD=$(boost_path)/lib/libboost_system.a $(boost_path)/lib/libboost_thread.a
LIBADD=$(boost_path)/lib/libboost_system.a $(boost_path)/lib/libboost_thread.a

lib_LTLIBRARIES=libbitmap.la
libbitmap_la_SOURCES=JudyBitmap.cpp DenseBitmap.cpp bit_map.c 

bin_PROGRAMS=test_judy_bitmap
test_judy_bitmap_SOURCES=unit_test_judy_bitmap.cpp JudyBitmap.cpp
test_judy_bitmap_CPPFLAGS=$(AM_CPPFLAGS)

bin_PROGRAMS+=test_dense_bitmap
test_dense_bitmap_SOURCES=unit_test_dense_bitmap.cpp DenseBitmap.cpp bit_map.c
test_dense_bitmap_CPPFLAGS=$(AM_CPPFLAGS)
