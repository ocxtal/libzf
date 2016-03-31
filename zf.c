
/**
 * @file zf.c
 *
 * @brief zlib-file API compatible I/O wrapper library
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "kopen.h"
#include "zf.h"

#ifdef HAVE_Z
#include "zlib.h"
#endif

#ifdef HAVE_BZ2
#include "bzlib.h"
#endif

#define UNITTEST_UNIQUE_ID			44

#ifdef TEST
/* use auto-generated main function to run tests */
#define UNITTEST 					1
#define UNITTEST_ALIAS_MAIN			1
#endif

#include "unittest.h"


/* constants */
#define ZF_BUF_SIZE			( 512 * 1024 )		/* 512KB */

/* function pointer type aliases */
typedef void *(*zf_dopen_t)(
	int fd,
	char const *mode);
typedef void *(*zf_open_t)(
	char const *path,
	char const *mode);
typedef void (*zf_init_t)(
	void *);
typedef void *(*zf_close_t)(
	void *fp);
typedef size_t (*zf_read_t)(
	void *fp,
	void *ptr,
	size_t len);
typedef size_t (*zf_write_t)(
	void *fp,
	void *ptr,
	size_t len);

/* wrapped functions */

/**
 * @fn fread_wrap
 * @brief wrap fread to zlib compatible
 */
size_t fread_wrap(
	void *fp,
	void *ptr,
	size_t len)
{
	return(fread(ptr, 1, len, (FILE *)fp));
}

/**
 * @fn fwrite_wrap
 * @brief wrap fwrite to zlib compatible
 */
size_t fwrite_wrap(
	void *fp,
	void *ptr,
	size_t len)
{
	return(fwrite(ptr, 1, len, (FILE *)fp));
}

/* zlib-dependent functions */
#ifdef HAVE_Z
/**
 * @fn zf_init_gzip
 * @brief setup function, set buffer size to 512k
 */
void zf_init_gzip(
	void *fp)
{
	gzbuffer((gzFile)fp, 512 * 1024);		/** 512 kilobytes */
	return;
}
#endif

/**
 * @struct zf_functions_s
 * @brief function container
 */
struct zf_functions_s {
	char const *ext;
	zf_dopen_t dopen;
	zf_open_t open;
	zf_init_t init;
	zf_close_t close;
	zf_read_t read;
	zf_write_t write;	
};

/**
 * @struct zf_s
 * @brief context container
 */
struct zf_s {
	char *path;
	char *mode;
	int fd;
	int eof;		/* == 1 if fp reached EOF, == 2 if curr reached the end of buf */
	void *ko;
	void *fp;		/* one of {FILE * / gzFile / BZFILE *} */
	uint8_t *buf;
	uint64_t size;
	uint64_t curr, end;
	struct zf_functions_s fn;
};

/**
 * @val fn_table
 * @brief extension and function table
 */
static
struct zf_functions_s const fn_table[] = {
	/* default */
	{
		.ext = NULL,
		.dopen = (zf_dopen_t)fdopen,
		.open = (zf_open_t)fopen,
		.init = (zf_init_t)NULL,
		.close = (zf_close_t)fclose,
		.read = (zf_read_t)fread_wrap,
		.write = (zf_write_t)fwrite_wrap
	},
	/* gzip */
	{
		.ext = ".gz",
		#ifdef HAVE_Z
		.dopen = (zf_dopen_t)gzdopen,
		.open = (zf_open_t)gzopen,
		.init = (zf_init_t)zf_init_gzip,
		.close = (zf_close_t)gzclose,
		.read = (zf_read_t)gzread,
		.write = (zf_write_t)gzwrite
		#endif
	},
	/* bzip2 */
	{
		.ext = ".bz2",
		#ifdef HAVE_BZ2
		.dopen = (zf_dopen_t)BZ2_bzdopen,
		.open = (zf_open_t)BZ2_bzopen,
		.init = (zf_init_t)NULL,
		.close = (zf_close_t)BZ2_bzclose,
		.read = (zf_read_t)BZ2_bzread,
		.write = (zf_write_t)BZ2_bzwrite
		#endif
	},
	/* other unsupported formats */
	{ .ext = ".lz" },
	{ .ext = ".lzma" },
	{ .ext = ".xz" },
	{ .ext = ".z" }
};

