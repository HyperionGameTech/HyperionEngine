using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.Text;
using Microsoft.CodeAnalysis.Emit;
using System.Diagnostics;

namespace Hyperion
{
    public class CSharpScriptCompiler : ScriptCompilerBase
    {
        private Dictionary<string, string> resolvedModuleNames = [];

        public CSharpScriptCompiler(string sourceDirectory, string intermediateDirectory, string binaryOutputDirectory)
            : base(sourceDirectory, intermediateDirectory, binaryOutputDirectory)
        {
        }

        public override void BuildAllProjects()
        {
            Logger.Log(logChannel, LogLevel.Info, "Building all projects in source directory: {0}", sourceDirectory);

            string[] symLinks = System.IO.Directory.GetFiles(binaryOutputDirectory, "*.*.dll", System.IO.SearchOption.AllDirectories)
                .Where(file => (System.IO.File.GetAttributes(file) & System.IO.FileAttributes.ReparsePoint) == System.IO.FileAttributes.ReparsePoint)
                .ToArray();

            foreach (string symLink in symLinks)
            {
                try
                {
                    System.IO.File.Delete(symLink);
                }
                catch (Exception e)
                {
                    Logger.Log(logChannel, LogLevel.Error, "Failed to delete symlink: {0}", e.Message);
                }
            }

            DeleteOldHotReloadVersions();

            string[] directories = System.IO.Directory.GetDirectories(sourceDirectory, "*", System.IO.SearchOption.AllDirectories)
                .Append(sourceDirectory)
                .Where(directory => System.IO.Directory.GetFiles(directory, "*.cs").Length > 0)
                .ToArray();

            foreach (string directory in directories)
            {
                Logger.Log(logChannel, LogLevel.Info, "Processing module directory: {0}", directory);

                string moduleName;
                int hotReloadVersion;

                BuildProject(
                    scriptDirectory: directory,
                    forceRebuild: false,
                    moduleName: out moduleName,
                    hotReloadVersion: out hotReloadVersion
                );
            }
        }

        private bool DetectNeedsRebuild(string scriptDirectory, string moduleName)
        {
            // Get the max updated timestamp of all the files in the directory

            string[] files = System.IO.Directory.GetFiles(scriptDirectory, "*.cs", System.IO.SearchOption.AllDirectories);

            long maxTimestamp = 0;

            foreach (string file in files)
            {
                long timestamp = System.IO.File.GetLastWriteTime(file).ToFileTime();

                if (timestamp > maxTimestamp)
                {
                    maxTimestamp = timestamp;
                }
            }

            if (!System.IO.Directory.Exists(binaryOutputDirectory))
            {
                // Directory does not exist, needs rebuild
                return true;
            }

            Logger.Log(logChannel, LogLevel.Info, "binaryOutputDirectory: {0}", binaryOutputDirectory);

            string[] dlls = System.IO.Directory.GetFiles(binaryOutputDirectory, $"{moduleName}.dll", System.IO.SearchOption.AllDirectories);

            if (dlls.Length == 0)
            {
                // DLL not found, needs rebuild
                return true;
            }

            foreach (string dll in dlls)
            {
                Logger.Log(logChannel, LogLevel.Info, "Checking DLL: {0}", dll);
                if (System.IO.File.GetLastWriteTime(dll).ToFileTime() < maxTimestamp)
                {
                    return true;
                }
            }

            return false;
        }

        private string GetModuleNameForScriptDirectory(string scriptDirectory)
        {
            // Get relative path to the source directory
            string relativePath = System.IO.Path.GetRelativePath(sourceDirectory, scriptDirectory)
                .Replace(" ", "")
                .Replace(".", "");

            Logger.Log(logChannel, LogLevel.Info, $"Relative path: {relativePath}");

            while (relativePath.EndsWith("/") || relativePath.EndsWith("\\"))
            {
                relativePath = relativePath.Substring(0, relativePath.Length - 1);
            }

            // chop the last part of the relative path
            string moduleName = "Default";

            if (relativePath.Length != 0)
            {
                moduleName = relativePath.Replace("/", ".").Replace("\\", ".");
            }

            while (moduleName.StartsWith("."))
            {
                moduleName = moduleName.Substring(1);
            }

            if (moduleName.Length == 0)
            {
                throw new Exception("Invalid module name");
            }

            return moduleName;
        }

