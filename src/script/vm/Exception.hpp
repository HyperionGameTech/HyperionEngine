#pragma once

#include <core/Name.hpp>

#include <util/UTF8.hpp>

namespace hyperion {

class Script_Value;

class Script_Exception
{
public:
    Script_Exception(const char* str);
    Script_Exception(const Script_Exception& other);
    Script_Exception& operator=(const Script_Exception& other);
    Script_Exception(Script_Exception&& other) noexcept;
    Script_Exception& operator=(Script_Exception&& other) noexcept;
    ~Script_Exception();

    const char* ToString() const
    {
        return m_str;
    }

    static Script_Exception InvalidComparisonException(const char* leftTypeStr, const char* rightTypeStr);
    static Script_Exception InvalidOperationException(const char* opName,
        const char* leftTypeStr, const char* rightTypeStr);
    static Script_Exception InvalidOperationException(const char* opName, const char* typeStr);
    static Script_Exception InvalidCastException(const char* fromTypeStr, const char* toTypeStr);
    static Script_Exception InvalidBitwiseArgument();
    static Script_Exception InvalidArgsException(int expected, int received, bool variadic = false);
    static Script_Exception InvalidArgsException(const char* expectedStr, int received);
    static Script_Exception InvalidArgsException(const char* expectedStr);
    static Script_Exception InvalidConstructorException();
    static Script_Exception NullReferenceException();
    static Script_Exception DivisionByZeroException();
    static Script_Exception OutOfBoundsException(SizeType index, SizeType size);
    static Script_Exception MemberNotFoundException(Script_Value* pValue, HashCode::ValueType hashCode);
    static Script_Exception InvalidMemberAccessException(Script_Value* pValue);
    static Script_Exception FileOpenException(const char* fileName);
    static Script_Exception UnopenedFileWriteException();
    static Script_Exception UnopenedFileReadException();
    static Script_Exception UnopenedFileCloseException();
    static Script_Exception LibraryLoadException(const char* libName);
    static Script_Exception LibraryFunctionLoadException(const char* funcName);
    static Script_Exception DuplicateExportException();
    static Script_Exception KeyNotFoundException(const char* key);
    static Script_Exception ClassNotFoundException(const char* className);

private:
    char* m_str;
};

} // namespace hyperion
