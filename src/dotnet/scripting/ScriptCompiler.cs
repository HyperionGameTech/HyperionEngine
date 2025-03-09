using System;
using System.IO;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.MSBuild;
using Microsoft.CodeAnalysis.Text;
using Microsoft.CodeAnalysis.Emit;

namespace Hyperion
{
    public class ScriptCompiler
    {
        private static LogChannel logChannel = LogChannel.ByName("ScriptCompiler");

        private string sourceDirectory;
        private string intermediateDirectory;
        private string binaryOutputDirectory;

        public ScriptCompiler(string sourceDirectory, string intermediateDirectory, string binaryOutputDirectory)
        {
            this.sourceDirectory = sourceDirectory;
            this.intermediateDirectory = intermediateDirectory;
            this.binaryOutputDirectory = binaryOutputDirectory;

            // Make the directories if they don't exist.
            
            foreach (string directory in new string[] { sourceDirectory, intermediateDirectory, binaryOutputDirectory })
            {
                try
                {
                    CreateDirectoryIfNotExist(directory);
                }
                catch (Exception e)
                {
                    Logger.Log(logChannel, LogType.Error, "Failed to create directory {0}: {1}", directory, e.Message);
                }
            }
        }

        private void CreateDirectoryIfNotExist(string directory)
        {
            if (!System.IO.Directory.Exists(directory))
            {
                System.IO.Directory.CreateDirectory(directory);
            }
        }

        public void BuildAllProjects()
        {
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
                    Logger.Log(logChannel, LogType.Error, "Failed to delete symlink: {0}", e.Message);
                }
            }

            string[] directories = System.IO.Directory.GetDirectories(sourceDirectory, "*", System.IO.SearchOption.AllDirectories)
                .Append(sourceDirectory)
                .Where(directory => System.IO.Directory.GetFiles(directory, "*.cs").Length > 0)
                .ToArray();

