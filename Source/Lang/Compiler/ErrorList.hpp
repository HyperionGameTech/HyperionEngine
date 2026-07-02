#pragma once

#include <Core/Defines.hpp>
#include <Core/Unicode.hpp>
#include <Core/Types.hpp>

#include <Core/Containers/FlatSet.hpp>

#include <Lang/Compiler/CompilerError.hpp>

#include <algorithm>
#include <ostream>

namespace Hyperion {

class SCRIPT_API ErrorList
{
public:
    ErrorList();
    
    ErrorList(const ErrorList& other);
    ErrorList& operator=(const ErrorList& other);
    
    ErrorList(ErrorList&& other) noexcept;
    ErrorList& operator=(ErrorList&& other) noexcept;

    ~ErrorList() = default;

    size_t Size() const
    {
        return m_errors.Size();
    }

    CompilerError& operator[](size_t index)
    {
        return m_errors[index];
    }

    const CompilerError& operator[](size_t index) const
    {
        return m_errors[index];
    }

    void AddError(const CompilerError& error)
    {
        if (ErrorsSuppressed())
        {
            return;
        }

        m_errors.Insert(error);
    }

    void ClearErrors()
    {
        m_errors.Clear();
    }

    void Concatenate(const ErrorList& other)
    {
        m_errors.Merge(other.m_errors);
    }

    bool ErrorsSuppressed() const
    {
        return m_errorSuppressionDepth > 0;
    }

    void SuppressErrors(bool suppress)
    {
        if (suppress)
        {
            m_errorSuppressionDepth++;
        }
        else
        {
            Assert(m_errorSuppressionDepth > 0);
            m_errorSuppressionDepth--;
        }
    }

    bool HasFatalErrors() const;
    std::ostream& WriteOutput(std::ostream& os) const;

private:
    FlatSet<CompilerError> m_errors;
    uint32 m_errorSuppressionDepth;
};

} // namespace Hyperion
