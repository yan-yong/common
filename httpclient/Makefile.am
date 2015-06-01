include $(top_srcdir)/common.mk

AM_CPPFLAGS=-I$(boost_path)/include -I$(libev_path)/include -I$(top_srcdir)
AM_LDFLAGS=-rdynamic -lpthread -L$(top_srcdir)/_lib -llog -lhttpparser -lz -lcrypto -lrt -lfetcher -lutility -lssl -ldns_resolver

LDADD=$(boost_path)/lib/libboost_system.a $(boost_path)/lib/libboost_thread.a $(libev_path)/lib/libevent.a

source_list=HttpClient.cpp SchedulerTypes.cpp TRedirectChecker.cpp ChannelManager.cpp Storage.cpp Resource.cpp

lib_LTLIBRARIES=libhttp_client.la
libhttp_client_la_SOURCES=$(source_list)

sbin_PROGRAMS=test_httpclient
test_httpclient_SOURCES=unit_test_httpclient.cpp $(source_list) 
test_httpclient_CPPFLAGS=$(AM_CPPFLAGS)