/**
 * @fn zfopen
 * @brief open file, similar to fopen / gzopen,
 * compression format can be explicitly specified adding an extension to `mode', e.g. "w+.bz2".
 */
zf_t *zfopen(
	char const *path,
	char const *mode)
{
	if(path == NULL || mode == NULL) {
		return(NULL);
	}

	/* determine format */
	char *mode_dup = (char *)mode;
	char const *path_tail = path + strlen(path);
	char const *mode_tail = mode + strlen(mode);
	struct zf_functions_s const *fn = &fn_table[0];
	for(int64_t i = 1; i < sizeof(fn_table) / sizeof(struct zf_functions_s); i++) {
		
		/* check path */
		if(strncmp(path_tail - strlen(fn_table[i].ext),
			fn_table[i].ext,
			strlen(fn_table[i].ext)) == 0) {

			/* hit */
			fn = &fn_table[i];
			break;
		}

		/* check mode */
		if(strncmp(mode_tail - strlen(fn_table[i].ext),
			fn_table[i].ext,
			strlen(fn_table[i].ext)) == 0) {
			
			/* hit */
			fn = &fn_table[i];
			char *mode_dup = strdup(mode);
			mode_dup[strlen(mode) - strlen(fn_table[i].ext)] = '\0';
			break;
		}
	}

	/* check if functions are available */
	if(fn->open == NULL) {
		return(NULL);
	}

	/* malloc context */
	struct zf_s *fio = (struct zf_s *)malloc(
		sizeof(struct zf_s) + ZF_BUF_SIZE);
	if(fio == NULL) {
		return(NULL);
	}
	memset(fio, 0, sizeof(struct zf_s));
	fio->buf = (uint8_t *)(fio + 1);
	fio->size = ZF_BUF_SIZE;
	fio->fn = *fn;

	/* open file */
	if(mode[0] == 'r') {
		/* read mode, open file with kopen */
		fio->ko = kopen(path, &fio->fd);
		if(fio->ko == NULL) {
			goto _zfopen_finish;
		}
		fio->fp = fio->fn.dopen(fio->fd, mode);
	} else {
		/* write mode, check if stdout is specified */
		if(strncmp(path, "-", strlen("-")) == 0) {
			fio->path = "-";
			fio->fd = STDOUT_FILENO;
			fio->ko = NULL;
			fio->fp = stdout;
			goto _zfopen_finish;
		}

		/* open file */
		fio->fp = fio->fn.open(path, mode);
		fio->fd = -1;		/* fd is invalid in write mode */
		fio->ko = NULL;		/* ko is also invalid */
		goto _zfopen_finish;
	}

_zfopen_finish:;
	if(fio->fp == NULL) {
		/* something wrong occurred */
		if(fio->ko != NULL) {
			kclose(fio->ko);
		}
		free(fio);
		return(NULL);
	}

	/* everything is going right */
	fio->path = strdup(path);
	fio->mode = (mode_dup == mode) ? strdup(mode) : mode_dup;
	fio->curr = fio->end = 0;
	if(fio->fn.init != NULL) {
		fio->fn.init(fio->fp);
	}

	return((zf_t *)fio);
}

/**
 * @fn zfclose
 * @brief close file, similar to fclose / gzclose
 */
int zfclose(
	zf_t *fio)
{
	if(fio == NULL) {
		return(1);
	}

	/* flush if write mode */
	if(fio->mode[0] != 'r') {
		fio->fn.write(fio->fp, (void *)fio->buf, fio->curr);
	}

	/* close file */
	if(fio->fp != NULL) {
		fio->fn.close(fio->fp);
		if(fio->ko != NULL) {
			kclose(fio->ko);
		}
	}
	if(fio->path != NULL) {
		free(fio->path);
	}
	if(fio->mode != NULL) {
		free(fio->mode);
	}
	free(fio);
	return(0);
}

/**
 * @fn zfread
 * @brief read from file, similar to gzread
 */
