CPPFLAGS	= -I..
CFLAGS		= -g -std=c99 -Wall -Wpedantic -Werror
LDFLAGS		= -L..
LIBS		= -lkvfs -lldns

all:		kvfs_upload_dns kvfs_download_dns \
			kvfs_upload_file kvfs_download_file

kvfs_upload_dns:		kvfs_upload_dns.o
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

kvfs_download_dns:		kvfs_download_dns.o
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

kvfs_upload_file:		kvfs_upload_file.o
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

kvfs_download_file:		kvfs_download_file.o
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

clean:
	$(RM) *.o
