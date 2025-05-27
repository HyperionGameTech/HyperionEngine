/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <parser/ErrorList.hpp>

#include <core/containers/FlatSet.hpp>

#include <core/utilities/StringUtil.hpp>

#include <fstream>

namespace hyperion::buildtool {

ErrorList::ErrorList()
    : m_error_suppression_depth(0)
{
}

ErrorList::ErrorList(const ErrorList &other)
    : m_errors(other.m_errors),
      m_error_suppression_depth(other.m_error_suppression_depth)
{
}

ErrorList &ErrorList::operator=(const ErrorList &other)
{
    m_errors = other.m_errors;
    m_error_suppression_depth = other.m_error_suppression_depth;

    return *this;
}

ErrorList::ErrorList(ErrorList &&other) noexcept
    : m_errors(std::move(other.m_errors)),
      m_error_suppression_depth(other.m_error_suppression_depth)
{
    other.m_error_suppression_depth = 0;
}

ErrorList &ErrorList::operator=(ErrorList &&other) noexcept
{
    m_errors = std::move(other.m_errors);
    m_error_suppression_depth = other.m_error_suppression_depth;

    other.m_error_suppression_depth = 0;

    return *this;
}

bool ErrorList::HasFatalErrors() const
{
    return m_errors.FindIf([](const CompilerError &error)
    {
        return error.GetLevel() == LEVEL_ERROR;
    }) != m_errors.End();
}

} // namespace hyperion::buildtool
