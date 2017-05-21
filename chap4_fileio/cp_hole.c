#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define MYBUFSIZE 4096

static char *progname;
static char buf[MYBUFSIZE];

static void write_chunk(int in, int out, size_t size)
{
	ssize_t remain = size;

	while (remain) {
		ssize_t ntoread = remain >= MYBUFSIZE ? MYBUFSIZE : remain % MYBUFSIZE;
		ssize_t nread = read(in, buf, ntoread);
		if (nread != ntoread)
			err(EXIT_FAILURE, "read() failed");

		if (write(out, buf, nread) != nread)
			err(EXIT_FAILURE, "write() failed");

		remain -= nread;
	}
}

int main(int argc, char *argv[])
{
	progname = argv[0];

	if (argc < 3) {
		fprintf(stderr, "usage: %s <infile> <outfile>\n", progname);
		exit(EXIT_FAILURE);
	}
	char *infilename = argv[1];
	int in;
	in = open(infilename, O_RDONLY);
	if (in == -1)
		err(EXIT_FAILURE, "open() failed");

	struct stat s;
	if (fstat(in, &s) == -1) {
		warn("fstat() failed");
		goto close_in;
	}
	off_t size = s.st_size;
	mode_t mode = s.st_mode;

	char *outfilename = argv[2];
	int out;
	out = open(outfilename, O_WRONLY | O_TRUNC | O_CREAT, mode);
	if (out == -1) {
		warn("open() failed");
		goto close_in;
	}

	off_t current = 0;
	off_t end = 0;
	for (;;) {
		// skip a hole
		current = lseek(in, current, SEEK_DATA);
		if (current == -1) {
			if (errno == ENXIO) {
				// no more data chunk
				break;
			} else {
				warn("lseek(SEEK_DATA) failed");
				goto close_out;
			}
		}
		if (lseek(out, current, SEEK_SET) == -1) {
			warn("lseek(SEEK_SET) failed");
			goto close_out;
		}

		// get the end of data chunk
		end = lseek(in, current, SEEK_HOLE);
		if (end == -1) {
			warn("lseek(SEEK_HOLE) failed");
			goto close_out;
		}
		if (lseek(in, current, SEEK_SET) == -1) {
			warn("lseek(SEEK_SET) failed");
			goto close_out;
		}

		write_chunk(in, out, end - current);
		current = end;
	}
	if (end != size) {
		// add a hole at the end of the file if exist
		if (ftruncate(out, size) == -1) {
			warn("ftruncate() failedd");
			goto close_out;
		}
	}

close_out:
	if (close(out) == -1)
		err(EXIT_FAILURE, "close() failed");
close_in:
	if (close(in) == -1)
		err(EXIT_FAILURE, "close() failed");
	exit(EXIT_SUCCESS);
}
