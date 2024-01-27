#ifndef EXCEPTION_HPP
#define EXCEPTION_HPP

#include <util/UTF8.hpp>

namespace hyperion {
namespace vm {

class Exception
{
public:
    Exception(const char *str);
    Exception(const Exception &other);
    Exception &operator=(const Exception &other);
    Exception(Exception &&other) noexcept;
    Exception &operator=(Exception &&other) noexcept;
    ~Exception();

    const char *ToString() const { return m_str; }

    static Exception InvalidComparisonException(const char *left_type_str, const char *right_type_str);
    static Exception InvalidOperationException(const char *op_name,
        const char *left_type_str, const char *right_type_str);
    static Exception InvalidOperationException(const char *op_name, const char *type_str);
    static Exception InvalidBitwiseArgument();
    static Exception InvalidArgsException(int expected, int received, bool variadic = false);
    static Exception InvalidArgsException(const char *expected_str, int received);
    static Exception InvalidArgsException(const char *expected_str);
    static Exception InvalidConstructorException();
    static Exception NullReferenceException();
    static Exception DivisionByZeroException();
    static Exception OutOfBoundsException();
    static Exception MemberNotFoundException(UInt32 hash_code);
    static Exception FileOpenException(const char *file_name);
    static Exception UnopenedFileWriteException();
    static Exception UnopenedFileReadException();
    static Exception UnopenedFileCloseException();
    static Exception LibraryLoadException(const char *lib_name);
    static Exception LibraryFunctionLoadException(const char *func_name);
    static Exception DuplicateExportException();
    static Exception KeyNotFoundException(const char *key);

private:
    char *m_str;
};

} // namespace vm
} // namespace hyperion

#endif
