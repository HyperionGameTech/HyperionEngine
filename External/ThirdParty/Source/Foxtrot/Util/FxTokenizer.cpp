#include "FxTokenizer.hpp"

#include <Core/MemPool/FxMemPool.hpp>
#include <FxEngine.hpp>


const char* FxToken::GetTypeName(FxTokenType type)
{
    const char* type_names[] = {
        "Unknown",    "Identifier",

        "String",     "Integer",    "Float",

        "Equals",

        "LParen",     "RParen",

        "LBracket",   "RBracket",

        "LBrace",     "RBrace",

        "Plus",       "Dollar",     "Minus",

        "Question",

        "Dot",        "Comma",      "Semicolon",

        "DocComment",
    };

    return type_names[static_cast<uint32>(type)];
}


FxToken::IsNumericResult FxToken::IsNumeric() const
{
    char ch;

    FxToken::IsNumericResult result = FxToken::IsNumericResult::eNaN;

    for (int i = 0; i < Length; i++) {
        ch = Start[i];

        // If there is a number preceding the dot then we are a frfunctional
        if (ch == '.' && result != FxToken::IsNumericResult::eNaN) {
            result = FxToken::IsNumericResult::eFractional;
            continue;
        }

        if ((ch >= '0' && ch <= '9')) {
            // If no numbers have been found yet then set to integer
            if (result == FxToken::IsNumericResult::eNaN) {
                result = FxToken::IsNumericResult::eInteger;
            }
            continue;
        }

        // Not a number
        return FxToken::IsNumericResult::eNaN;
    }

    // Is numeric
    return result;
}


/////////////////////////////////////
// Tokenizer
/////////////////////////////////////

FxTokenType FxTokenizer::GetTokenType(FxToken& token)
{
    if (token.Type == FxTokenType::eDocComment) {
        return FxTokenType::eDocComment;
    }

    // Check if the token is a number
    FxToken::IsNumericResult is_numeric = token.IsNumeric();

    switch (is_numeric) {
    case FxToken::IsNumericResult::eInteger:
        return FxTokenType::eInteger;
    case FxToken::IsNumericResult::eFractional:
        return FxTokenType::eFloat;
    case FxToken::IsNumericResult::eNaN:
        break;
    }

    if (token.Length > 2) {
        // Checks if the token is a string
        if (token.Start[0] == '"' && token.Start[token.Length - 1] == '"') {
            return FxTokenType::eString;
        }
    }

    if (token.Length == 1) {
        switch (token.Start[0]) {
        case '=':
            return FxTokenType::eEquals;
        case '(':
            return FxTokenType::eLParen;
        case ')':
            return FxTokenType::eRParen;
        case '[':
            return FxTokenType::eLBracket;
        case ']':
            return FxTokenType::eRBracket;
        case '{':
            return FxTokenType::eLBrace;
        case '}':
            return FxTokenType::eRBrace;
        case '+':
            return FxTokenType::ePlus;
        case '-':
            return FxTokenType::eMinus;
        case '$':
            return FxTokenType::eDollar;
        case '.':
            return FxTokenType::eDot;
        case ',':
            return FxTokenType::eComma;
        case ';':
            return FxTokenType::eSemicolon;
        }
    }

    return FxTokenType::eIdentifier;
}


void FxTokenizer::SubmitTokenIfData(FxToken& token, char* end_ptr, char* start_ptr)
{
    if (token.IsEmpty()) {
        return;
    }

    if (end_ptr == nullptr) {
        end_ptr = mpData;
    }

    if (start_ptr == nullptr) {
        start_ptr = mpData;
    }

    token.End = mpData;
    token.Type = GetTokenType(token);

    mTokens.Insert(token);
    token.Clear();

    token.Start = mpData;

    uint16 column = 1;
    if (mpLinePtr) {
        column = static_cast<uint16>((mpData - mpLinePtr));
    }

    token.FileColumn = column;
    token.FileLine = mLineNumber + 1;
}


bool FxTokenizer::CheckOperators(FxToken& current_token, char ch)
{
    if (ch == '.' && current_token.IsNumeric() != FxToken::IsNumericResult::eNaN) {
        return false;
    }

    bool is_operator = (strchr(SingleCharOperators, ch) != NULL);

    if (is_operator) {
        // If there is data waiting, submit to the token list
        SubmitTokenIfData(current_token, mpData);

        // Submit the operator as its own token
        current_token.Increment();

        char* end_of_operator = mpData;
        ++mpData;

        SubmitTokenIfData(current_token, end_of_operator, mpData);

        return true;
    }

    return false;
}


