#ifndef ERROR_LIST_HPP
#define ERROR_LIST_HPP

#include <core/lib/FlatSet.hpp>

#include <script/compiler/CompilerError.hpp>
#include <util/UTF8.hpp>
#include <Types.hpp>

#include <algorithm>
#include <ostream>

namespace hyperion::compiler {

class ErrorList
{
public:
    ErrorList();
    ErrorList(const ErrorList &other);
    ErrorList &operator=(const ErrorList &other);
    ErrorList(ErrorList &&other) noexcept;
    ErrorList &operator=(ErrorList &&other) noexcept;
    ~ErrorList() = default;

    SizeType Size() const
        { return m_errors.Size(); }

    CompilerError &operator[](SizeType index)
        { return m_errors[index]; }

    const CompilerError &operator[](SizeType index) const
        { return m_errors[index]; }

    void AddError(const CompilerError &error)
    {
        if (ErrorsSuppressed()) {
            return;
        }

        m_errors.Insert(error);
    }

    void ClearErrors()
        { m_errors.Clear(); }

    void Concatenate(const ErrorList &other)
        { m_errors.Merge(other.m_errors); }

    bool ErrorsSuppressed() const
        { return m_error_suppression_depth > 0; }

    void SuppressErrors(bool suppress)
    {
        if (suppress) {
            m_error_suppression_depth++;
        } else {
            AssertThrow(m_error_suppression_depth > 0);
            m_error_suppression_depth--;
        }
    }

    bool HasFatalErrors() const;
    std::ostream &WriteOutput(std::ostream &os) const;

private:
    FlatSet<CompilerError>  m_errors;
    UInt                    m_error_suppression_depth;
};

} // namespace hyperion::compiler

#endif
