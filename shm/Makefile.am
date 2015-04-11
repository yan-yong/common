include $(top_srcdir)/common.mk

AM_CPPFLAGS=-I$(top_srcdir)

AM_LDFLAGS=-rdynamic -lpthread -L$(top_srcdir)/_lib -llog -lz -lrt -lutility -lbitmap

LDADD=$(boost_path)/lib/libboost_system.a $(boost_path)/lib/libboost_thread.a

sbin_PROGRAMS=test_share_hashset
test_share_hashset_SOURCES=test_hash_set.cpp
