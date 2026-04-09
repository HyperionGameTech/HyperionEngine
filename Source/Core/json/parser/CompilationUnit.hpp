/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

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