size_t zfread(
	zf_t *fio,
	void *ptr,
	size_t len)
{
	size_t copied_size = 0;

	/* check eof */
	if(fio->eof == 2) {
		return(0);
	}

	/* if buffer is not empty, copy from buffer */
	if(fio->curr < fio->end) {
		uint64_t rem_size = fio->end - fio->curr;
		uint64_t buf_copy_size = (rem_size < len) ? rem_size : len;
		memcpy(ptr, (void *)&fio->buf[fio->curr], buf_copy_size);

		/* update */
		len -= buf_copy_size;
		ptr += buf_copy_size;
		fio->curr += buf_copy_size;
		copied_size += buf_copy_size;
	}

	/* if fp already reached EOF */
	if(fio->eof == 1) {
		fio->eof = 2;
		return(copied_size);
	}

	/* issue fread */
	if(len > 0) {
		uint64_t read_size = fio->fn.read(fio->fp, ptr, len);
		copied_size += read_size;
		if(read_size < len) {
			fio->eof = 2;
		}
	}
	return(copied_size);
}

/**
 * @fn zfwrite
 * @brief write to file, similar to gzwrite
 */
size_t zfwrite(
	zf_t *fio,
	void *ptr,
	size_t len)
{
	return(fio->fn.write(fio->fp, ptr, len));
}

/**
 * @fn zfgetc
 */
int zfgetc(
	zf_t *fio)
{
	/* if the pointer reached the end, refill the buffer */
	if(fio->curr >= fio->end) {
		fio->curr = 0;
		fio->end = (fio->eof == 0)
			? fio->fn.read(fio->fp, fio->buf, fio->size)
			: 0;
		fio->eof = (fio->end < fio->size) + (fio->end == 0);
	}
	if(fio->eof == 2) {
		return(EOF);
	}
	return((int)fio->buf[fio->curr++]);
}

/**
 * @fn zfputc
 */
int zfputc(
	zf_t *fio,
	int c)
{
	fio->buf[fio->curr++] = (uint8_t)c;
	
	/* flush if buffer is full */
	if(fio->curr == fio->size) {
		fio->curr = 0;
		uint64_t written = fio->fn.write(fio->fp, fio->buf, fio->size);
	}
	return(c);
}

/**
 * @fn zfputs
 */
int zfputs(
	zf_t *fio,
	char const *s)
{
	while(*s != '\0') {
		zfputc(fio, (int)*s++);
	}
	zfputc(fio, '\n');
	return(0);
}

/**
 * @fn zfprintf
 */
int zfprintf(
	zf_t *fio,
	char const *format,
	...)
{
	va_list l;
	va_start(l, format);

	/* flush */
	if(fio->curr != 0) {
		uint64_t flush = fio->fn.write(fio->fp, fio->buf, fio->curr);
		/* something is wrong */
		if(flush != fio->curr) {
			return(0);
		}
	}

	/* fprintf */
	uint64_t size = vsprintf((char *)fio->buf, format, l);

	/* flush */
	uint64_t written = fio->fn.write(fio->fp, fio->buf, size);
	fio->curr = size - written;

	va_end(l);
	return((int)written);
}

/* unittests */
#ifdef TEST
#include <time.h>

/**
 * @fn init_rand
 */
void *init_rand(
	void *params)
{
	srand(time(NULL));
	return(NULL);
}
void clean_rand(
	void *gctx)
{
	return;		/* nothing to do */
}

unittest_config(
	.name = "fio",
	.init = init_rand,
	.clean = clean_rand
);

/**
 * @val ascii_table
 */
static
char const *ascii_table =
	" !\"#$%&\'()*+,-./"
	"0123456789:;<=>?"
	"@ABCDEFGHIJKLMNO"
	"PQRSTUVWXYZ[\\]^_"
	"`abcdefghijklmno"
	"pqrstuvwxyz{|}~\n";

/**
 * @fn make_random_array
 */
static inline
void *make_random_array(
	void *params)
{
	uint64_t len = (uint64_t)params;
	char *arr = (char *)malloc(len);
	for(uint64_t i = 0; i < len; i++) {
		arr[i] = ascii_table[rand() % strlen(ascii_table)];
	}
	return((void *)arr);
}

#define with(_len) \
	.init = make_random_array, \
	.clean = free, \
	.params = (void *)((uint64_t)(_len))

#define omajinai() \
	char *arr = (char *)ctx;

