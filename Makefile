CC = gcc
CXX = g++

INCLUDES =

CFLAGS = -g -Wall $(INCLUDES)
CXXFLAGS = -g -Wall $(INCLUDES)

LDFLAGS = -g
LDLIBS = -lm -ledit

# Implicit make rules, in lieu of specifying linking rules:
#
# $(CC) $(LDFLAGS) <all-dependent-.o-files> $(LDLIBS)
#
#

lispy: lispy.o mpc.o

# Implicit rules to build .o files:
#
# $(CC) -c $(CFLAGS) <the-.c-file>
#
#

lispy.o: lispy.c mpc.h

mpc.o: mpc.c mpc.h

# Always provide the "clean" target that removes intermediate files.
# What you remove depend on your choice of coding tools
# (different editors generate different backup files for example).
# # And the "clean" target is not a file name, so we tell make that
# it's a "phony" target.

.PHONY: clean
clean:
	rm -f *.o *~ a.out core lispy

# "all" target is useful if your Makefile builds multiple programs.
# Here we'll have it first do "clean", and rebuild the main target.

.PHONY: all
all: clean lispy
