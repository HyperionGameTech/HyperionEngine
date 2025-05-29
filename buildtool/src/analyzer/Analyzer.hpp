/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BUILDTOOL_ANALYZER_HPP
#define HYPERION_BUILDTOOL_ANALYZER_HPP

#include <analyzer/Module.hpp>
#include <analyzer/AnalyzerError.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/containers/HashSet.hpp>
#include <core/containers/HashMap.hpp>

#include <core/utilities/Result.hpp>

#include <core/Defines.hpp>

namespace hyperion {
namespace buildtool {

struct AnalyzerState
{
    Array<AnalyzerError> errors;

    HYP_FORCE_INLINE bool HasErrors() const
    {
        return !errors.Empty();
    }
};

class Analyzer
{
public:
    Analyzer() = default;
    ~Analyzer() = default;

    HYP_FORCE_INLINE const FilePath& GetWorkingDirectory() const
    {
        return m_working_directory;
    }

    HYP_FORCE_INLINE void SetWorkingDirectory(const FilePath& working_directory)
    {
        m_working_directory = working_directory;
    }

    HYP_FORCE_INLINE const FilePath& GetSourceDirectory() const
    {
        return m_source_directory;
    }

    HYP_FORCE_INLINE void SetSourceDirectory(const FilePath& source_directory)
    {
        m_source_directory = source_directory;
    }

    HYP_FORCE_INLINE const FilePath& GetCXXOutputDirectory() const
    {
        return m_cxx_output_directory;
    }

    HYP_FORCE_INLINE void SetCXXOutputDirectory(const FilePath& cxx_output_directory)
    {
        m_cxx_output_directory = cxx_output_directory;
    }

    HYP_FORCE_INLINE const FilePath& GetCSharpOutputDirectory() const
    {
        return m_csharp_output_directory;
    }

    HYP_FORCE_INLINE void SetCSharpOutputDirectory(const FilePath& csharp_output_directory)
    {
        m_csharp_output_directory = csharp_output_directory;
    }

    HYP_FORCE_INLINE const HashSet<FilePath>& GetExcludeDirectories() const
    {
        return m_exclude_directories;
    }

    HYP_FORCE_INLINE void SetExcludeDirectories(const HashSet<FilePath>& exclude_directories)
    {
        m_exclude_directories = exclude_directories;
    }

    HYP_FORCE_INLINE const HashSet<FilePath>& GetExcludeFiles() const
    {
        return m_exclude_files;
    }

    HYP_FORCE_INLINE void SetExcludeFiles(const HashSet<FilePath>& exclude_files)
    {
        m_exclude_files = exclude_files;
    }

    HYP_FORCE_INLINE const AnalyzerState& GetState() const
    {
        return m_state;
    }

    HYP_FORCE_INLINE const Array<UniquePtr<Module>>& GetModules() const
    {
        return m_modules;
    }

    HYP_FORCE_INLINE const HashMap<String, String>& GetGlobalDefines() const
    {
        return m_global_defines;
    }

    HYP_FORCE_INLINE void SetGlobalDefines(HashMap<String, String>&& global_defines)
    {
        m_global_defines = std::move(global_defines);
    }

    HYP_FORCE_INLINE const HashSet<String>& GetIncludePaths() const
    {
        return m_include_paths;
    }

    HYP_FORCE_INLINE void SetIncludePaths(HashSet<String>&& include_paths)
    {
        m_include_paths = std::move(include_paths);
    }

    HYP_FORCE_INLINE void AddError(const AnalyzerError& error)
    {
        Mutex::Guard guard(m_mutex);

        m_state.errors.PushBack(error);
    }

    const HypClassDefinition* FindHypClassDefinition(UTF8StringView class_name) const;

    Module* AddModule(const FilePath& path);

    TResult<void, AnalyzerError> ProcessModule(Module& mod);

private:
    FilePath m_working_directory;
    FilePath m_source_directory;
    FilePath m_cxx_output_directory;
    FilePath m_csharp_output_directory;

    HashSet<FilePath> m_exclude_directories;
    HashSet<FilePath> m_exclude_files;

    AnalyzerState m_state;
    Array<UniquePtr<Module>> m_modules;
    mutable Mutex m_mutex;
    HashMap<String, String> m_global_defines;
    HashSet<String> m_include_paths;
};

} // namespace buildtool
} // namespace hyperion

#endif
