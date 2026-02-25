/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/json/parser/ErrorList.hpp>

namespace Hyperion::JSON {

class CompilationUnit
{
public:
    CompilationUnit();
    CompilationUnit(const CompilationUnit& other) = delete;
    ~CompilationUnit();

    ErrorList& GetErrorList()
    {
        return m_errorList;
    }

    const ErrorList& GetErrorList() const
    {
        return m_errorList;
    }

private:
    ErrorList m_errorList;
};

} // namespace Hyperion::JSON
