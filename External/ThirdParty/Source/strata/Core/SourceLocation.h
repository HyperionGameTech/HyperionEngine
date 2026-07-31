#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "strata/Core/Util.h"

typedef struct {
    uint32_t start;
    uint16_t length;
    uint16_t fileId;
} SourceRange;

static inline uint32_t SourceRangeEnd(SourceRange r)
{
    return r.start + r.length;
}

static inline bool SourceRangeValid(SourceRange r)
{
    return r.length != 0 || r.start != 0;
}

#define SRC_INVALID ((SourceRange){0, 0, 0})
#define SRC_POS(pos, len) ((SourceRange){(uint32_t)(pos), (uint16_t)(len), 0})

typedef struct {
    uint32_t line;   // 1-based
    uint32_t column; // 1-based
} LineCol;

typedef struct {
    const char* m_text;
    size_t m_textLen;
    const char* m_name;

    uint32_t* m_lineStarts;
    size_t m_lineCount;
    size_t m_lineCap;
} SourceManager;

void SourceManagerInit(SourceManager* sm);
void SourceManagerFree(SourceManager* sm);
void SourceManagerSetSource(SourceManager* sm, const char* text, size_t len, const char* name);

Str SourceManagerSlice(const SourceManager* sm, SourceRange r);
LineCol SourceManagerLineCol(const SourceManager* sm, uint32_t offset);
Str SourceManagerLineText(const SourceManager* sm, uint32_t line);
uint32_t SourceManagerLineCount(const SourceManager* sm);
