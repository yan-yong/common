include $(top_srcdir)/common.mk

AM_CPPFLAGS=-I$(libev_path)/include -I$(boost_path)/include
AM_LDFLAGS=-rdynamic -lpthread -lrt

LDADD=$(libev_path)/lib/libevent.a
LIBADD=$(libev_path)/lib/libevent.a

sbin_PROGRAMS=test_dns_resolver
test_dns_resolver_SOURCES=DNSResolver.cpp unit_test_dns_resolver.cpp
test_dns_resolver_CPPFLAGS=$(AM_CPPFLAGS)

lib_LTLIBRARIES=libdns_resolver.la
libdns_resolver_la_SOURCES=DNSResolver.cpp
