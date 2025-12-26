/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/json/parser/ErrorList.hpp>

namespace Hyperion::json {

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

} // namespace Hyperion::json
