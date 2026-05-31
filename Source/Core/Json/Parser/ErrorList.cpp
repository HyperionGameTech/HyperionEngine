/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/JSON/Parser/ErrorList.hpp>

#include <Core/Utilities/StringUtil.hpp>

#include <Core/Containers/FlatSet.hpp>

namespace Hyperion::JSON {

ErrorList::ErrorList()
    : m_errorSuppressionDepth(0)
{
}

ErrorList::ErrorList(const ErrorList& other)
    : m_errors(other.m_errors),
      m_errorSuppressionDepth(other.m_errorSuppressionDepth)
{
}

ErrorList& ErrorList::operator=(const ErrorList& other)
{
    m_errors = other.m_errors;
    m_errorSuppressionDepth = other.m_errorSuppressionDepth;

    return *this;
}

ErrorList::ErrorList(ErrorList&& other) noexcept
    : m_errors(std::move(other.m_errors)),
      m_errorSuppressionDepth(other.m_errorSuppressionDepth)
{
    other.m_errorSuppressionDepth = 0;
}

ErrorList& ErrorList::operator=(ErrorList&& other) noexcept
{
    m_errors = std::move(other.m_errors);
    m_errorSuppressionDepth = other.m_errorSuppressionDepth;

    other.m_errorSuppressionDepth = 0;

    return *this;
}

bool ErrorList::HasFatalErrors() const
{
    return m_errors.FindIf([](const CompilerError& error)
               {
                   return error.GetLevel() == LEVEL_ERROR;
               })
        != m_errors.End();
}

} // namespace Hyperion::JSON
