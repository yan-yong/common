include $(top_srcdir)/common.mk

AM_CPPFLAGS=-I$(boost_path)/include -I$(top_srcdir)
AM_LDFLAGS=-rdynamic -lpthread -L$(top_srcdir)/_lib -llog -lz -lhttpparser -lcrypto -lutility -ldns_resolver

LDADD=$(boost_path)/lib/libboost_system.a $(libev_path)/lib/libevent.a
LIBADD=$(boost_path)/lib/libboost_system.a $(libev_path)/lib/libevent.a

source_list=httpserver.cpp  mime_types.cpp  reply.cpp  request_parser.cpp connection.cpp

sbin_PROGRAMS=test_async_httpserver
test_async_httpserver_SOURCES=test_http_server.cpp $(source_list) 
test_async_httpserver_CPPFLAGS=$(AM_CPPFLAGS)

lib_LTLIBRARIES=libasync_httpserver.la
libasync_httpserver_la_SOURCES=$(source_list)