        private string GetProjectOutputDirectory(string moduleName, bool createDirectory = false)
        {
            string path = System.IO.Path.Combine(intermediateDirectory, moduleName);

            if (createDirectory && !System.IO.Directory.Exists(path))
            {
                System.IO.Directory.CreateDirectory(path);
            }

            return path;
        }

        private string GetProjectFilePath(string moduleName, string projectOutputDirectory)
        {
            string moduleBaseName = moduleName.Split(".").Last();

            return System.IO.Path.Combine(projectOutputDirectory, $"{moduleBaseName}.csproj");
        }

        private string GetAssemblyPath(string moduleName, bool relative = false)
        {
            if (relative)
            {
                return $"{moduleName}.dll";
            }
            else
            {
                return System.IO.Path.Combine(binaryOutputDirectory, $"{moduleName}.dll");
            }
        }

        private bool BuildProject(string scriptDirectory, bool forceRebuild, out string moduleName, out int hotReloadVersion)
        {
            Logger.Log(logChannel, LogLevel.Info, "Building project in directory {0}", scriptDirectory);

            moduleName = GetModuleNameForScriptDirectory(scriptDirectory);
            hotReloadVersion = -1;

            try
            {
                if (!forceRebuild && !DetectNeedsRebuild(scriptDirectory: scriptDirectory, moduleName: moduleName))
                {
                    Logger.Log(logChannel, LogLevel.Info, "Skipping rebuild of module {0}, no changes detected", moduleName);

                    return true;
                }
            }
            catch (Exception e)
            {
                Logger.Log(logChannel, LogLevel.Error, "Failed to detect if module {0} needs rebuild: {1}", moduleName, e.Message);

                return false;
            }

            Logger.Log(logChannel, LogLevel.Info, "Rebuilding module {0}...", moduleName);

            string projectOutputDirectory = GetProjectOutputDirectory(
                moduleName: moduleName,
                createDirectory: true
            );

            string projectFilePath = GetProjectFilePath(
                moduleName: moduleName,
                projectOutputDirectory: projectOutputDirectory
            );

            Logger.Log(logChannel, LogLevel.Info, "Generating .csproj at {0}", projectFilePath);

            if (!GenerateCSharpProjectFile(
                projectFilePath: projectFilePath,
                scriptDirectory: scriptDirectory,
                moduleName: moduleName
            ))
            {
                return false;
            }

            return RunDotNetCLI(
                projectOutputDirectory: projectOutputDirectory,
                hotReloadVersion: out hotReloadVersion
            );
        }

        private static string EscapePath(string path)
        {
            // prevent escape codes from being interpreted by the XML parser
            // (e.g \x64 is interpreted as a unicode character)
            return path.Replace("\\x", "\\\\x");
        }