/* test zfopen / zfclose with len 100M */
#define TEST_ARR_LEN 		100000000
unittest(with(TEST_ARR_LEN))
{
	omajinai();

	/* write */
	zf_t *wfp = zfopen("tmp.txt", "w");
	assert(wfp != NULL, "%p", wfp);

	size_t written = zfwrite(wfp, arr, TEST_ARR_LEN);
	assert(written == TEST_ARR_LEN, "%llu", written);

	zfclose(wfp);

	/* read */
	zf_t *rfp = zfopen("tmp.txt", "r");
	assert(rfp != NULL, "%p", rfp);

	char *rarr = (char *)malloc(TEST_ARR_LEN);
	size_t read = zfread(rfp, rarr, TEST_ARR_LEN);
	assert(read == TEST_ARR_LEN, "%llu", read);
	assert(zfgetc(rfp) == EOF, "%d", zfgetc(rfp));

	zfclose(rfp);

	/* compare */
	assert(memcmp(arr, rarr, TEST_ARR_LEN) == 0);

	/* cleanup */
	free(rarr);
	remove("tmp.txt");
}

/* getc / putc */
unittest(with(TEST_ARR_LEN))
{
	omajinai();

	/* write with zfputc */
	zf_t *wfp = zfopen("tmp.txt", "w");

	for(int64_t i = 0; i < TEST_ARR_LEN; i++) {
		zfputc(wfp, arr[i]);
	}

	zfclose(wfp);

	/* read with zfgetc */
	zf_t *rfp = zfopen("tmp.txt", "r");

	char *rarr = (char *)malloc(TEST_ARR_LEN);
	for(int64_t i = 0; i < TEST_ARR_LEN; i++) {
		rarr[i] = zfgetc(rfp);
	}
	assert(zfgetc(rfp) == EOF, "%d", zfgetc(rfp));

	zfclose(rfp);

	/* compare */
	assert(memcmp(arr, rarr, TEST_ARR_LEN) == 0);

	/* cleanup */
	free(rarr);
	remove("tmp.txt");
}

/* zlib-dependent tests */
#ifdef HAVE_Z
unittest(with(TEST_ARR_LEN))
{
	omajinai();

	/* write */
	zf_t *wfp = zfopen("tmp.txt.gz", "w");
	assert(wfp != NULL, "%p", wfp);

	size_t written = zfwrite(wfp, arr, TEST_ARR_LEN);
	assert(written == TEST_ARR_LEN, "%llu", written);

	zfclose(wfp);

	/* read */
	zf_t *rfp = zfopen("tmp.txt.gz", "r");
	assert(rfp != NULL, "%p", rfp);

	char *rarr = (char *)malloc(TEST_ARR_LEN);
	size_t read = zfread(rfp, rarr, TEST_ARR_LEN);
	assert(read == TEST_ARR_LEN, "%llu", read);
	assert(zfgetc(rfp) == EOF, "%d", zfgetc(rfp));

	zfclose(rfp);

	/* compare */
	assert(memcmp(arr, rarr, TEST_ARR_LEN) == 0);

	/* cleanup */
	free(rarr);
	remove("tmp.txt.gz");
}

/* specify compression format with mode flag */
unittest(with(TEST_ARR_LEN))
{
	omajinai();

	/* write */
	zf_t *wfp = zfopen("tmpfile", "w.gz");
	assert(wfp != NULL, "%p", wfp);

	size_t written = zfwrite(wfp, arr, TEST_ARR_LEN);
	assert(written == TEST_ARR_LEN, "%llu", written);

	zfclose(wfp);

	/* read */
	zf_t *rfp = zfopen("tmpfile", "r.gz");
	assert(rfp != NULL, "%p", rfp);

	char *rarr = (char *)malloc(TEST_ARR_LEN);
	size_t read = zfread(rfp, rarr, TEST_ARR_LEN);
	assert(read == TEST_ARR_LEN, "%llu", read);
	assert(zfgetc(rfp) == EOF, "%d", zfgetc(rfp));

	zfclose(rfp);

	/* compare */
	assert(memcmp(arr, rarr, TEST_ARR_LEN) == 0);

	/* cleanup */
	free(rarr);
	remove("tmpfile");
}

