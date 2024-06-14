#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <math.h>

#define ARRAY_SIZE(a) ((int)(sizeof(a) / sizeof(a[0])))


#define NFRACBITS 28

struct calpar {
	const char *path;
	int (*calc)(const struct calpar *cp, int dir_fd);
};

static int read_param(int dir_fd, const char *path, int sgn, float *out)
{
	int fd, stat;
	char buf[40];
	char *endptr;
	unsigned long v;
	int32_t w;

	fd = openat(dir_fd, path, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Cannot open file %s (%s)\n",
			path, strerror(errno));
		return -1;
	}
	memset(buf, 0, sizeof(buf));
	stat = read(fd, buf, sizeof(buf));
	if (stat < 0) {
		fprintf(stderr, "Error reading from file %s (%s)\n",
			path, strerror(errno));
		goto end;
	}
	v = strtoul(buf, &endptr, 16);

	if (!v && (endptr == buf)) {
		fprintf(stderr, "No valid digits in %s\n", path);
		goto end;
	}
	if (v == ULONG_MAX && errno == ERANGE) {
		fprintf(stderr, "Overflow reading %s\n", path);
		goto end;
	}
	w = sgn && (v > 127) ? v - 256 : v;
	*out = w;
end:
	close(fd);
	return stat;
}

static int to_fp(float v)
{
	v *= pow(2, NFRACBITS);
	return round(v);
}

static int is_g1_calc(const struct calpar *cp, int dir_fd)
{
	int stat;
	float v;
	int32_t fpv;

	stat = read_param(dir_fd, cp->path, 1, &v);
	if (stat < 0)
		return stat;
	v *= 0.00078125F;
	fpv = to_fp(v);
	printf("is_g1 = %f, fp = 0x%08x (Q%d)\n", v, fpv, NFRACBITS);
	return 0;
}

static int is_g2_calc(const struct calpar *cp, int dir_fd)
{
	int stat;
	float v;
	int32_t fpv;

	stat = read_param(dir_fd, cp->path, 0, &v);
	if (stat < 0)
		return stat;
	v = (v * 0.8F) / 256.0F + 0.6F;
	fpv = to_fp(v);
	printf("is_g2 = %f, fp = 0x%08x (Q%d)\n", v, fpv, NFRACBITS);
	return 0;
}

static int vs_g_calc(const struct calpar *cp, int dir_fd)
{
	int stat;
	float v;
	int32_t fpv;

	stat = read_param(dir_fd, cp->path, 0, &v);
	if (stat < 0)
		return stat;
	v = (v * 0.4F) / 256.0F + 0.8F;
	fpv = to_fp(v);
	printf("vs_g = %f, fp = 0x%08x (Q%d)\n", v, fpv, NFRACBITS);
	return 0;
}

static const struct calpar params[] = {
	{
		.path = "is_g1",
		.calc = is_g1_calc,
	},
	{
		.path = "is_g2",
		.calc = is_g2_calc,
	},
	{
		.path = "vs_g",
		.calc = vs_g_calc,
	},
};


int main(int argc, char *argv[])
{
	int dir_fd, i, stat;
	const char *paths[] = {
		"is_g1",
		"is_g2",
		"vs_g",
	};

	if (argc < 2) {
		fprintf(stderr, "Use %s <path_of_device_under_sys>\n", argv[0]);
		exit(127);
	}
	/* Open the directory first */
	dir_fd = open(argv[1], O_RDONLY|O_DIRECTORY);
	if (dir_fd < 0) {
		fprintf(stderr,
			"Error opening sysfs device directory %s (%s)\n",
			argv[1], strerror(errno));
		exit(127);
	}
	for (i = 0; i < ARRAY_SIZE(paths); i++) {
		stat = params[i].calc(&params[i], dir_fd);
		if (stat < 0)
			exit(127);
	}
	return 0;
}
