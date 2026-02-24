#include <script/vm/Exception.hpp>
#include <script/vm/Value.hpp>

namespace Hyperion {

template <class FormatStringType, class... Args>
static inline Script_Exception FormattedException(FormatStringType formatString, Args&&... args)
{
    char buffer[256];
    int n = std::snprintf(buffer, HYP_ARRAY_SIZE(buffer), formatString.Data(), std::forward<Args>(args)...);

    if (n >= HYP_ARRAY_SIZE(buffer))
    {
        // recreate buffer using dynamic allocation
        const SizeType size = SizeType(n) + 1;

        char* dynamicBuffer = (char*)std::malloc(size);
        std::snprintf(dynamicBuffer, size, formatString.Data(), std::forward<Args>(args)...);

        Script_Exception exc(dynamicBuffer);

        std::free(dynamicBuffer);

        return exc;
    }

    return Script_Exception(buffer);
}

Script_Exception::Script_Exception(const char* str)
{
    const SizeType len = std::strlen(str);
    m_str = new char[len + 1];
    std::strcpy(m_str, str);
}

Script_Exception::Script_Exception(const Script_Exception& other)
{
    const SizeType len = std::strlen(other.m_str);
    m_str = new char[len + 1];
    std::strcpy(m_str, other.m_str);
}

Script_Exception::Script_Exception(Script_Exception&& other) noexcept
    : m_str(other.m_str)
{
    other.m_str = nullptr;
}

Script_Exception& Script_Exception::operator=(const Script_Exception& other)
{
    if (this != &other)
    {
        delete[] m_str;
        const SizeType len = std::strlen(other.m_str);
        m_str = new char[len + 1];
        std::strcpy(m_str, other.m_str);
    }

    return *this;
}

Script_Exception& Script_Exception::operator=(Script_Exception&& other) noexcept
{
    if (this != &other)
    {
        delete[] m_str;
        m_str = other.m_str;
        other.m_str = nullptr;
    }

    return *this;
}

Script_Exception::~Script_Exception()
{
    delete[] m_str;
}

Script_Exception Script_Exception::InvalidComparisonException(
    const char* leftTypeStr,
    const char* rightTypeStr)
{
    return FormattedException(
        HYP_STATIC_STRING("Invalid comparison between types %s and %s"),
        leftTypeStr,
        rightTypeStr);
}

Script_Exception Script_Exception::InvalidOperationException(
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

Script_Exception Script_Exception::InvalidOperationException(const char* opName, const char* typeStr)
{
    return FormattedException(
        HYP_STATIC_STRING("Invalid operation (%s) on type %s"),
        opName,
        typeStr);
}

Script_Exception Script_Exception::InvalidNewException(const char* typeStr)
{
    return FormattedException(
        HYP_STATIC_STRING("Could not create an instance of %s"),
        typeStr);
}

Script_Exception Script_Exception::InvalidCastException(const char* fromTypeStr, const char* toTypeStr)
{
    return FormattedException(
        HYP_STATIC_STRING("Invalid cast: cannot cast from %s to %s"),
        fromTypeStr,
        toTypeStr);
}

Script_Exception Script_Exception::InvalidBitwiseArgument()
{
    return Script_Exception("Invalid argument to bitwise operation");
}

Script_Exception Script_Exception::InvalidArgsException(int expected, int received, bool variadic)
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
    return Script_Exception(buffer);
}

Script_Exception Script_Exception::InvalidArgsException(const char* expectedStr, int received)
{
    char buffer[256];
    std::sprintf(buffer, "Invalid arguments: expected %s, received %d", expectedStr, received);
    return Script_Exception(buffer);
}

Script_Exception Script_Exception::InvalidArgsException(const char* expectedStr)
{
    char buffer[256];
    std::sprintf(buffer, "Invalid arguments: expected %s", expectedStr);
    return Script_Exception(buffer);
}

Script_Exception Script_Exception::InvalidConstructorException()
{
    return Script_Exception("Invalid constructor");
}

Script_Exception Script_Exception::NullReferenceException()
{
    return Script_Exception("Null object dereferenced");
}

Script_Exception Script_Exception::DivisionByZeroException()
{
    return Script_Exception("Division by zero");
}

Script_Exception Script_Exception::OutOfBoundsException(SizeType index, SizeType size)
{
    char buffer[256];
    std::snprintf(buffer, 256, "Index out of array bounds! Index: %llu, size: %llu", index, size);

    return Script_Exception(buffer);
}

Script_Exception Script_Exception::MemberNotFoundException(BoxedValue* pValue, HashCode::ValueType hashCode)
{
    return FormattedException(
        HYP_STATIC_STRING("Member with hash code %llu not found on type: `%s`"),
        hashCode,
        pValue ? GetTypeString(*pValue) : "<null>");
}

Script_Exception Script_Exception::InvalidMemberAccessException(BoxedValue* pValue)
{
    return FormattedException(
        HYP_STATIC_STRING("Invalid member access on type `%s`!"),
        pValue ? GetTypeString(*pValue) : "<null>");
}

Script_Exception Script_Exception::FileOpenException(const char* fileName)
{
    return FormattedException(
        HYP_STATIC_STRING("Failed to open file `%s`"),
        fileName);
}

Script_Exception Script_Exception::UnopenedFileWriteException()
{
    return Script_Exception("Attempted to write to an unopened file");
}

Script_Exception Script_Exception::UnopenedFileReadException()
{
    return Script_Exception("Attempted to read from an unopened file");
}

Script_Exception Script_Exception::UnopenedFileCloseException()
{
    return Script_Exception("Attempted to close an unopened file");
}

Script_Exception Script_Exception::LibraryLoadException(const char* libName)
{
    return FormattedException(
        HYP_STATIC_STRING("Failed to open library `%s`"),
        libName);
}

Script_Exception Script_Exception::LibraryFunctionLoadException(const char* funcName)
{
    return FormattedException(
        HYP_STATIC_STRING("Failed to load function `%s` from library"),
        funcName);
}

Script_Exception Script_Exception::DuplicateExportException()
{
    return Script_Exception("Duplicate export");
}

Script_Exception Script_Exception::KeyNotFoundException(const char* key)
{
    return FormattedException(
        HYP_STATIC_STRING("Key `%s` not found"),
        key ? key : "<null>");
}

Script_Exception Script_Exception::ClassNotFoundException(const char* className)
{
    return FormattedException(
        HYP_STATIC_STRING("Class `%s` not found"),
        className ? className : "<null>");
}

} // namespace Hyperion
