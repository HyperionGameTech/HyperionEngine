#include <script/vm/Exception.hpp>

namespace hyperion {
namespace vm {

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
    char buffer[256];
    std::snprintf(
        buffer,
        255,
        "Cannot compare %s with %s",
        leftTypeStr,
        rightTypeStr);
    return Exception(buffer);
}

Exception Exception::InvalidOperationException(
    const char* opName,
    const char* leftTypeStr,
    const char* rightTypeStr)
{
    char buffer[256];
    std::snprintf(
        buffer,
        255,
        "Invalid operation (%s) on types %s and %s",
        opName,
        leftTypeStr,
        rightTypeStr);
    return Exception(buffer);
}

Exception Exception::InvalidOperationException(const char* opName, const char* typeStr)
{
    char buffer[256];
    std::snprintf(
        buffer,
        255,
        "Invalid operation (%s) on type %s",
        opName,
        typeStr);
    return Exception(buffer);
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

Exception Exception::OutOfBoundsException()
{
    return Exception("Index out of bounds of Array");
}

Exception Exception::MemberNotFoundException(HashCode::ValueType hashCode)
{
    char buffer[256];
    std::snprintf(buffer, 256, "Member with hash code %llu not found", hashCode);

    return Exception(buffer);
}

Exception Exception::FileOpenException(const char* fileName)
{
    char buffer[256];
    std::sprintf(buffer, "Failed to open file `%s`", fileName);

    return Exception(buffer);
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
    char buffer[256];
    std::snprintf(buffer, 256, "Failed to open library `%s`", libName);

    return Exception(buffer);
}

Exception Exception::LibraryFunctionLoadException(const char* funcName)
{
    char buffer[256];
    std::snprintf(buffer, 256, "Failed to open library function `%s`", funcName);

    return Exception(buffer);
}

Exception Exception::DuplicateExportException()
{
    char buffer[256];
    std::snprintf(buffer, 256, "Duplicate exported symbol");

    return Exception(buffer);
}

Exception Exception::KeyNotFoundException(const char* key)
{
    char buffer[256];
    std::snprintf(buffer, 256, "Key `%s` not found", key);

    return Exception(buffer);
}

} // namespace vm
} // namespace hyperion
