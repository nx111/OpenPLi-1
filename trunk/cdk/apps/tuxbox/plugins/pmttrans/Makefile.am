AM_CPPFLAGS = \
	-I$(top_srcdir)/include 

bin_PROGRAMS = pmttrans

pmttrans_SOURCES = pmttrans.c
pmttrans_CXXFLAGS = $(SIGC_CFLAGS) 
pmttrans_LDADD = -lpthread @SIGC_LIBS@

