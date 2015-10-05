#  $Id: Makefile.am 356 2011-01-05 00:10:50Z babic $

CXXFLAGS=$(cxxflags)
CCFLAGS=$(ccflags)

bin_PROGRAMS 		= sdgchecker sdginfer
lib_LTLIBRARIES		= libtinf.la

# List all sources here
libtinf_la_SOURCES		= \
	HashFunctions.cpp	Rand.cpp			Utilities.cpp \
	Term.cpp			TermVisitor.cpp		DotDump.cpp \
	CSE.cpp				Parser.cpp			DependencyParser.cpp \
	CClosure.cpp		BuildAutomaton.cpp	Acceptor.cpp

libtinf_la_CPPFLAGS = $(BOOST_CPPFLAGS)

sdgchecker_CPPFLAGS 	= $(BOOST_CPPFLAGS)
sdgchecker_SOURCES 		= Checker.cpp
# -static flag makes things more convenient for debugging and startup
# time faster, but slows down compilation.
sdgchecker_LDFLAGS		= -static
sdgchecker_LDADD		= libtinf.la $(BOOST_PROGRAM_OPTIONS_LIBS) $(BOOST_FILESYSTEM_LIBS)

sdginfer_CPPFLAGS			= $(BOOST_CPPFLAGS)
sdginfer_SOURCES			= SDGInference.cpp
sdginfer_LDFLAGS			= -static
sdginfer_LDADD				= libtinf.la $(BOOST_PROGRAM_OPTIONS_LIBS) $(BOOST_FILESYSTEM_LIBS)
