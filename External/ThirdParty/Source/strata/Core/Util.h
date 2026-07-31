#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <stdarg.h>

char* DupString(const char* s);

typedef struct ArenaChunk {
    struct ArenaChunk* next;
    size_t size;
} ArenaChunk;

typedef struct {
    ArenaChunk* head;
    size_t default_chunk;
    char* ptr;
    char* end;
} Arena;

void  arena_init(Arena* a, size_t initial_chunk);
void  arena_free(Arena* a);
void* arena_alloc(Arena* a, size_t size);
void* arena_alloc_aligned(Arena* a, size_t size, size_t align);
void* arena_dup(Arena* a, const void* src, size_t size);
char* arena_strdup(Arena* a, const char* s);
char* arena_strndup(Arena* a, const char* s, size_t n);
char* arena_format(Arena* a, const char* fmt, ...);
char* arena_vformat(Arena* a, const char* fmt, va_list args);

Arena* scratch_arena(void);
void   scratch_reset(void);

typedef struct {
    const char* data;
    size_t len;
} Str;

#define STR_C(cstr) ((Str){(cstr), strlen(cstr)})
#define STR_N(d, n) ((Str){(d), (n)})
#define STR_EMPTY ((Str){NULL, 0})

static inline bool StrEq(Str a, Str b)
{
    return a.len == b.len && (a.len == 0 || memcmp(a.data, b.data, a.len) == 0);
}

static inline bool StrEqC(Str a, const char* b)
{
    size_t n = strlen(b);
    return a.len == n && (a.len == 0 || memcmp(a.data, b, a.len) == 0);
}

static inline Str StrSlice(Str s, size_t start, size_t end)
{
    if (end > s.len) end = s.len;
    if (start > end) start = end;
    return (Str){s.data + start, end - start};
}

typedef struct {
    void** items;
    size_t count;
    size_t cap;
} Vec;

static inline void VecInit(Vec* v)
{
    v->items = NULL;
    v->count = 0;
    v->cap = 0;
}

void  VecReserve(Vec* v, size_t n);
void  VecPush(Vec* v, void* item);
void* VecPop(Vec* v);

static inline void* VecGet(const Vec* v, size_t i)
{
    return (i < v->count) ? v->items[i] : NULL;
}

typedef struct {
    const char** keys;
    void** values;
    size_t count;
    size_t cap;
} StrMap;

static inline void StrMapInit(StrMap* m)
{
    m->keys = NULL;
    m->values = NULL;
    m->count = 0;
    m->cap = 0;
}

void  StrMapPut(StrMap* m, const char* key, void* value);
void* StrMapGet(const StrMap* m, const char* key);

typedef struct {
    char* data;
    size_t len;
    size_t cap;
} Sb;

static inline void SbInit(Sb* sb)
{
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
}

void SbPutc(Sb* sb, char c);
void SbPuts(Sb* sb, const char* s);
void SbPutn(Sb* sb, const char* s, size_t n);
void SbPutr(Sb* sb, char c, size_t repeat);
void SbPrintf(Sb* sb, const char* fmt, ...);
char* SbFinish(Sb* sb, Arena* arena);
