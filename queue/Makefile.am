include $(top_srcdir)/common.mk

AM_CPPFLAGS=-I$(boost_path)/include -I$(top_srcdir)
AM_LDFLAGS=-rdynamic -lpthread -L$(top_srcdir)/_lib -llog

LDADD=$(boost_path)/lib/libboost_system.a $(boost_path)/lib/libboost_thread.a
LIBADD=$(boost_path)/lib/libboost_system.a $(boost_path)/lib/libboost_thread.a

sbin_PROGRAMS=queue_test
queue_test_SOURCES=unit_test_queue.cpp