            foreach (string directory in directories)
            {
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

            Logger.Log(logChannel, LogType.Info, "binaryOutputDirectory: {0}", binaryOutputDirectory);

            string[] dlls = System.IO.Directory.GetFiles(binaryOutputDirectory, $"{moduleName}.dll", System.IO.SearchOption.AllDirectories);

            if (dlls.Length == 0)
            {
                // DLL not found, needs rebuild
                return true;
            }

            foreach (string dll in dlls)
            {
                Logger.Log(logChannel, LogType.Info, "Checking DLL: {0}", dll);
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

            Logger.Log(logChannel, LogType.Info, $"Relative path: {relativePath}");

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
            Logger.Log(logChannel, LogType.Info, "Building project in directory {0}", scriptDirectory);

            moduleName = GetModuleNameForScriptDirectory(scriptDirectory);
            hotReloadVersion = -1;

            try
            {
                if (!forceRebuild && !DetectNeedsRebuild(scriptDirectory: scriptDirectory, moduleName: moduleName))
                {
                    Logger.Log(logChannel, LogType.Info, "Skipping rebuild of module {0}, no changes detected", moduleName);

                    return true;
                }
            }
            catch (Exception e)
            {
                Logger.Log(logChannel, LogType.Error, "Failed to detect if module {0} needs rebuild: {1}", moduleName, e.Message);

                return false;
            }

            Logger.Log(logChannel, LogType.Info, "Rebuilding module {0}...", moduleName);

            string projectOutputDirectory = GetProjectOutputDirectory(
                moduleName: moduleName,
                createDirectory: true
            );

            string projectFilePath = GetProjectFilePath(
                moduleName: moduleName,
                projectOutputDirectory: projectOutputDirectory
            );

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

        private bool GenerateCSharpProjectFile(string projectFilePath, string scriptDirectory, string moduleName)
        {
            string projectContent =
                "<Project Sdk=\"Microsoft.NET.Sdk\">\n"
                    + "<PropertyGroup>\n"
                        + "<OutputType>Library</OutputType>\n"
                        + "<TargetFramework>net8.0</TargetFramework>\n"
                        + "<ImplicitUsings>enable</ImplicitUsings>\n"
                        + "<Nullable>enable</Nullable>\n"
                        + "<AllowUnsafeBlocks>true</AllowUnsafeBlocks>\n"
                        + "<EnableDynamicLoading>true</EnableDynamicLoading>\n"
                        + $"<AssemblyName>{moduleName}</AssemblyName>\n"
                    + "</PropertyGroup>\n"
                    + "<ItemGroup>\n"
                    + $"<ProjectReference Include=\"{System.IO.Path.Combine(intermediateDirectory, "HyperionRuntime", "HyperionRuntime.csproj")}\">\n"
                        + "<ReferenceOutputAssembly>true</ReferenceOutputAssembly>\n"
                        + "<Private>true</Private>\n"
                    + "</ProjectReference>\n"
                    + string.Join("", System.IO.Directory.GetFiles(scriptDirectory, "*.cs").Select(script => $"<Compile Include=\"{script}\" />\n"))
                    + "</ItemGroup>\n"
                + "</Project>\n";

            try
            {
                System.IO.File.WriteAllText(projectFilePath, projectContent);
            }
            catch (Exception e)
            {
                Logger.Log(logChannel, LogType.Error, "Failed to write project file: {0}", e.Message);

                return false;
            }

            return true;
        }

        private bool RunDotNetCLI(string projectOutputDirectory, out int hotReloadVersion)
        {
            hotReloadVersion = -1;

            System.Diagnostics.Process process = new System.Diagnostics.Process();
            process.StartInfo.FileName = "dotnet";
            process.StartInfo.Arguments = $"build";
            process.StartInfo.UseShellExecute = false;
            process.StartInfo.WorkingDirectory = projectOutputDirectory;
            process.StartInfo.RedirectStandardOutput = true;
            process.StartInfo.RedirectStandardError = true;
            process.StartInfo.CreateNoWindow = true;
            process.OutputDataReceived += (object sendingProcess, System.Diagnostics.DataReceivedEventArgs eventArgs) =>
            {
                Logger.Log(logChannel, LogType.Info, "{0}", eventArgs.Data);
            };
            process.ErrorDataReceived += (object sendingProcess, System.Diagnostics.DataReceivedEventArgs eventArgs) =>
            {
                Logger.Log(logChannel, LogType.Error, "{0}", eventArgs.Data);
            };
            process.Start();

            process.BeginOutputReadLine();
            process.BeginErrorReadLine();

            process.WaitForExit();

            if (process.ExitCode != 0)
            {
                Logger.Log(logChannel, LogType.Error, "Failed to compile script. Check the output log for more information.");

                MessageBox.Critical()
                    .Title("Script Compilation Error")
                    .Text("Failed to compile script. Check the output log for more information.")
                    .Button("OK", () => { Logger.Log(logChannel, LogType.Info, "OK clicked"); })
                    .Show();

                return false;
            }

            Logger.Log(logChannel, LogType.Info, "Script compiled successfully");

            // Grep all DLLs in the output directory
            string[] dlls = System.IO.Directory.GetFiles(System.IO.Path.Combine(projectOutputDirectory, "bin"), "*.dll", System.IO.SearchOption.AllDirectories);
            string[] outputDlls = new string[dlls.Length];

            for (int i = 0; i < dlls.Length; i++)
            {
                // Copy the DLL to the output directory
                string outputDllPath = System.IO.Path.Combine(binaryOutputDirectory, System.IO.Path.GetFileName(dlls[i]));

                try
                {
                    System.IO.File.Copy(dlls[i], outputDllPath, true);
                }
                catch (Exception e)
                {
                    Logger.Log(logChannel, LogType.Error, "Failed to copy DLL: {0}", e.Message);

                    return false;
                }

                outputDlls[i] = outputDllPath;
            }

            return CreateSymLinks(outputDlls, out hotReloadVersion);
        }

        private bool CreateSymLinks(string[] dlls, out int hotReloadVersion)
        {
            // For each dll file, find the highest foo.X.dll file and create a symlink foo.(X+1).dll to the newly compiled DLL
            // This is to ensure that the game always loads the latest version of the DLL

            hotReloadVersion = 0;

            HashSet<string> directories = new HashSet<string>();

            foreach (string dll in dlls)
            {
                string? directoryName = System.IO.Path.GetDirectoryName(dll);

                if (directoryName == null)
                {
                    Logger.Log(logChannel, LogType.Error, "Failed to get directory name for DLL {0}", dll);

                    continue;
                }

                directories.Add(directoryName);
            }

            HashSet<string> files = new HashSet<string>();

            foreach (string directory in directories)
            {
                string[] directoryFiles = System.IO.Directory.GetFiles(directory, "*.*.dll");

                foreach (string file in directoryFiles)
                {
                    files.Add(file);
                }
            }

            foreach (string file in files)
            {
                string[] splitParts = System.IO.Path.GetFileNameWithoutExtension(file).Split('.');
                string versionString = splitParts[splitParts.Length - 1];

                if (int.TryParse(versionString, out int version))
                {
                    if (version > hotReloadVersion)
                    {
                        hotReloadVersion = version;
                    }
                }
            }

            hotReloadVersion += 1;

            foreach (string dll in dlls)
            {
                string fileName = System.IO.Path.GetFileName(dll);
                string directory = dll.Substring(0, dll.Length - fileName.Length);

                string newFileName = $"{System.IO.Path.GetFileNameWithoutExtension(fileName)}.{hotReloadVersion}.dll";
                string newFilePath = System.IO.Path.Combine(directory, newFileName);

                try
                {
                    if (System.IO.File.Exists(newFilePath))
                    {
                        System.IO.File.Delete(newFilePath);
                    }

                    System.IO.File.CreateSymbolicLink(newFilePath, dll);
                }
                catch (Exception e)
                {
                    Logger.Log(logChannel, LogType.Error, "Failed to create symlink from {0} to {1}: {2}", dll, newFilePath, e.Message);

                    return false;
                }
            }

            return true;
        }

        public bool Compile(ref ManagedScript managedScript)
        {
            string moduleName;
            int hotReloadVersion;

            string? scriptDirectory = System.IO.Path.GetDirectoryName(managedScript.Path);

            if (scriptDirectory == null)
            {
                Logger.Log(logChannel, LogType.Error, "Failed to get script directory for script {0}", managedScript.Path);

                return false;
            }

            if (BuildProject(
                scriptDirectory: (string)scriptDirectory,
                forceRebuild: true,
                moduleName: out moduleName,
                hotReloadVersion: out hotReloadVersion
            ))
            {
                managedScript.AssemblyPath = GetAssemblyPath(moduleName, relative: true);
                managedScript.HotReloadVersion = hotReloadVersion;

                return true;
            }

            return false;
        }
    }
}