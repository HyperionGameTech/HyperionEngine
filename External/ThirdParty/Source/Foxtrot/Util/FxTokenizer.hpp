#pragma once

#include <Core/FxPagedArray.hpp>
//
#include <Core/FxFile.hpp>
#include <Core/FxHash.hpp>
#include <Core/MemPool/FxMemPool.hpp>
#include <FxEngine.hpp>
#include <cassert>
#include <cstdlib>
#include <cstring>


enum class FxTokenType
{
    eUnknown,
    eIdentifier,

    eString,
    eInteger,
    eFloat,

    eEquals,

    eLParen,
    eRParen,

    eLBracket,
    eRBracket,

    eLBrace,
    eRBrace,

    ePlus,
    eDollar,
    eMinus,

    eQuestion,

    eDot,
    eComma,
    eSemicolon,

    eDocComment,
};

struct FxToken
{
    enum class IsNumericResult
    {
        eNaN,
        eInteger,
        eFractional
    };

public:
    void Print(bool no_newline = false) const
    {
        printf("Token: (T:%-10s) {%.*s} %c", GetTypeName(Type), Length, Start, (no_newline) ? ' ' : '\n');
    }

    void Increment() { ++Length; }

    inline std::string GetStr() const { return std::string(Start, Length); }

    inline char* GetHeapStr() const
    {
        char* str = gEnginePool->Alloc<char>(Length + 1);

        if (str == nullptr) {
            FxPanic("FoxTokenizer", "Error allocating heap string!", 0);
            return nullptr;
        }

        std::strncpy(str, Start, Length);
        str[Length] = 0;

        return str;
    }

    FxHash64 GetHash()
    {
        if (Hash != FxHashNull64) {
            return Hash;
        }
        return (Hash = FxHashData64(FxSlice<char>(Start, Length)));
    }

    IsNumericResult IsNumeric() const;
    static const char* GetTypeName(FxTokenType type);

    int64 ToInt() const
    {
        char buffer[32];

        std::strncpy(buffer, Start, Length);
        buffer[Length] = 0;

        char* end = nullptr;
        return strtoll(buffer, &end, 10);
    }

    float32 ToFloat() const
    {
        char buffer[32];
        std::strncpy(buffer, Start, Length);
        buffer[Length] = 0;

        char* end = nullptr;
        return strtof(buffer, &end);
    }

    bool operator==(const char* str) const { return !strncmp(Start, str, Length); }

    bool IsEmpty() const { return (Start == nullptr || Length == 0); }

    void Clear()
    {
        Start = nullptr;
        End = nullptr;
        Length = 0;
    }


public:
    char* Start = nullptr;
    char* End = nullptr;

    FxHash64 Hash = FxHashNull64;
    FxTokenType Type = FxTokenType::eUnknown;
    uint32 Length = 0;

    uint16 FileColumn = 0;
    uint32 FileLine = 0;
};

class FxTokenizer
{
private:
public:
    const char* SingleCharOperators = "=()[]{}+-$.,;?";

    struct State
    {
        char* pDataStart = nullptr;
        char* pData = nullptr;
        char* pDataEnd = nullptr;
        bool bInString = false;

        uint32 FileLine = 0;
        char* pStartOfLine = nullptr;
    };


    FxTokenizer() = delete;

    FxTokenizer(char* data, uint32 buffer_size)
        : mpDataStart(data), mpData(data), mpDataEnd(data + buffer_size), mpLinePtr(data)
    {
    }

    void SubmitTokenIfData(FxToken& token, char* end_ptr = nullptr, char* start_ptr = nullptr);

    FxTokenType GetTokenType(FxToken& token);
    bool CheckOperators(FxToken& current_token, char ch);
    uint32 ReadQuotedString(char* buffer, uint32 max_size, bool skip_on_success = true);
    bool ExpectString(const char* expected_value, bool skip_on_success = true);

    void TryReadInternalCall();
    void IncludeFile(const char* path);

    void Tokenize();

    void SetDataPtr(char* ptr)
    {
        mpDataStart = ptr;
        mpData = ptr;
    }

    size_t GetTokenIndexInFile(FxToken& token) const
    {
        assert(token.Start > mpData);
        return (token.Start - mpData);
    }

    FxPagedArray<FxToken>& GetTokens() { return mTokens; }

    void SaveState()
    {
        mSavedState.pDataStart = mpData;
        mSavedState.pData = mpData;
        mSavedState.pDataEnd = mpDataEnd;
        mSavedState.bInString = mbInString;

        mSavedState.FileLine = mLineNumber;
        mSavedState.pStartOfLine = mpLinePtr;
    }

    void RestoreState()
    {
        mpDataStart = mSavedState.pDataStart;
        mpData = mSavedState.pData;
        mpDataEnd = mSavedState.pDataEnd;
        mbInString = mSavedState.bInString;

        mLineNumber = mSavedState.FileLine;
        mpLinePtr = mSavedState.pStartOfLine;
    }

    ~FxTokenizer();

private:
    bool IsWhitespace(char ch) { return (ch == ' ' || ch == '\t' || IsNewline(ch) || ch == '\r'); }

    bool IsNewline(char ch)
    {
        const bool is_newline = (ch == '\n');
        if (is_newline) {
            ++mLineNumber;
            mpLinePtr = mpData;
        }
        return is_newline;
    }

private:
    State mSavedState;

    char* mpDataStart = nullptr;
    char* mpData = nullptr;
    char* mpDataEnd = nullptr;

    bool mbInString = false;

    uint32 mLineNumber = 0;
    char* mpLinePtr = nullptr;

    FxPagedArray<FxToken> mTokens;
};


template <>
struct std::formatter<FxToken>
{
    auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(const FxToken& obj, std::format_context& ctx) const
    {
        const std::string str(obj.Start, obj.Length);
        return std::format_to(ctx.out(), "(Type={}, {})", FxToken::GetTypeName(obj.Type), str);
    }
};
