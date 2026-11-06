/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/DataProcessing/Shared/SourceLocation.hpp>
#include <Core/DataProcessing/Shared/Token.hpp>
#include <Core/DataProcessing/Shared/TokenStream.hpp>
#include <Core/DataProcessing/Shared/ErrorList.hpp>

#include <Core/Reflection/BoxedValueFwd.hpp>
#include <Core/Reflection/TypeInfoFwd.hpp>

#include <Core/Containers/String.hpp>
#include <Core/Types.hpp>

namespace Hyperion::DataProcessing {

enum ErrorMessage : uint8;

class CompilerError;

} // namespace Hyperion::DataProcessing

namespace Hyperion::DataProcessing::HMF {

struct ParseOptions;

class Parser
{
public:
    Parser(
        TokenStream* tokenStream,
        DataProcessing::ErrorList<CompilerError>* errorList,
        BoxedValue* target = nullptr,
        const ParseOptions* options = nullptr);

    Parser(const Parser& other) = delete;
    Parser& operator=(const Parser& other) = delete;

    ~Parser();

    bool Parse();
    bool Parse(BoxedValue& out, bool moveResult = true);

    /*! \brief Parses the whole token stream as one value of the given type */
    bool ParseSingleValue(const TypeInfo& typeInfo, BoxedValue& out);

private:
    bool ParseObjectBody(const Class* cls, BoxedValue& target, const UTF8StringView& objectName = UTF8StringView {});
    bool ParseValue(const TypeInfo& typeInfo, BoxedValue& out);

    bool ParseSchemaSection(const Class* cls, BoxedValue& target, const ANSIStringView& sectionName);

    bool ParseBoolValue(BoxedValue& out);
    bool ParseIntegralValue(const TypeInfo& typeInfo, BoxedValue& out);
    bool ParseFloatValue(const TypeInfo& typeInfo, BoxedValue& out);
    bool ParseStringValue(const TypeInfo& typeInfo, BoxedValue& out);
    bool ParseEnumValue(const TypeInfo& typeInfo, BoxedValue& out);
    bool ParseEnumFlagsValue(const TypeInfo& typeInfo, BoxedValue& out);
    bool ParseArrayValue(const TypeInfo& typeInfo, BoxedValue& out);
    bool ParseVectorValue(const TypeInfo& typeInfo, BoxedValue& out);
    bool ParseMapValue(const TypeInfo& typeInfo, BoxedValue& out);
    bool ParseSetValue(const TypeInfo& typeInfo, BoxedValue& out);
    bool ParsePairValue(const TypeInfo& typeInfo, BoxedValue& out);
    bool ParseTupleValue(const TypeInfo& typeInfo, BoxedValue& out);
    bool ParseMatrixValue(const TypeInfo& typeInfo, BoxedValue& out);
    bool ParseObjectValue(const TypeInfo& typeInfo, BoxedValue& out);
    bool ParseVariantValue(const TypeInfo& typeInfo, BoxedValue& out);
    bool ParseAssetPathLiteral(const TypeInfo& typeInfo, BoxedValue& out);

    void SkipValue();
    void SkipBracedBlock();
    void SkipBracketedBlock();

    /*! \brief Consumes the rest of an object value (after its class name) and renders it back to HMF source text. */
    String CaptureObjectSource(UTF8StringView className);

    /*! \brief Consumes the next value and renders it back to HMF source text. */
    String CaptureValueSource();

    bool IsClassNamedObject() const;

    bool ResolveEnumName(const Class* enumClass, UTF8StringView name, BoxedValue& outValue);

    bool Match(TokenClass tokenClass);
    bool Expect(TokenClass tokenClass, const char* what);
    bool ExpectIdentifier(UTF8StringView& outName);
    bool IsIdentKeyword(const Token& token, const char* keyword) const;

    void Error(ErrorMessage msg, const SourceLocation& loc);
    void Error(ErrorMessage msg, const SourceLocation& loc, const String& arg1);
    void Error(ErrorMessage msg, const SourceLocation& loc, const String& arg1, const String& arg2);

    void Warning(ErrorMessage msg, const SourceLocation& loc, const String& arg1);
    void Warning(ErrorMessage msg, const SourceLocation& loc, const String& arg1, const String& arg2);

    HYP_FORCE_INLINE const Token& Peek(int n = 0) const
    {
        return m_tokenStream->Peek(n);
    }

    HYP_FORCE_INLINE const Token& Next()
    {
        return m_tokenStream->Next();
    }

    TokenStream* m_tokenStream;
    DataProcessing::ErrorList<CompilerError>* m_errorList;
    BoxedValue* m_target;
    bool m_ownsTarget;
    const ParseOptions* m_options;
    uint32 m_objectDepth;
};

} // namespace Hyperion::DataProcessing::HMF