uint32 FxTokenizer::ReadQuotedString(char* buffer, uint32 max_size, bool skip_on_success)
{
    char ch;
    char* data = mpData;

    uint32 buf_size = 0;

    if (data >= mpDataEnd) {
        return 0;
    }

    // Skip spaces and tabs
    while ((ch = *(data)) && (ch == ' ' || ch == '\t'))
        ++data;

    // Check if this is a string
    if (ch != '"') {
        puts("Not a string!");
        return 0;
    }

    // Eat the quote
    ++data;

    for (uint32 i = 0; i < max_size; i++) {
        ch = *data;

        if (ch == '"') {
            // Eat the quote
            ++data;
            break;
        }

        buffer[buf_size++] = ch;

        ++data;
    }

    if (skip_on_success) {
        mpData = data;
    }

    buffer[buf_size] = 0;

    return buf_size;
}


bool FxTokenizer::ExpectString(const char* expected_value, bool skip_on_success)
{
    char ch;
    int expected_index = 0;

    char* data = mpData;

    while (data < mpDataEnd && ((ch = *(data)))) {
        if (!expected_value[expected_index]) {
            break;
        }

        if (ch != expected_value[expected_index]) {
            return false;
        }

        ++expected_index;
        ++data;
    }

    if (skip_on_success) {
        mpData = data;
    }

    return true;
}


void FxTokenizer::IncludeFile(const char* path)
{
    FxFile file(path, FxFile::ModType::eRead, FxFile::DataType::eBinary);


    if (!file.IsFileOpen()) {
        FxLogError("Could not open include file '{}'", path);
        return;
    }


    // Save the current state of the tokenizer
    SaveState();

    FxSlice<char> include_data = file.Read<char>();

    SetDataPtr(include_data.pData);
    mpDataEnd = include_data.pData + include_data.Size;
    mbInString = false;

    // Tokenize all of the included file
    Tokenize();

    gEnginePool->Free<char>(mpData);

    // Restore back to previous state
    RestoreState();
}


void FxTokenizer::Tokenize()
{
    if (!mTokens.IsInited()) {
        mTokens.Create(512);
    }

    FxToken current_token;
    current_token.Start = mpData;

    bool in_comment = false;
    bool is_doccomment = false;

    char ch;

    while (mpData < mpDataEnd && ((ch = *(mpData)))) {
        if (ch == '\r') {
            ++mpData;
            continue;
        }

        if (ch == '/' && ((mpData + 1) < mpDataEnd) && ((*(mpData + 1)) == '/')) {
            SubmitTokenIfData(current_token);
            in_comment = true;

            ++mpData;
            if (*(++mpData) == '?') {
                while ((ch = *(++mpData))) {
                    if (isalnum(ch) || ch == '\n') {
                        current_token.Start = mpData;
                        break;
                    }
                }
                is_doccomment = true;
            }
        }

        if (ch == '/' && ((mpData + 1) < mpDataEnd) && ((*(mpData + 1)) == '*')) {
            SubmitTokenIfData(current_token);
            in_comment = true;

            ++mpData;

            while ((mpData + 1 < mpDataEnd) && (ch = *(++mpData))) {
                if (ch == '*' && ((mpData + 1) < mpDataEnd) && (*(mpData + 1) == '/')) {
                    current_token.Start = mpData;
                    break;
                }
            }
        }

        // If we are in a comment, skip until we hit a newline. Carriage return is eaten by our
        // below by the IsWhitespace check.
        if (in_comment) {
            if (!IsNewline(ch)) {
                ++mpData;
                if (is_doccomment) {
                    current_token.Increment();
                }
                continue;
            }

            if (is_doccomment) {
                current_token.Type = FxTokenType::eDocComment;
                SubmitTokenIfData(current_token);

                current_token.Type = FxTokenType::eUnknown;

                is_doccomment = false;
            }

            // We hit a newline, mark as no longer in comment
            in_comment = false;
        }

        if (ch == '"') {
            // If we are not currently in a string, submit the token if there is data waiting
            if (!mbInString) {
                SubmitTokenIfData(current_token);
            }

            mbInString = !mbInString;
        }

        if (mbInString) {
            ++mpData;
            current_token.Increment();
            continue;
        }

        // Internal call
        if (ch == '@') {
            ++mpData;
            TryReadInternalCall();

            current_token.Length = 0;
            current_token.Start = mpData;

            continue;
        }

        if (IsWhitespace(ch)) {
            SubmitTokenIfData(current_token);

            mpData++;
            current_token.Start = mpData;
            continue;
        }

        if (CheckOperators(current_token, ch)) {
            continue;
        }

        mpData++;
        current_token.Increment();
    }
    SubmitTokenIfData(current_token);
}


void FxTokenizer::TryReadInternalCall()
{
    if (ExpectString("include")) {
        char include_path[512];

        if (!ReadQuotedString(include_path, 512)) {
            puts("Error reading include path!");
            return;
        }

        IncludeFile(include_path);
    }
}

FxTokenizer::~FxTokenizer()
{
    gEnginePool->Free(mpDataStart);
    mTokens.Destroy();
}
