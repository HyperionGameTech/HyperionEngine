/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYP_JSON_COMPILATION_UNIT_HPP
#define HYP_JSON_COMPILATION_UNIT_HPP

#include <core/lib/String.hpp>
#include <core/lib/HashMap.hpp>
#include <core/lib/RefCountedPtr.hpp>
#include <core/lib/UniquePtr.hpp>

#include <util/json/parser/ErrorList.hpp>

namespace hyperion::json {

class CompilationUnit
{
public:
    CompilationUnit();
    CompilationUnit(const CompilationUnit &other) = delete;
    ~CompilationUnit();

    ErrorList &GetErrorList() { return m_error_list; }
    const ErrorList &GetErrorList() const { return m_error_list; }

private:
    ErrorList               m_error_list;
};

} // namespace hyperion::json

#endif
