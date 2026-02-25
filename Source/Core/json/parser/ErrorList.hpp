/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/containers/FlatSet.hpp>

#include <Core/json/parser/CompilerError.hpp>
#include <Core/Unicode.hpp>

#include <Core/Types.hpp>

namespace Hyperion::JSON {

class ErrorList
{
public:
    ErrorList();
    ErrorList(const ErrorList& other);
    ErrorList& operator=(const ErrorList& other);
    ErrorList(ErrorList&& other) noexcept;
    ErrorList& operator=(ErrorList&& other) noexcept;
    ~ErrorList() = default;

    SizeType Size() const
    {
        return m_errors.Size();
    }

    CompilerError& operator[](SizeType index)
    {
        return m_errors[index];
    }

    const CompilerError& operator[](SizeType index) const
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
            if (m_errorSuppressionDepth <= 0)
            {
                return;
            }

            m_errorSuppressionDepth--;
        }
    }

    bool HasFatalErrors() const;

private:
    FlatSet<CompilerError> m_errors;
    uint32 m_errorSuppressionDepth;
};

} // namespace Hyperion::JSON