/* getc / putc */
unittest(with(TEST_ARR_LEN))
{
	omajinai();

	/* write with zfputc */
	zf_t *wfp = zfopen("tmp.txt.gz", "w");

	for(int64_t i = 0; i < TEST_ARR_LEN; i++) {
		zfputc(wfp, arr[i]);
	}

	zfclose(wfp);

	/* read with zfgetc */
	zf_t *rfp = zfopen("tmp.txt.gz", "r");

	char *rarr = (char *)malloc(TEST_ARR_LEN);
	for(int64_t i = 0; i < TEST_ARR_LEN; i++) {
		rarr[i] = zfgetc(rfp);
	}
	assert(zfgetc(rfp) == EOF, "%d", zfgetc(rfp));

	zfclose(rfp);

	/* compare */
	assert(memcmp(arr, rarr, TEST_ARR_LEN) == 0);

	/* cleanup */
	free(rarr);
	remove("tmp.txt.gz");
}
#endif /* HAVE_Z */

/* bzip2-dependent tests */
#ifdef HAVE_BZ2
unittest(with(TEST_ARR_LEN))
{
	omajinai();

	/* write */
	zf_t *wfp = zfopen("tmp.txt.bz2", "w");
	assert(wfp != NULL, "%p", wfp);

	size_t written = zfwrite(wfp, arr, TEST_ARR_LEN);
	assert(written == TEST_ARR_LEN, "%llu", written);

	zfclose(wfp);

	/* read */
	zf_t *rfp = zfopen("tmp.txt.bz2", "r");
	assert(rfp != NULL, "%p", rfp);

	char *rarr = (char *)malloc(TEST_ARR_LEN);
	size_t read = zfread(rfp, rarr, TEST_ARR_LEN);
	assert(read == TEST_ARR_LEN, "%llu", read);
	assert(zfgetc(rfp) == EOF, "%d", zfgetc(rfp));

	zfclose(rfp);

	/* compare */
	assert(memcmp(arr, rarr, TEST_ARR_LEN) == 0);

	/* cleanup */
	free(rarr);
	remove("tmp.txt.bz2");
}

/* specify compression format with mode flag */
unittest(with(TEST_ARR_LEN))
{
	omajinai();

	/* write */
	zf_t *wfp = zfopen("tmpfile", "w.bz2");
	assert(wfp != NULL, "%p", wfp);

	size_t written = zfwrite(wfp, arr, TEST_ARR_LEN);
	assert(written == TEST_ARR_LEN, "%llu", written);

	zfclose(wfp);

	/* read */
	zf_t *rfp = zfopen("tmpfile", "r.bz2");
	assert(rfp != NULL, "%p", rfp);

	char *rarr = (char *)malloc(TEST_ARR_LEN);
	size_t read = zfread(rfp, rarr, TEST_ARR_LEN);
	assert(read == TEST_ARR_LEN, "%llu", read);
	assert(zfgetc(rfp) == EOF, "%d", zfgetc(rfp));

	zfclose(rfp);

	/* compare */
	assert(memcmp(arr, rarr, TEST_ARR_LEN) == 0);

	/* cleanup */
	free(rarr);
	remove("tmpfile");
}

/* getc / putc */
unittest(with(TEST_ARR_LEN))
{
	omajinai();

	/* write with zfputc */
	zf_t *wfp = zfopen("tmp.txt.bz2", "w");

	for(int64_t i = 0; i < TEST_ARR_LEN; i++) {
		zfputc(wfp, arr[i]);
	}

	zfclose(wfp);

	/* read with zfgetc */
	zf_t *rfp = zfopen("tmp.txt.bz2", "r");

	char *rarr = (char *)malloc(TEST_ARR_LEN);
	for(int64_t i = 0; i < TEST_ARR_LEN; i++) {
		rarr[i] = zfgetc(rfp);
	}
	assert(zfgetc(rfp) == EOF, "%d", zfgetc(rfp));

	zfclose(rfp);

	/* compare */
	assert(memcmp(arr, rarr, TEST_ARR_LEN) == 0);

	/* cleanup */
	free(rarr);
	remove("tmp.txt.bz2");
}
#endif /* HAVE_BZ2 */

#endif

/**
 * end of zf.c
 */
