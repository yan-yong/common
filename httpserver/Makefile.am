include $(top_srcdir)/common.mk

AM_CPPFLAGS=-I$(boost_path)/include -I$(top_srcdir)
AM_LDFLAGS=-rdynamic -lpthread -L$(top_srcdir)/_lib -llog

LDADD=$(boost_path)/lib/libboost_system.a
LIBADD=$(boost_path)/lib/libboost_system.a

source_list=httpserver.cpp  mime_types.cpp  reply.cpp  request_parser.cpp  server.cpp

sbin_PROGRAMS=test_httpserver
test_httpserver_SOURCES=test_httpserver.cpp $(source_list) 
test_httpserver_CPPFLAGS=$(AM_CPPFLAGS)

lib_LTLIBRARIES=libhttpserver.la
libhttpserver_la_SOURCES=$(source_list)