        private bool GenerateCSharpProjectFile(string projectFilePath, string scriptDirectory, string moduleName)
        {
            string? dependenciesDirectory = System.IO.Path.GetDirectoryName(System.Environment.ProcessPath);
            if (dependenciesDirectory == null)
            {
                Logger.Log(logChannel, LogLevel.Error, "Failed to get dependencies directory!");

                return false;
            }

            List<string> csFiles = System.IO.Directory.GetFiles(scriptDirectory, "*.cs")
                .ToList();

            // iterate subdirectories recursively
            foreach (string subDirectory in System.IO.Directory.GetDirectories(scriptDirectory, "*", System.IO.SearchOption.AllDirectories))
            {
                csFiles.AddRange(System.IO.Directory.GetFiles(subDirectory, "*.cs"));
            }

            string projectContent =
                "<Project Sdk=\"Microsoft.NET.Sdk\">\n"
                    + "<PropertyGroup>\n"
                        + "<OutputType>Library</OutputType>\n"
                        + "<TargetFramework>net10.0</TargetFramework>\n"
                        + "<ImplicitUsings>enable</ImplicitUsings>\n"
                        + "<Nullable>enable</Nullable>\n"
                        + "<AllowUnsafeBlocks>true</AllowUnsafeBlocks>\n"
                        + "<EnableDynamicLoading>true</EnableDynamicLoading>\n"
                        + $"<AssemblyName>{moduleName}</AssemblyName>\n"
                    + "</PropertyGroup>\n"
                    + "<ItemGroup>\n"
                    + $"<Reference Include=\"Hyperion.NET.Shared\">\n"
                        + $"<HintPath>{EscapePath(System.IO.Path.Combine(dependenciesDirectory, "Hyperion.NET.Shared.dll"))}</HintPath>\n"
                        + "<Private>false</Private>\n"
                    + "</Reference>\n"
                    + $"<Reference Include=\"Hyperion.NET.Runtime\">\n"
                        + $"<HintPath>{EscapePath(System.IO.Path.Combine(dependenciesDirectory, "Hyperion.NET.Runtime.dll"))}</HintPath>\n"
                        + "<Private>false</Private>\n"
                    + "</Reference>\n"
                    + string.Join("", csFiles.Select(script => $"<Compile Include=\"{EscapePath(script)}\" />\n"))
                    + "</ItemGroup>\n"
                + "</Project>\n";

            try
            {
                System.IO.File.WriteAllText(projectFilePath, projectContent);
            }
            catch (Exception e)
            {
                Logger.Log(logChannel, LogLevel.Error, "Failed to write project file: {0}", e.Message);

                return false;
            }

            return true;
        }

        private bool RunDotNetCLI(string projectOutputDirectory, out int hotReloadVersion)
        {
            Logger.Log(logChannel, LogLevel.Info, "Running dotnet CLI in {0}", projectOutputDirectory);

            hotReloadVersion = -1;

            // ensure the working dir exists
            if (!System.IO.Directory.Exists(projectOutputDirectory))
            {
                // make the directory (recursive)
                try
                {
                    System.IO.Directory.CreateDirectory(projectOutputDirectory);
                }
                catch (Exception e)
                {
                    Logger.Log(logChannel, LogLevel.Error, "Failed to create project output directory {0}: {1}", projectOutputDirectory, e.Message);
                    return false;
                }
            }

            System.Diagnostics.Process process = new System.Diagnostics.Process();
#if HYP_MACOS
            process.StartInfo.FileName = "/usr/local/share/dotnet/dotnet";
#else
            process.StartInfo.FileName = "dotnet";
#endif
            process.StartInfo.Arguments = $"build";
            process.StartInfo.WorkingDirectory = projectOutputDirectory;
            process.StartInfo.RedirectStandardOutput = true;
            process.StartInfo.RedirectStandardError = true;
            process.StartInfo.UseShellExecute = false;
            process.StartInfo.CreateNoWindow = true;

            // prevents a stale MSBuildSdksPath (left behind by an uninstalled SDK) from breaking resolution of Microsoft.NET.Sdk
            process.StartInfo.Environment.Remove("MSBuildSdksPath");

#if !HYP_MACOS
            process.OutputDataReceived += (object sendingProcess, System.Diagnostics.DataReceivedEventArgs eventArgs) =>
            {
                Logger.Log(logChannel, LogLevel.Info, "{0}", eventArgs.Data);
            };
            process.ErrorDataReceived += (object sendingProcess, System.Diagnostics.DataReceivedEventArgs eventArgs) =>
            {
                Logger.Log(logChannel, LogLevel.Error, "{0}", eventArgs.Data);
            };
#endif

            try
            {
                process.Start();
            }
            catch (Exception e)
            {
                Logger.Log(logChannel, LogLevel.Error, "Failed to start dotnet process: {0}", e.Message);

                return false;
            }

#if !HYP_MACOS
            process.BeginOutputReadLine();
            process.BeginErrorReadLine();
#else
            // due to a bug isolated to macOS causing deadlock / hang on process exit,
            // we need to handle output synchronously.
            string standardOutput = process.StandardOutput.ReadToEnd();
            string standardErrorOutput = process.StandardError.ReadToEnd();

            if (!string.IsNullOrEmpty(standardOutput))
                Logger.Log(logChannel, LogLevel.Info, "{0}", standardOutput);

            if (!string.IsNullOrEmpty(standardErrorOutput))
                Logger.Log(logChannel, LogLevel.Error, "{0}", standardErrorOutput);
#endif

            Logger.Log(logChannel, LogLevel.Info, "Waiting for dotnet process to finish...");

            if (!process.WaitForExit(timeoutMilliseconds))
            {
                Logger.Log(logChannel, LogLevel.Error, "dotnet process timed out after {0} milliseconds", timeoutMilliseconds);

                // kill the process
                process.Kill();

                return false;
            }

            Logger.Log(logChannel, LogLevel.Info, "dotnet process finished with exit code {0}", process.ExitCode);

            if (process.ExitCode != 0)
            {
                Logger.Log(logChannel, LogLevel.Error, "Failed to compile script. Check the output log for more information.");

                MessageBox.Critical()
                    .Title("Script Compilation Error")
                    .Text("Failed to compile script. Check the output log for more information.")
                    .Button("OK", () => { Logger.Log(logChannel, LogLevel.Info, "OK clicked"); })
                    .Show();

                Debugger.Break();

                return false;
            }

            Logger.Log(logChannel, LogLevel.Info, "Script compiled successfully");

            // Grep all DLLs in the output directory
            string[] dlls = System.IO.Directory.GetFiles(System.IO.Path.Combine(projectOutputDirectory, "bin"), "*.dll", System.IO.SearchOption.AllDirectories);

            return CopyOutputAssemblies(dlls, out hotReloadVersion);
        }

