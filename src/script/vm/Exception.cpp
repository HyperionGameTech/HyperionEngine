#include <script/vm/Exception.hpp>
#include <script/vm/Value.hpp>

namespace hyperion {
namespace vm {

template <class FormatStringType, class... Args>
static inline Exception FormattedException(FormatStringType formatString, Args&&... args)
{
    char buffer[256];
    int n = std::snprintf(buffer, HYP_ARRAY_SIZE(buffer), formatString.Data(), std::forward<Args>(args)...);

    if (n >= HYP_ARRAY_SIZE(buffer))
    {
        // recreate buffer using dynamic allocation
        const SizeType size = SizeType(n) + 1;

        char* dynamicBuffer = (char*)std::malloc(size);
        std::snprintf(dynamicBuffer, size, formatString.Data(), std::forward<Args>(args)...);

        Exception exc(dynamicBuffer);

        std::free(dynamicBuffer);

        return exc;
    }

    return Exception(buffer);
}

Exception::Exception(const char* str)
{
    const SizeType len = std::strlen(str);
    m_str = new char[len + 1];
    std::strcpy(m_str, str);
}

Exception::Exception(const Exception& other)
{
    const SizeType len = std::strlen(other.m_str);
    m_str = new char[len + 1];
    std::strcpy(m_str, other.m_str);
}

Exception& Exception::operator=(const Exception& other)
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

Exception& Exception::operator=(Exception&& other) noexcept
{
    if (this != &other)
    {
        delete[] m_str;
        m_str = other.m_str;
        other.m_str = nullptr;
    }

    return *this;
}

Exception::~Exception()
{
    delete[] m_str;
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

Exception Exception::OutOfBoundsException(SizeType index, SizeType size)
{
    char buffer[256];
    std::snprintf(buffer, 256, "Index out of array bounds! Index: %llu, size: %llu", index, size);

    return Exception(buffer);
}

Exception Exception::MemberNotFoundException(HashCode::ValueType hashCode)
{
    return FormattedException(
        HYP_STATIC_STRING("Member with hash code %llu not found!"),
        hashCode);
}

Exception Exception::InvalidMemberAccessException(Value* pValue)
{
    return FormattedException(
        HYP_STATIC_STRING("Invalid member access on type `%s`!"),
        pValue ? pValue->GetTypeString() : "<null>");
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

} // namespace vm
} // namespace hyperion
