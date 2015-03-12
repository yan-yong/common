include $(top_srcdir)/common.mk

AM_CPPFLAGS=-I$(boost_path)/include -I$(top_srcdir)
AM_LDFLAGS=-rdynamic -lpthread -L$(top_srcdir)/_lib -llog -lrt

LDADD=$(boost_path)/lib/libboost_system.a $(boost_path)/lib/libboost_thread.a
LIBADD=$(boost_path)/lib/libboost_system.a $(boost_path)/lib/libboost_thread.a

lib_LTLIBRARIES=libcthread.la
libcthread_la_SOURCES=ProcessPool.cpp Thread.cpp 

bin_PROGRAMS=test_thread
test_thread_SOURCES=unit_test_thread.cpp ProcessPool.cpp Thread.cpp
test_thread_CPPFLAGS=$(AM_CPPFLAGS)