        // A loaded assembly can't be overwritten on Windows, not even when it was loaded through a symlink, and loaded
        // script assemblies stay loaded for the whole session. So each build gets its own real Module.N.dll which hot reload
        // loads, and the unversioned Module.dll is only refreshed when nothing has it loaded.
        private bool CopyOutputAssemblies(string[] dlls, out int hotReloadVersion)
        {
            hotReloadVersion = 1;

            foreach (int version in GetHotReloadVersions().Values.SelectMany(versions => versions))
            {
                hotReloadVersion = Math.Max(hotReloadVersion, version + 1);
            }

            foreach (string dll in dlls)
            {
                string moduleFileName = System.IO.Path.GetFileNameWithoutExtension(dll);
                string versionedDllPath = System.IO.Path.Combine(binaryOutputDirectory, $"{moduleFileName}.{hotReloadVersion}.dll");

                Logger.Log(logChannel, LogLevel.Info, "Copying output script assembly {0} to {1}", dll, versionedDllPath);

                try
                {
                    System.IO.File.Copy(dll, versionedDllPath, true);
                }
                catch (Exception e)
                {
                    Logger.Log(logChannel, LogLevel.Error, "Failed to copy script assembly: {0}", e.Message);

                    return false;
                }

                string dllPath = System.IO.Path.Combine(binaryOutputDirectory, $"{moduleFileName}.dll");

                try
                {
                    System.IO.File.Copy(dll, dllPath, true);
                }
                catch (IOException)
                {
                    Logger.Log(logChannel, LogLevel.Info, "{0} is in use, hot reload will load {1} instead", dllPath, versionedDllPath);
                }
            }

            return true;
        }

