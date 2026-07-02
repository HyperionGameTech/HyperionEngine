/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#ifndef HYPERION_CODEGEN_COMPILATION_UNIT_HPP
#define HYPERION_CODEGEN_COMPILATION_UNIT_HPP

#include <parser/ErrorList.hpp>

#include <Core/Containers/Map.hpp>

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

    HYP_FORCE_INLINE const Map<String, String>& GetPreprocessorDefinitions() const
    {
        return m_preprocessorDefinitions;
    }

    HYP_FORCE_INLINE void SetPreprocessorDefinitions(const Map<String, String>& preprocessorDefinitions)
    {
        m_preprocessorDefinitions = preprocessorDefinitions;
    }

private:
    ErrorList m_errorList;
    Map<String, String> m_preprocessorDefinitions;
};

} // namespace Hyperion::CodeGen

#endif
