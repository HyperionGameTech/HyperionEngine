#include "FxConfigFile.hpp"

#include <Core/FxFile.hpp>
#include <Core/FxHash.hpp>
#include <Core/FxPagedArray.hpp>
#include <Math/FxQuat.hpp>
#include <Math/FxVec3.hpp>
#include <Math/FxVec4.hpp>
#include <Util/FxTokenizer.hpp>
#include <string>


/////////////////////////////////////
// Config entry functions
/////////////////////////////////////


FxConfigEntry& FxConfigEntry::operator=(FxConfigEntry&& other)
{
    if (other.Type == FxConfigEntry::ValueType::eString) {
        mStringValue = other.mStringValue;
        other.mStringValue = nullptr;
    }
    else {
        Set(other);
    }

    Type = other.Type;
    other.Type = FxConfigEntry::ValueType::eNone;

    Members = std::move(other.Members);
    ArrayData = std::move(other.ArrayData);

    bIsArray = other.bIsArray;
    other.bIsArray = false;

    Name = other.Name;
    other.Name.Clear();

    return *this;
}

FxConfigEntry::FxConfigEntry(FxConfigEntry&& other) { (*this) = std::move(other); }

void FxConfigEntry::AddMember(FxConfigEntry&& entry)
{
    if (!Members.IsInited()) {
        Members.Create();
    }

    Members.Insert(std::move(entry));
}

std::string FxConfigValue::AsString() const
{
    switch (Type) {
    case ValueType::eNone:
        return "";
    case ValueType::eInt:
        return std::to_string(mIntValue);
    case ValueType::eFloat:
        return std::to_string(mFloatValue);
    case ValueType::eString:
        return std::format("\"{}\"", mStringValue);
    case ValueType::eStruct:
        break;
    }

    return "";
}

std::string FxConfigEntry::AsString(uint32 indent) const
{
    std::string member_list = "";

    std::string indent_str = "";

    for (uint32 i = 0; i < indent; i++) {
        indent_str += '\t';
    }


    if (Type == ValueType::eStruct) {
        for (const FxConfigEntry& entry : Members) {
            member_list += std::format("{}\t{} = {}\n", indent_str, entry.Name.Get(), entry.AsString(indent + 1));
        }

        return std::format("{{\n{}{}}}", member_list, indent_str);
    }
    else if (bIsArray) {
        for (const FxConfigValue& value : ArrayData) {
            member_list += std::format("{}, ", value.AsString());
        }

        return std::format("[ {} ]", member_list);
    }


    return this->FxConfigValue::AsString();
}

FxConfigEntry* FxConfigEntry::GetMember(const FxHash64 name_hash) const
{
    for (FxConfigEntry& entry : Members) {
        if (entry.Name == name_hash) {
            return &entry;
        }
    }

    return nullptr;
}


void FxConfigEntry::AppendValue(const FxVec3f& vec)
{
    if (!Members.IsInited()) {
        Members.Create(4);
    }

    AppendValue(vec.X);
    AppendValue(vec.Y);
    AppendValue(vec.Z);
}

void FxConfigEntry::AppendValue(const FxVec4f& vec)
{
    if (!Members.IsInited()) {
        Members.Create(5);
    }

    AppendValue(vec.X);
    AppendValue(vec.Y);
    AppendValue(vec.Z);
    AppendValue(vec.W);
}


void FxConfigEntry::AppendValue(const FxQuat& quat)
{
    if (!Members.IsInited()) {
        Members.Create(5);
    }

    AppendValue(quat.X);
    AppendValue(quat.Y);
    AppendValue(quat.Z);
    AppendValue(quat.W);
}


FxVec3f FxConfigEntry::GetVec3f() const
{
    FxAssert(bIsArray == true && ArrayData.Size() >= 3);

    return FxVec3f(ArrayData[0].Get<float32>(), ArrayData[1].Get<float32>(), ArrayData[2].Get<float32>());
}


