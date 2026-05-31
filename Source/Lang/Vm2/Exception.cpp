#include <Lang/vm/Exception.hpp>
#include <Lang/vm/Value.hpp>
#include <Lang/vm/ScriptMemory.hpp>

#include <Core/memory/pool/Pool.hpp>

namespace Hyperion {

template <class FormatStringType, class... Args>
Exception Exception::FormattedException(FormatStringType formatString, Args... args)
{
    char buffer[256];
    int n = std::snprintf(buffer, HYP_ARRAY_SIZE(buffer), formatString.Data(), args...);

    if (n >= HYP_ARRAY_SIZE(buffer))
    {
        // recreate buffer using dynamic allocation
        const size_t size = size_t(n) + 1;

        char* dynamicBuffer = (char*)g_scriptPool->Allocate(size);
        AssertDebug(dynamicBuffer != nullptr);

        std::snprintf(dynamicBuffer, size, formatString.Data(), args...);

        return { Exception::TakeOwnershipOfStringPointer, dynamicBuffer };
    }

    return Exception(buffer);
}

Exception::Exception(TakeOwnershipOfStringPointerTag, char* str)
    : m_str(str)
{
}

Exception::Exception(const char* str)
{
    const size_t len = std::strlen(str);

    m_str = (char*)g_scriptPool->Allocate(len + 1);
    AssertDebug(m_str != nullptr);

    std::strncpy(m_str, str, len);
    m_str[len] = '\0';
}

Exception::Exception(const Exception& other)
{
    const size_t len = std::strlen(other.m_str);
    m_str = (char*)g_scriptPool->Allocate(len + 1);
    AssertDebug(m_str != nullptr);

    std::strncpy(m_str, other.m_str, len);
    m_str[len] = '\0';
}

Exception::Exception(Exception&& other) noexcept
    : m_str(other.m_str)
{
    other.m_str = nullptr;
}

Exception& Exception::operator=(const Exception& other)
{
    if (this != &other)
    {
        g_scriptPool->Free(m_str);
        const size_t len = std::strlen(other.m_str);

        m_str = (char*)g_scriptPool->Allocate(len + 1);
        AssertDebug(m_str != nullptr);

        std::strncpy(m_str, other.m_str, len);
        m_str[len] = '\0';
    }

    return *this;
}

Exception& Exception::operator=(Exception&& other) noexcept
{
    if (this != &other)
    {
        g_scriptPool->Free(m_str);

        m_str = other.m_str;
        other.m_str = nullptr;
    }

    return *this;
}

Exception::~Exception()
{
    g_scriptPool->Free(m_str);
}

Exception Exception::InvalidComparisonException(
    const char* leftTypeStr,
    const char* rightTypeStr)
{
    return FormattedException(
        HYP_STATIC_STRING("Invalid comparison between types %s and %s"),
        leftTypeStr,
        rightTypeStr);
}

Exception Exception::InvalidOperationException(
    const char* opName,
    const char* leftTypeStr,
    const char* rightTypeStr)
{
    return FormattedException(
        HYP_STATIC_STRING("Invalid operation (%s) between types %s and %s"),
        opName,
        leftTypeStr,
        rightTypeStr);
}

Exception Exception::InvalidOperationException(const char* opName, const char* typeStr)
{
    return FormattedException(
        HYP_STATIC_STRING("Invalid operation (%s) on type %s"),
        opName,
        typeStr);
}

Exception Exception::InvalidNewException(const char* typeStr)
{
    return FormattedException(
        HYP_STATIC_STRING("Could not create an instance of %s"),
        typeStr);
}

Exception Exception::InvalidCastException(const char* fromTypeStr, const char* toTypeStr)
{
    return FormattedException(
        HYP_STATIC_STRING("Invalid cast: cannot cast from %s to %s"),
        fromTypeStr,
        toTypeStr);
}

Exception Exception::InvalidBitwiseArgument()
{
    return Exception("Invalid argument to bitwise operation");
}

Exception Exception::InvalidArgsException(int expected, int received, bool variadic)
{
    char buffer[256];
    if (variadic)
    {
        std::sprintf(buffer, "Invalid arguments: expected at least %d, received %d", expected, received);
    }
    else
    {
        std::sprintf(buffer, "Invalid arguments: expected %d, received %d", expected, received);
    }
    return Exception(buffer);
}

Exception Exception::InvalidArgsException(const char* expectedStr, int received)
{
    char buffer[256];
    std::sprintf(buffer, "Invalid arguments: expected %s, received %d", expectedStr, received);
    return Exception(buffer);
}

Exception Exception::InvalidArgsException(const char* expectedStr)
{
    char buffer[256];
    std::sprintf(buffer, "Invalid arguments: expected %s", expectedStr);
    return Exception(buffer);
}

Exception Exception::InvalidConstructorException()
{
    return Exception("Invalid constructor");
}

Exception Exception::NullReferenceException()
{
    return Exception("Null object dereferenced");
}

Exception Exception::DivisionByZeroException()
{
    return Exception("Division by zero");
}

Exception Exception::OutOfBoundsException(size_t index, size_t size)
{
    char buffer[256];
    std::snprintf(buffer, 256, "Index out of array bounds! Index: %llu, size: %llu", index, size);

    return Exception(buffer);
}

Exception Exception::MemberNotFoundException(BoxedValue* pValue, HashCode::ValueType hashCode)
{
    return FormattedException(
        HYP_STATIC_STRING("Member with hash code %llu not found on type: `%s`"),
        hashCode,
        pValue ? GetTypeString(*pValue) : "<null>");
}

Exception Exception::InvalidMemberAccessException(BoxedValue* pValue)
{
    return FormattedException(
        HYP_STATIC_STRING("Invalid member access on type `%s`!"),
        pValue ? GetTypeString(*pValue) : "<null>");
}

Exception Exception::FileOpenException(const char* fileName)
{
    return FormattedException(
        HYP_STATIC_STRING("Failed to open file `%s`"),
        fileName);
}

Exception Exception::UnopenedFileWriteException()
{
    return Exception("Attempted to write to an unopened file");
}

Exception Exception::UnopenedFileReadException()
{
    return Exception("Attempted to read from an unopened file");
}

Exception Exception::UnopenedFileCloseException()
{
    return Exception("Attempted to close an unopened file");
}

Exception Exception::LibraryLoadException(const char* libName)
{
    return FormattedException(
        HYP_STATIC_STRING("Failed to open library `%s`"),
        libName);
}

Exception Exception::LibraryFunctionLoadException(const char* funcName)
{
    return FormattedException(
        HYP_STATIC_STRING("Failed to load function `%s` from library"),
        funcName);
}

Exception Exception::DuplicateExportException()
{
    return Exception("Duplicate export");
}

Exception Exception::KeyNotFoundException(const char* key)
{
    return FormattedException(
        HYP_STATIC_STRING("Key `%s` not found"),
        key ? key : "<null>");
}

Exception Exception::ClassNotFoundException(const char* className)
{
    return FormattedException(
        HYP_STATIC_STRING("Class `%s` not found"),
        className ? className : "<null>");
}

} // namespace Hyperion
