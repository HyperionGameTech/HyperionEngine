/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BUILDTOOL_COMPILATION_UNIT_HPP
#define HYPERION_BUILDTOOL_COMPILATION_UNIT_HPP

#include <parser/ErrorList.hpp>

#include <core/containers/HashMap.hpp>

namespace hyperion::buildtool {

class CompilationUnit
{
public:
    CompilationUnit();
    CompilationUnit(const CompilationUnit& other) = delete;
    ~CompilationUnit();

    HYP_FORCE_INLINE ErrorList& GetErrorList()
    {
        return m_error_list;
    }

    HYP_FORCE_INLINE const ErrorList& GetErrorList() const
    {
        return m_error_list;
    }

    HYP_FORCE_INLINE const HashMap<String, String>& GetPreprocessorDefinitions() const
    {
        return m_preprocessor_definitions;
    }

    HYP_FORCE_INLINE void SetPreprocessorDefinitions(const HashMap<String, String>& preprocessor_definitions)
    {
        m_preprocessor_definitions = preprocessor_definitions;
    }

private:
    ErrorList m_error_list;
    HashMap<String, String> m_preprocessor_definitions;
};

} // namespace hyperion::buildtool

#endif