FxQuat FxConfigEntry::GetQuat() const
{
    FxAssert(bIsArray == true && ArrayData.Size() >= 4);

    return FxQuat(ArrayData[0].Get<float32>(), ArrayData[1].Get<float32>(), ArrayData[2].Get<float32>(),
                  ArrayData[3].Get<float32>());
}


FxConfigEntry::~FxConfigEntry()
{
    if (Type == ValueType::eString && mStringValue != nullptr) {
        free(mStringValue);
    }

    Type = ValueType::eNone;

    mStringValue = nullptr;
    if (Members.IsInited()) {
        Members.Destroy();
    }
}


/////////////////////////////////////
// Config file functions
/////////////////////////////////////

void FxConfigFile::Load(const std::string& path)
{
    FxFile file(path.c_str(), FxFile::eRead, FxFile::eBinary);

    if (!file.IsFileOpen()) {
        return;
    }

    InitConstants();

    FxSlice<char> file_buffer = file.Read<char>();

    FxTokenizer tokenizer(file_buffer.pData, file_buffer.Size);
    tokenizer.Tokenize();

    Parse(tokenizer.GetTokens());
}

static FxConfigEntry::ValueType GetValueTokenType(const FxToken& token)
{
    using VType = FxConfigEntry::ValueType;

    VType current_type = VType::eNone;

    FxToken::IsNumericResult numeric_result = token.IsNumeric();
    if (numeric_result != FxToken::IsNumericResult::eNaN) {
        if (numeric_result == FxToken::IsNumericResult::eInteger) {
            return VType::eInt;
        }
        else {
            return VType::eFloat;
        }
    }

    for (uint32 i = 0; i < token.Length; i++) {
        char ch = token.Start[i];

        if (ch == '"') {
            current_type = VType::eString;
        }
    }

    return current_type;
}

bool FxConfigFile::EatToken(FxTokenType type)
{
    const bool correct_token = (GetToken()->Type == type);

    if (!correct_token) {
        FxLogError("Config: Expected '{}' but found '{}'", FxToken::GetTypeName(type),
                   FxToken::GetTypeName(GetToken()->Type));
        return false;
    }

    NextToken();

    return true;
}

void FxConfigFile::PrintEntries()
{
    FxPagedArray<FxConfigEntry>& entries = GetEntries();

    for (const FxConfigEntry& entry : entries) {
        FxLogInfo("Entry: {} -> {}", entry.Name.Get(), entry.AsString());
    }
}

void FxConfigFile::ParseValue(FxConfigValue& value)
{
    using VType = FxConfigEntry::ValueType;

    FxToken* value_token = GetToken();

    // Check for constants
    if (value_token->Type == FxTokenType::eIdentifier) {
        for (const FxConfigEntry& entry : mConstants) {
            if (entry.Name == value_token->GetHash()) {
                value.Set(entry);
                NextToken();
                return;
            }
        }

        FxLogError("Could not find reference to constant {}!", value_token->GetStr());
    }

    // Handle unary minus (in a simple way, but still handles recursive negatives)
    if (value_token->Type == FxTokenType::eMinus) {
        EatToken(FxTokenType::eMinus);

        FxConfigValue temp;
        ParseValue(temp);

        switch (temp.Type) {
        case VType::eInt:
            value.Set<int64>(-temp.Get<int64>());
            break;
        case VType::eFloat:
            value.Set<float32>(-temp.Get<float32>());
            break;
        default:
            break;
        }

        return;
    }

    value.Type = GetValueTokenType(*value_token);


    switch (value.Type) {
    case VType::eNone:
        break;
    case VType::eString:
        // Remove the first quote
        value_token->Start++;
        value_token->Length--;

        // Remove the ending quote
        value_token->Length--;
        value.Set(value_token->GetStr());
        break;
    case VType::eInt:
        value.Set<int64>(value_token->ToInt());
        break;
    case VType::eFloat:
        value.Set<float32>(value_token->ToFloat());
        break;
    default:
        break;
    }

    NextToken();
}

