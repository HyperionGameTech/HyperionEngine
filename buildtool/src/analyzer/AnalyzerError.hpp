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
          m_errorCode(0)
    {
    }

    template <auto MessageString>
    AnalyzerError(const StaticMessage& currentFunction, ValueWrapper<MessageString>, const FilePath& path)
        : AnalyzerError(currentFunction, ValueWrapper<MessageString>(), path, 0)
    {
    }

    template <auto MessageString, class... Args>
    AnalyzerError(const StaticMessage& currentFunction, ValueWrapper<MessageString>, const FilePath& path, int errorCode, Args&&... args)
        : Error(currentFunction, ValueWrapper<HYP_STATIC_STRING("[{}] {}: ").template Concat<MessageString>()>(), errorCode, path, std::forward<Args>(args)...),
          m_path(path),
          m_errorCode(errorCode)
    {
    }

    AnalyzerError(const Error& error, const FilePath& path, int errorCode = 0)
        : Error(error),
          m_path(path),
          m_errorCode(errorCode)
    {
    }

    ~AnalyzerError() = default;

    HYP_FORCE_INLINE const FilePath& GetPath() const
    {
        return m_path;
    }

    HYP_FORCE_INLINE int GetErrorCode() const
    {
        return m_errorCode;
    }

private:
    FilePath m_path;
    int m_errorCode;
};

} // namespace buildtool
} // namespace hyperion

#endif
