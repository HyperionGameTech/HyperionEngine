#include <script/vm/Exception.hpp>

namespace hyperion {
namespace vm {

Exception::Exception(const char *str)
{
    const SizeType len = std::strlen(str);
    m_str = new char[len + 1];
    std::strcpy(m_str, str);
}

Exception::Exception(const Exception &other)
{
    const SizeType len = std::strlen(other.m_str);
    m_str = new char[len + 1];
    std::strcpy(m_str, other.m_str);
}

Exception &Exception::operator=(const Exception &other)
{
    if (this != &other) {
        delete[] m_str;
        const SizeType len = std::strlen(other.m_str);
        m_str = new char[len + 1];
        std::strcpy(m_str, other.m_str);
    }

    return *this;
}

Exception &Exception::operator=(Exception &&other) noexcept
{
    if (this != &other) {
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
    const char *left_type_str,
    const char *right_type_str
)
{
    char buffer[256];
    std::snprintf(
        buffer,
        255,
        "Cannot compare %s with %s",
        left_type_str,
        right_type_str
    );
    return Exception(buffer);
}

Exception Exception::InvalidOperationException(
    const char *op_name,
    const char *left_type_str,
    const char *right_type_str
)
{
    char buffer[256];
    std::snprintf(
        buffer,
        255,
        "Invalid operation (%s) on types %s and %s",
        op_name,
        left_type_str,
        right_type_str
    );
    return Exception(buffer);
}

Exception Exception::InvalidOperationException(const char *op_name, const char *type_str)
{
    char buffer[256];
    std::snprintf(
        buffer,
        255,
        "Invalid operation (%s) on type %s",
        op_name,
        type_str
    );
    return Exception(buffer);
}

Exception Exception::InvalidBitwiseArgument()
{
    return Exception("Invalid argument to bitwise operation");
}

Exception Exception::InvalidArgsException(int expected, int received, bool variadic)
{
    char buffer[256];
    if (variadic) {
        std::sprintf(buffer, "Invalid arguments: expected at least %d, received %d", expected, received);
    } else {
        std::sprintf(buffer, "Invalid arguments: expected %d, received %d", expected, received);
    }
    return Exception(buffer);
}

Exception Exception::InvalidArgsException(const char *expected_str, int received)
{
    char buffer[256];
    std::sprintf(buffer, "Invalid arguments: expected %s, received %d", expected_str, received);
    return Exception(buffer);
}

Exception Exception::InvalidArgsException(const char *expected_str)
{
    char buffer[256];
    std::sprintf(buffer, "Invalid arguments: expected %s", expected_str);
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

Exception Exception::MemberNotFoundException(UInt32 hash_code)
{
    char buffer[256];
    std::snprintf(buffer, 256, "Member with hash code %u not found", hash_code);

    return Exception(buffer);
}

Exception Exception::FileOpenException(const char *file_name)
{
    char buffer[256];
    std::sprintf(buffer, "Failed to open file `%s`", file_name);

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

Exception Exception::LibraryLoadException(const char *lib_name)
{
    char buffer[256];
    std::snprintf(buffer, 256, "Failed to open library `%s`", lib_name);

    return Exception(buffer);
}

Exception Exception::LibraryFunctionLoadException(const char *func_name)
{
    char buffer[256];
    std::snprintf(buffer, 256, "Failed to open library function `%s`", func_name);

    return Exception(buffer);
}

Exception Exception::DuplicateExportException()
{
    char buffer[256];
    std::snprintf(buffer, 256, "Duplicate exported symbol");

    return Exception(buffer);
}

Exception Exception::KeyNotFoundException(const char *key)
{
    char buffer[256];
    std::snprintf(buffer, 256, "Key `%s` not found", key);

    return Exception(buffer);
}

} // namespace vm
} // namespace hyperion
