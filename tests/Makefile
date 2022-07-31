OBJS		= chunk.o kvfs.o \
			  driver_memcache.o driver_file.o driver_dns.o

CPPFLAGS	= -I..
CXXFLAGS	= -g -std=c++11 -Wall -Wpedantic -Werror
LDFLAGS		= -L..
LIBS		= -lkvfs -lmemcached -lldns -lUnitTest++

all:		test

test:		test.o $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS) $(LIBS)

check:		test
	./test

clean:
	$(RM) *.o
