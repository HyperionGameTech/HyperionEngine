#pragma once

#include <Core/name/Name.hpp>

#include <Core/Unicode.hpp>

namespace Hyperion {

struct BoxedValue;

class Exception
{
    enum TakeOwnershipOfStringPointerTag
    {
        TakeOwnershipOfStringPointer
    };
    
    template <class FormatStringType, class... Args>
    static Exception FormattedException(FormatStringType formatString, Args... args);
    
    Exception(TakeOwnershipOfStringPointerTag, char* str);

public:
    explicit Exception(const char* str);

    Exception(const Exception& other);
    Exception& operator=(const Exception& other);

    Exception(Exception&& other) noexcept;
    Exception& operator=(Exception&& other) noexcept;

    ~Exception();

    const char* ToString() const
    {
        return m_str;
    }

    static Exception InvalidComparisonException(const char* leftTypeStr, const char* rightTypeStr);
    static Exception InvalidOperationException(const char* opName,
        const char* leftTypeStr, const char* rightTypeStr);
    static Exception InvalidOperationException(const char* opName, const char* typeStr);
    static Exception InvalidNewException(const char* typeStr);
    static Exception InvalidCastException(const char* fromTypeStr, const char* toTypeStr);
    static Exception InvalidBitwiseArgument();
    static Exception InvalidArgsException(int expected, int received, bool variadic = false);
    static Exception InvalidArgsException(const char* expectedStr, int received);
    static Exception InvalidArgsException(const char* expectedStr);
    static Exception InvalidConstructorException();
    static Exception NullReferenceException();
    static Exception DivisionByZeroException();
    static Exception OutOfBoundsException(size_t index, size_t size);
    static Exception MemberNotFoundException(BoxedValue* pValue, HashCode::ValueType hashCode);
    static Exception InvalidMemberAccessException(BoxedValue* pValue);
    static Exception FileOpenException(const char* fileName);
    static Exception UnopenedFileWriteException();
    static Exception UnopenedFileReadException();
    static Exception UnopenedFileCloseException();
    static Exception LibraryLoadException(const char* libName);
    static Exception LibraryFunctionLoadException(const char* funcName);
    static Exception DuplicateExportException();
    static Exception KeyNotFoundException(const char* key);
    static Exception ClassNotFoundException(const char* className);

private:
    char* m_str;
};

} // namespace Hyperion
