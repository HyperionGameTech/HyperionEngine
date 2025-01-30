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
        : Error(),
          m_error_code(0)
    {
    }

    AnalyzerError(const StaticMessage &static_message, const FilePath &path, int error_code = 0, const String &error_message = String::empty)
        : Error(static_message),
          m_path(path),
          m_error_code(error_code),
          m_error_message(error_message)
    {
    }

    AnalyzerError(const Error &error, const FilePath &path, int error_code = 0, const String &error_message = String::empty)
        : Error(error),
          m_path(path),
          m_error_code(error_code),
          m_error_message(error_message)
    {
    }

    virtual ~AnalyzerError() override = default;

    virtual operator bool() const override
    {
        return true;
    }

    HYP_FORCE_INLINE const FilePath &GetPath() const
        { return m_path; }

    HYP_FORCE_INLINE int GetErrorCode() const
        { return m_error_code; }

    HYP_FORCE_INLINE const String &GetErrorMessage() const
        { return m_error_message; }

private:
    FilePath    m_path;
    int         m_error_code;
    String      m_error_message;
};

} // namespace buildtool
} // namespace hyperion

#endif
