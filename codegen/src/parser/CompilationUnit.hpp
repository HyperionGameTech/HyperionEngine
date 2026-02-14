/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CODEGEN_COMPILATION_UNIT_HPP
#define HYPERION_CODEGEN_COMPILATION_UNIT_HPP

#include <parser/ErrorList.hpp>

#include <core/containers/HashMap.hpp>

namespace Hyperion::CodeGen {

class CompilationUnit
{
public:
    CompilationUnit();
    CompilationUnit(const CompilationUnit& other) = delete;
    ~CompilationUnit();

    HYP_FORCE_INLINE ErrorList& GetErrorList()
    {
        return m_errorList;
    }

    HYP_FORCE_INLINE const ErrorList& GetErrorList() const
    {
        return m_errorList;
    }

    HYP_FORCE_INLINE const HashMap<String, String>& GetPreprocessorDefinitions() const
    {
        return m_preprocessorDefinitions;
    }

    HYP_FORCE_INLINE void SetPreprocessorDefinitions(const HashMap<String, String>& preprocessorDefinitions)
    {
        m_preprocessorDefinitions = preprocessorDefinitions;
    }

private:
    ErrorList m_errorList;
    HashMap<String, String> m_preprocessorDefinitions;
};

} // namespace Hyperion::CodeGen

#endif
