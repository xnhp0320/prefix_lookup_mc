#
# Copyright (c) 2015 Hirochika Asai <asai@jar.jp>
# All rights reserved.
#

AM_CFLAGS=-O3
bin_PROGRAMS = tbm_test
lib_LTLIBRARIES = libfib.la
libfib_la_SOURCES = lib/bitmap.h \
					lib/bitmap.c \
					lib/bitmap_v4.h \
					lib/bitmap_v4.c \
					lib/list.h \
					lib/mb_node.h \
					lib/mb_node.c \
					lib/mm.h \
					lib/mm.c \
					lib/mm_color.h \
					lib/mm_color.c \
					lib/bitmap_v6.c \
					lib/bitmap_v6.h 

libfib_la_CFLAGS = -msse4.2 -g -Wall -O3

tbm_test_SOURCES= tbm/tbm.h \
			 tbm/tbmv4.h \
			 tbm/tbmv4.c \
			 tbm/tbmv6.c \
			 tbm/test.c \
			 tbm/tbmv6.h 

tbm_test_LDADD = libfib.la
tbm_test_DEPENDENCIES = libfib.la
tbm_test_CFLAGS = -DTEST_CHECK

AUTOMAKE_OPTIONS = subdir-objects

CLEANFILES = *~
