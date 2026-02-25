/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#ifndef HYPERION_CODEGEN_ANALYZER_ERROR_HPP
#define HYPERION_CODEGEN_ANALYZER_ERROR_HPP

#include <Core/utilities/Result.hpp>

#include <Core/filesystem/FilePath.hpp>

#include <Core/Defines.hpp>

namespace Hyperion {
namespace CodeGen {

class AnalyzerError : public Error
{
public:
    AnalyzerError()
        : Error(),
          m_errorCode(0)
    {
    }

    template <auto CurrentFunctionString, auto MessageString>
    AnalyzerError(ValueWrapper<CurrentFunctionString> currentFunction, ValueWrapper<MessageString> messageString, const FilePath& path)
        : AnalyzerError(currentFunction, messageString, path, 0)
    {
    }

    template <auto CurrentFunctionString, auto MessageString, class... Args>
    AnalyzerError(ValueWrapper<CurrentFunctionString> currentFunction, ValueWrapper<MessageString>, const FilePath& path, int errorCode, Args&&... args)
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

} // namespace CodeGen
} // namespace Hyperion

#endif
