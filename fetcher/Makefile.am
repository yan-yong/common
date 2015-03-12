include $(top_srcdir)/common.mk

AM_CPPFLAGS=-DENABLE_SSL
AM_LDFLAGS=-rdynamic -lpthread 
source_list=httpserver.cpp  mime_types.cpp  reply.cpp  request_parser.cpp  server.cpp

lib_LTLIBRARIES=libfetcher.la
libfetcher_la_SOURCES=Fetcher.cpp
