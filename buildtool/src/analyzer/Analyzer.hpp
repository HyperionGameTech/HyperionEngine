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

const String& HypClassDefinitionTypeToString(HypClassDefinitionType type);
const String& HypMemberTypeToString(HypMemberType type);

class Analyzer
{
public:
    Analyzer() = default;
    ~Analyzer() = default;

    HYP_FORCE_INLINE const FilePath& GetWorkingDirectory() const
    {
        return m_workingDirectory;
    }

    HYP_FORCE_INLINE void SetWorkingDirectory(const FilePath& workingDirectory)
    {
        m_workingDirectory = workingDirectory;
    }

    HYP_FORCE_INLINE const FilePath& GetSourceDirectory() const
    {
        return m_sourceDirectory;
    }

    HYP_FORCE_INLINE void SetSourceDirectory(const FilePath& sourceDirectory)
    {
        m_sourceDirectory = sourceDirectory;
    }

    HYP_FORCE_INLINE const FilePath& GetCXXOutputDirectory() const
    {
        return m_cxxOutputDirectory;
    }

    HYP_FORCE_INLINE void SetCXXOutputDirectory(const FilePath& cxxOutputDirectory)
    {
        m_cxxOutputDirectory = cxxOutputDirectory;
    }

    HYP_FORCE_INLINE const FilePath& GetCSharpOutputDirectory() const
    {
        return m_csharpOutputDirectory;
    }

    HYP_FORCE_INLINE void SetCSharpOutputDirectory(const FilePath& csharpOutputDirectory)
    {
        m_csharpOutputDirectory = csharpOutputDirectory;
    }

    HYP_FORCE_INLINE const HashSet<FilePath>& GetExcludeDirectories() const
    {
        return m_excludeDirectories;
    }

    HYP_FORCE_INLINE void SetExcludeDirectories(const HashSet<FilePath>& excludeDirectories)
    {
        m_excludeDirectories = excludeDirectories;
    }

    HYP_FORCE_INLINE const HashSet<FilePath>& GetExcludeFiles() const
    {
        return m_excludeFiles;
    }

    HYP_FORCE_INLINE void SetExcludeFiles(const HashSet<FilePath>& excludeFiles)
    {
        m_excludeFiles = excludeFiles;
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
        return m_globalDefines;
    }

    HYP_FORCE_INLINE void SetGlobalDefines(HashMap<String, String>&& globalDefines)
    {
        m_globalDefines = std::move(globalDefines);
    }

    HYP_FORCE_INLINE const HashSet<String>& GetIncludePaths() const
    {
        return m_includePaths;
    }

    HYP_FORCE_INLINE void SetIncludePaths(HashSet<String>&& includePaths)
    {
        m_includePaths = std::move(includePaths);
    }

    HYP_FORCE_INLINE void AddError(const AnalyzerError& error)
    {
        Mutex::Guard guard(m_mutex);

        m_state.errors.PushBack(error);
    }

    const HypClassDefinition* FindHypClassDefinition(UTF8StringView className) const;

    Module* AddModule(const FilePath& path);

    TResult<void, AnalyzerError> ProcessModule(Module& mod);

    bool HasBaseClass(const HypClassDefinition& hypClassDefinition, UTF8StringView baseClassName) const;

private:
    FilePath m_workingDirectory;
    FilePath m_sourceDirectory;
    FilePath m_cxxOutputDirectory;
    FilePath m_csharpOutputDirectory;

    HashSet<FilePath> m_excludeDirectories;
    HashSet<FilePath> m_excludeFiles;

    AnalyzerState m_state;
    Array<UniquePtr<Module>> m_modules;
    mutable Mutex m_mutex;
    HashMap<String, String> m_globalDefines;
    HashSet<String> m_includePaths;
};

} // namespace buildtool
} // namespace hyperion

#endif
