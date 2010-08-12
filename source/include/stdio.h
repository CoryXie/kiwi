/*
 * Copyright (C) 2008-2009 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		Standard I/O functions.
 */

#ifndef __STDIO_H
#define __STDIO_H

#include <sys/types.h>
#define __need_NULL
#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

struct __fstream_internal;

/** End of file return value. */
#define EOF		(-1)

/** Size of buffers for IO streams. */
#define BUFSIZ		2048

/** Buffer flags for an IO stream. */
#define _IOFBF		0		/**< Input/output fully buffered. */
#define _IOLBF		1		/**< Input/output line buffered. */
#define _IONBF		2		/**< Input/output unbuffered. */

/** Minimum number of unique files from tmpnam() and friends. */
#define TMP_MAX		10000

/** Maximum number of streams that can be open simultaneously. */
#define FOPEN_MAX	32

/** Maximum length of a filename string. */
#define FILENAME_MAX	4096

/** Actions for fseek(). */
#define SEEK_SET	1		/**< Set the offset to the exact position specified. */
#define SEEK_CUR	2		/**< Add the supplied value to the current offset. */
#define SEEK_END	3		/**< Set the offset to the end of the file plus the supplied value. */

/** Type describing an open file stream. */
typedef struct __fstream_internal FILE;

/** Type describing a file offset. */
typedef off_t fpos_t;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

extern void clearerr(FILE *stream);
/* char *ctermid(char *); */
/* int dprintf(int, const char *__restrict, ...); */
extern int fclose(FILE *stream);
extern int feof(FILE *stream);
extern int ferror(FILE *stream);
extern int fflush(FILE *stream);
extern int fgetc(FILE *stream);
/* int fgetpos(FILE *, fpos_t *); */
extern char *fgets(char *s, int size, FILE *stream);
//extern int fileno(FILE *stream);
/* void flockfile(FILE *); */
/* FILE *fmemopen(void *__restrict buf, size_t size, const char *__restrict mode); */
extern FILE *fopen(const char *__restrict path, const char *__restrict mode);
extern int fprintf(FILE *__restrict stream, const char *__restrict fmt, ...);
extern int fputc(int ch, FILE *stream);
extern int fputs(const char *__restrict s, FILE *__restrict stream);
extern size_t fread(void *__restrict ptr, size_t size, size_t nmemb, FILE *__restrict stream);
extern FILE *freopen(const char *__restrict path, const char *__restrict mode, FILE *__restrict stream);
extern int fscanf(FILE *__restrict stream, const char *__restrict fmt, ...);
extern int fseek(FILE *stream, long off, int act);
/* int fseeko(FILE *, off_t, int); */
/* int fsetpos(FILE *, const fpos_t *); */
extern long ftell(FILE *stream);
/* off_t ftell(FILE *); */
/* int ftrylockfile(FILE *); */
/* void funlockfile(FILE *); */
extern size_t fwrite(const void *__restrict ptr, size_t size, size_t nmemb, FILE *__restrict stream);
extern int getc(FILE *stream);
extern int getchar(void);
/* int getc_unlocked(FILE *); */
/* int getchar_unlocked(void); */
/* ssize_t getdelim(char **__restrict, size_t *__restrict, int, FILE *__restrict); */
/* ssize_t getline(char **__restrict, size_t *__restrict, FILE *__restrict); */
extern char *gets(char *s);
/* FILE *open_memstream(char **, size_t *); */
/* int pclose(FILE *); */
extern void perror(const char *s);
/* FILE *popen(const char *, const char *); */
extern int printf(const char *__restrict fmt, ...);
extern int putc(int ch, FILE *stream);
extern int putchar(int ch);
/* int putc_unlocked(int, FILE *); */
/* int putchar_unlocked(int); */
extern int puts(const char *s);
/* int remove(const char *); */
/* int rename(const char *, const char *); */
/* int renameat(int, const char *, int, const char *); */
extern void rewind(FILE *stream);
extern int scanf(const char *__restrict fmt, ...);
/* void setbuf(FILE *__restrict, char *__restrict); */
/* int setvbuf(FILE *__restrict, char *__restrict, int, size_t); */
extern int snprintf(char *__restrict buf, size_t size, const char *__restrict fmt, ...);
extern int sprintf(char *__restrict buf, const char *__restrict fmt, ...);
extern int sscanf(const char *__restrict buf, const char *__restrict fmt, ...);
/* char *tempnam(const char *, const char *); */
/* FILE *tmpfile(void); */
/* char *tmpnam(char *); */
extern int ungetc(int ch, FILE *stream);
extern int vfprintf(FILE *__restrict stream, const char *__restrict fmt, va_list args);
extern int vfscanf(FILE *__restrict stream, const char *__restrict fmt, va_list args);
extern int vprintf(const char *__restrict fmt, va_list args);
extern int vscanf(const char *__restrict fmt, va_list args);
extern int vsnprintf(char *__restrict buf, size_t size, const char *__restrict fmt, va_list args);
extern int vsprintf(char *__restrict buf, const char *__restrict fmt, va_list args);
extern int vsscanf(const char *__restrict buf, const char *__restrict fmt, va_list args);

#ifdef __cplusplus
extern int fgetpos(FILE *, fpos_t *);
extern int fsetpos(FILE *, const fpos_t *);
extern int remove(const char *path);
extern int rename(const char *source, const char *dest);
extern void setbuf(FILE *__restrict, char *__restrict);
extern int setvbuf(FILE *__restrict, char *__restrict, int, size_t);
extern FILE *tmpfile(void);
extern char *tmpnam(char *);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __STDIO_H */
