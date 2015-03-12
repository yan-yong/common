include $(top_srcdir)/common.mk

AM_CPPFLAGS=-I$(boost_path)/include -I$(libev_path)/include -I$(top_srcdir)
AM_LDFLAGS=-rdynamic -lpthread -L$(top_srcdir)/_lib -llog

source_list=Channel.cpp HttpClient.cpp SchedulerTypes.cpp TFetchResult.cpp TUtility.cpp FetchProtocal.cpp HttpFetchProtocal.cpp TRedirectChecker.cpp

lib_LTLIBRARIES=libhttp_client.la
libhttp_client_la_SOURCES=$(source_list)
