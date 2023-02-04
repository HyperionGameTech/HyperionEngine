#ifndef ERROR_LIST_HPP
#define ERROR_LIST_HPP

#include <core/lib/DynArray.hpp>

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

    SizeType Size() const
        { return m_errors.Size(); }

    CompilerError &operator[](SizeType index)
        { return m_errors[index]; }

    const CompilerError &operator[](SizeType index) const
        { return m_errors[index]; }

    void AddError(const CompilerError &error)
        { m_errors.PushBack(error); }

    void ClearErrors()
        { m_errors.Clear(); }

    void SortErrors()
        { std::sort(m_errors.Begin(), m_errors.End()); }

    void Concatenate(const ErrorList &other)
        { m_errors.Concat(other.m_errors); }

    bool HasFatalErrors() const;
    std::ostream &WriteOutput(std::ostream &os) const;

private:
    Array<CompilerError> m_errors;
};

} // namespace hyperion::compiler

#endif
