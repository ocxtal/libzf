# libzf

A wrapper of stdio / zlib / bzip2, providing zlib-style file I/O APIs. The library internally uses [kopen](https://github.com/attractivechaos/klib) to open files in read mode, enabling reading (gzip or bzip2-compressed) files on remote servers over ftp / http protocols.

## Build

Python (2.7 or 3.x) is required to run the build script written in [waf](https://github.com/waf-project/waf).

```
$ ./waf configure
$ ./waf build
```

## Functions

### zfopen

Open a file. `mode` follows the options of the `fopen` in stdio. Compression format will be detected from the extension of the `path`. The format can also be specified explicitly adding an extension to the `mode` flag, e.g. `fiopen("path/to/a/file", "w+.bz2")`. Passing `"-"` to `path` will connect file to `stdin` / `stdout`.

```
zf_t *zfopen(
	char const *path,
	char const *mode);
```

### zfclose

Close a file.

```
int zfclose(
	zf_t *fp);
```

### zfread

Read the content of the file by `len`. Note that the signature of the function follows the `gzread` in zlib, being different from `fread` in stdio.

```
size_t zfread(
	zf_t *fp,
	void *ptr,
	size_t len);
```

### zfpeek

Read the content of the file, without advancing the file pointer. `len` must be less than or equal to 512 kilobytes.

```
size_t zfpeek(
	zf_t *fp,
	void *ptr,
	size_t len);
```

### zfwrite

Write to the file by `len`.

```
size_t zfwrite(
	zf_t *fp,
	void *ptr,
	size_t len);
```

### zfgetc

getc compatible.

```
int zfgetc(
	zf_t *fp);
```

### zfungetc

Must not be called > 32 times contiguously.

```
int zfungetc(
	zf_t *fp,
	int c);
```

### zfeof

feof compatible

```
int zfeof(
	zf_t *fp);
```

### zfputc

putc compatible.

```
int zfputc(
	zf_t *fp,
	int c);
```

### zfputs

puts compatible.

```
int zfputs(
	zf_t *fp,
	char const *s);
```

### zfprintf

fprintf compatible. Current implementation cannot handle a string longer than 512 kilobytes (after formatting).

```
int zfprintf(
	zf_t *fp,
	char const *format,
	...);
```

## License

MIT