        // Module.N.dll copies in the output directory, keyed by the Module.dll they are a version of
        private Dictionary<string, List<int>> GetHotReloadVersions()
        {
            Dictionary<string, List<int>> versionsByDllPath = new(StringComparer.OrdinalIgnoreCase);

            foreach (string file in System.IO.Directory.GetFiles(binaryOutputDirectory, "*.*.dll"))
            {
                string fileNameWithoutExtension = System.IO.Path.GetFileNameWithoutExtension(file);
                int lastDotIndex = fileNameWithoutExtension.LastIndexOf('.');

                if (lastDotIndex <= 0 || !int.TryParse(fileNameWithoutExtension.Substring(lastDotIndex + 1), out int version))
                {
                    continue;
                }

                string dllPath = System.IO.Path.Combine(binaryOutputDirectory, fileNameWithoutExtension.Substring(0, lastDotIndex) + ".dll");

                if (!System.IO.File.Exists(dllPath))
                {
                    continue;
                }

                if (!versionsByDllPath.TryGetValue(dllPath, out List<int>? versions))
                {
                    versions = [];
                    versionsByDllPath.Add(dllPath, versions);
                }

                versions.Add(version);
            }

            return versionsByDllPath;
        }

        private void DeleteOldHotReloadVersions()
        {
            foreach (KeyValuePair<string, List<int>> entry in GetHotReloadVersions())
            {
                int latestVersion = entry.Value.Max();
                string dllPathWithoutExtension = entry.Key.Substring(0, entry.Key.Length - ".dll".Length);

                foreach (int version in entry.Value)
                {
                    if (version == latestVersion)
                    {
                        continue;
                    }

                    try
                    {
                        System.IO.File.Delete($"{dllPathWithoutExtension}.{version}.dll");
                    }
                    catch (Exception)
                    {
                        // still loaded by another world, it gets cleaned up next session
                    }
                }
            }
        }

        // The newest Module.N.dll is always the freshest build, the unversioned Module.dll may be stale if it was in use when copying
        private void UseLatestHotReloadVersion(string moduleName, ref ScriptDesc scriptDesc)
        {
            string dllPath = System.IO.Path.Combine(binaryOutputDirectory, $"{moduleName}.dll");

            scriptDesc.HotReloadVersion = GetHotReloadVersions().TryGetValue(dllPath, out List<int>? versions)
                ? versions.Max()
                : 0;
        }

        public override bool Compile(ref ScriptDesc scriptDesc)
        {
            return BuildProjectForScript(scriptDesc.Path, forceRebuild: true, ref scriptDesc);
        }

        public bool ResolveAssembly(string scriptPath, ref ScriptDesc scriptDesc)
        {
            string? scriptDirectory = System.IO.Path.GetDirectoryName(scriptPath);

            if (scriptDirectory != null && resolvedModuleNames.TryGetValue(scriptDirectory, out string? moduleName))
            {
                scriptDesc.AssemblyPath = GetAssemblyPath(moduleName, relative: true);
                UseLatestHotReloadVersion(moduleName, ref scriptDesc);

                return true;
            }

            return BuildProjectForScript(scriptPath, forceRebuild: false, ref scriptDesc);
        }

        private bool BuildProjectForScript(string scriptPath, bool forceRebuild, ref ScriptDesc scriptDesc)
        {
            string moduleName;
            int hotReloadVersion;

            string? scriptDirectory = System.IO.Path.GetDirectoryName(scriptPath);

            if (scriptDirectory == null)
            {
                Logger.Log(logChannel, LogLevel.Error, "Failed to get script directory for script {0}", scriptPath);

                return false;
            }

            if (!BuildProject(
                scriptDirectory: scriptDirectory,
                forceRebuild: forceRebuild,
                moduleName: out moduleName,
                hotReloadVersion: out hotReloadVersion
            ))
            {
                return false;
            }

            resolvedModuleNames[scriptDirectory] = moduleName;

            scriptDesc.AssemblyPath = GetAssemblyPath(moduleName, relative: true);

            // -1 means the existing build was up to date
            if (hotReloadVersion >= 0)
            {
                scriptDesc.HotReloadVersion = hotReloadVersion;
            }
            else
            {
                UseLatestHotReloadVersion(moduleName, ref scriptDesc);
            }

            return true;
        }
    }
}