void FxConfigEntry::AppendValue(FxConfigValue&& value)
{
    if (!ArrayData.IsInited()) {
        ArrayData.Create(8);
    }

    ArrayData.Insert(std::move(value));
}

FxConfigEntry FxConfigFile::ParseEntry()
{
    // [IDENTIFIER] = [INT | FLOAT | STRING | STRUCT]

    FxConfigEntry entry;

    FxToken* token = GetToken();

    entry.Name = token->GetStr();

    EatToken(FxTokenType::eIdentifier);
    EatToken(FxTokenType::eEquals);

    // Parse struct
    // [IDENTIFIER] = { [ENTRY]... }
    if (GetToken()->Type == FxTokenType::eLBrace) {
        EatToken(FxTokenType::eLBrace);

        entry.Type = FxConfigEntry::ValueType::eStruct;

        // Add each entry as a member of the current entry
        while (GetToken()->Type != FxTokenType::eRBrace) {
            FxConfigEntry member = ParseEntry();
            entry.AddMember(std::move(member));
        }

        EatToken(FxTokenType::eRBrace);

        return entry;
    }

    // Parse array
    else if (GetToken()->Type == FxTokenType::eLBracket) {
        EatToken(FxTokenType::eLBracket);

        entry.bIsArray = true;

        FxToken* value_token = GetToken();
        entry.Type = GetValueTokenType(*value_token);

        while (GetToken()->Type != FxTokenType::eRBracket) {
            FxConfigValue value;
            value.Type = entry.Type;
            ParseValue(value);
            entry.AppendValue(std::move(value));

            if (GetToken()->Type == FxTokenType::eRBracket) {
                break;
            }

            EatToken(FxTokenType::eComma);
        }

        EatToken(FxTokenType::eRBracket);

        return entry;
    }

    // Parse single value entry
    // [IDENTIFIER] = [INT | FLOAT | STRING]

    FxToken* value_token = GetToken();
    entry.Type = GetValueTokenType(*value_token);
    ParseValue(entry);

    return entry;
}

void FxConfigFile::Parse(FxPagedArray<FxToken>& tokens)
{
    mConfigEntries.Create(32);

    mpTokens = &tokens;

    const uint32 tokens_size = tokens.Size();

    while (mTokenIndex < tokens_size) {
        mConfigEntries.Insert(ParseEntry());
    }

    mpTokens = nullptr;
}

FxConfigEntry* FxConfigFile::GetEntry(FxHash64 requested_name_hash) const
{
    for (FxConfigEntry& entry : mConfigEntries) {
        if (entry.Name == requested_name_hash) {
            return &entry;
        }
    }

    return nullptr;
}


void FxConfigFile::Write(const std::string& path)
{
    FxFile file(path.c_str(), FxFile::eWrite, FxFile::eText);

    if (!file.IsFileOpen()) {
        return;
    }

    for (const FxConfigEntry& entry : mConfigEntries) {
        file.WriteMulti(entry.Name.Get(), " = ", entry.AsString(), '\n');
    }

    file.Close();
}

void FxConfigFile::InitConstants()
{
    constexpr uint32 cMaxConstants = 16;

    mConstants.InitCapacity(cMaxConstants);

    mConstants.Insert(FxConfigEntry("TRUE", 1));
    mConstants.Insert(FxConfigEntry("FALSE", 0));

    mConstants.Insert(FxConfigEntry("OBJLAYER_WORLD", 0));
    mConstants.Insert(FxConfigEntry("OBJLAYER_PLAYER", 1));

    mConstants.Insert(FxConfigEntry("PHYS_STATIC", 0));
    mConstants.Insert(FxConfigEntry("PHYS_DYNAMIC", 1));
}
