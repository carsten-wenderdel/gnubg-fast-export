## $Id: Makefile.am,v 1.3 2017/08/10 19:26:49 plm Exp $
 
AM_CPPFLAGS = -I$(srcdir)/..
noinst_LTLIBRARIES = libneuralnet.la
libneuralnet_la_SOURCES = cache.h hash.c hash.h neuralnet.c neuralnet.h sse.h nsse.c sigmoid.h
AM_CFLAGS = $(SSE_CFLAGS)
