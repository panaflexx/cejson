/* stringbuf.h – heap-only, header-only, C17 string builder */
#ifndef STRINGBUF_H
#define STRINGBUF_H

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

/* Default max size if user passes 0 or huge value */
#ifndef STRINGBUF_DEFAULT_CAP
#define STRINGBUF_DEFAULT_CAP (1024 * 64)   /* 64 KB default */
#endif

typedef struct {
    char*  data;      /* Heap-allocated buffer, always null-terminated */
    ssize_t size;      /* Used bytes (excluding terminating '\0') */
    ssize_t capacity;  /* Total allocated bytes (including space for '\0') */
	bool   owned;
} StringBuf;

/* Init with optional initial content */
static inline bool stringbuf_init_str(StringBuf* sb, const char* src, size_t srclen, size_t capacity_hint)
{
    if (!sb) return false;

    size_t cap = capacity_hint;
    if (cap == 0 || cap > 1024*1024*16)        /* sanity cap at 16 MiB */
        cap = STRINGBUF_DEFAULT_CAP;

    /* Always leave room for trailing '\0' */
    char* mem = (char*)malloc(cap);
    if (!mem) return false;

    sb->data = mem;
    sb->capacity = cap;
    sb->size = 0;
    sb->data[0] = '\0';
	sb->owned = true;

    if (src && srclen) {
        if (srclen > cap - 1) srclen = cap - 1;
        memcpy(sb->data, src, srclen);
        sb->size = srclen;
        sb->data[sb->size] = '\0';
    }

    return true;
}
static inline bool stringbuf_init(StringBuf* sb, size_t capacity_hint)
{
	return stringbuf_init_str(sb, NULL, 0, capacity_hint);
}

/* Init with optional initial content */
static inline bool stringbuf_init_buf(StringBuf* sb, char* src, size_t srclen)
{
    if (!sb) return false;

    sb->data = src;
    sb->capacity = srclen;
    sb->size = 0;
    sb->data[0] = '\0';
	sb->owned = false;

    return true;
}

/* Free the buffer – must be called when done */
static inline void stringbuf_free(StringBuf* sb)
{
    if (sb && sb->data && sb->owned) {
        free(sb->data);
	}
	sb->data = NULL;
	sb->size = sb->capacity = 0;
}

/* Ensure at least `need` bytes of space (including trailing '\0') */
static inline bool stringbuf_reserve(StringBuf* sb, ssize_t need)
{
    if (!sb) return false;
    need++;                                    /* +1 for '\0' */

    if (need <= sb->capacity) return true;

	printf("REALLOC\n");
    ssize_t newcap = sb->capacity * 2;
    if (newcap < need) newcap = need;
    if (newcap < 128) newcap = 128;

    char* newmem = (char*)realloc(sb->data, newcap);
    if (!newmem) return false;

    sb->data = newmem;
    sb->capacity = newcap;
    return true;
}

/* Append raw bytes */
static inline bool stringbuf_append(StringBuf* sb, const char* src, ssize_t len)
{
    if (!sb || !src || len == 0) return false;
    if (!stringbuf_reserve(sb, sb->size + len)) return false;

	if(sb->size + len >= sb->capacity) {
		fprintf(stderr, "StringBuf ERROR: TARGET OVER CAPACITY\n");
		return false;
	}

    memcpy(sb->data + sb->size, src, len);
    sb->size += len;
    sb->data[sb->size] = '\0';
    return true;
}

/* Append null-terminated string */
static inline bool stringbuf_append_str(StringBuf* sb, const char* str)
{
    return str ? stringbuf_append(sb, str, strlen(str)) : false;
}

/* Append single char */
static inline bool stringbuf_append_char(StringBuf* sb, char c)
{
    if (!stringbuf_reserve(sb, sb->size + 1)) return false;
    sb->data[sb->size] = c;
    sb->data[++sb->size] = '\0';
    return true;
}

/* Formatted append (like snprintf) */
static inline bool stringbuf_appendf(StringBuf* sb, const char* fmt, ...)
{
    if (!sb || !fmt) return false;

    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);

    int needed = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);
    if (needed < 0) { va_end(ap); return false; }

    if (!stringbuf_reserve(sb, sb->size + (size_t)needed)) {
        va_end(ap);
        return false;
    }

    vsnprintf(sb->data + sb->size, sb->capacity - sb->size, fmt, ap);
    sb->size += (size_t)needed;
    va_end(ap);
    return true;
}

/* Clear contents (keeps allocation) */
static inline void stringbuf_clear(StringBuf* sb)
{
    if (sb) {
        sb->size = 0;
        if (sb->data) sb->data[0] = '\0';
    }
}

/* Accessors */
static inline char*       stringbuf_data(StringBuf* sb)        { return sb ? sb->data : NULL; }
static inline const char* stringbuf_cstr(const StringBuf* sb) { return sb && sb->data ? sb->data : ""; }
static inline size_t      stringbuf_size(const StringBuf* sb) { return sb ? sb->size : 0; }
static inline size_t      stringbuf_capacity(const StringBuf* sb) { return sb ? sb->capacity : 0; }
static inline bool        stringbuf_empty(const StringBuf* sb) { return !sb || sb->size == 0; }

#endif /* STRINGBUF_H */
