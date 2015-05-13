include $(top_srcdir)/common.mk

AM_CPPFLAGS=-I$(boost_path)/include -I$(top_srcdir)
AM_LDFLAGS=-rdynamic -lpthread -L$(top_srcdir)/_lib -llog

LDADD=$(boost_path)/lib/libboost_system.a
LIBADD=$(boost_path)/lib/libboost_system.a

source_list=net_utility.cpp string_utility.cpp ip_location.cpp

lib_LTLIBRARIES=libutility.la
libutility_la_SOURCES=$(source_list)

sbin_PROGRAMS=test_ip_locate
test_ip_locate_SOURCES=test_ip_locate.cpp $(source_list) 
test_ip_locate_CPPFLAGS=$(AM_CPPFLAGS)
