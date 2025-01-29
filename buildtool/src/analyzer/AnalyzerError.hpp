/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BUILDTOOL_ANALYZER_ERROR_HPP
#define HYPERION_BUILDTOOL_ANALYZER_ERROR_HPP

#include <core/utilities/Result.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/Defines.hpp>

namespace hyperion {
namespace buildtool {

class AnalyzerError : public Error
{
public:
    AnalyzerError()
        : Error()
    {
    }

    AnalyzerError(const StaticMessage &static_message)
        : AnalyzerError(static_message, "")
    {
    }

    AnalyzerError(const StaticMessage &static_message, const FilePath &path)
        : Error(static_message),
          m_path(path)
    {
    }

    virtual ~AnalyzerError() override = default;

    virtual operator bool() const override
    {
        return true;
    }

    HYP_FORCE_INLINE const FilePath &GetPath() const
        { return m_path; }

private:
    FilePath    m_path;
};

} // namespace buildtool
} // namespace hyperion

#endif
