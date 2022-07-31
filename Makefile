CPPFLAGS	= -I.
CFLAGS		= -g -Wall -Wpedantic -Wextra -Werror -Wno-pointer-sign
OBJS		= chunk.o kvfs.o kvfs_stdio.o \
			  drivers/memcache.o drivers/file.o drivers/dns.o
LIBS		=

all:		libkvfs.a

libkvfs.a:	$(OBJS)
	$(AR) rcv $(@) $^

clean:
	$(RM) $(OBJS) libkvfs.a
	(cd tests && make clean)
	(cd demo && make clean)
