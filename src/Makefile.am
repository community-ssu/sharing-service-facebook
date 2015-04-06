AM_CFLAGS = -Wall -fPIC $(DEPS_CFLAGS)

plugins_LTLIBRARIES = libfacebooksharingservice.la

libfacebooksharingservice_la_SOURCES = service.c places.c
libfacebooksharingservice_la_CFLAGS = $(DEPS_CFLAGS) $(DEBUG_CFLAGS)
libfacebooksharingservice_la_LDFLAGS = -module -avoid-version -export-dynamic
libfacebooksharingservice_la_LIBADD = $(DEPS_LIBS)

MAINTAINERCLEANFILES = Makefile.in
