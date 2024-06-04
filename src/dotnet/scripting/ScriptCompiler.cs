using System;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.MSBuild;
using Microsoft.CodeAnalysis.Text;
using Microsoft.CodeAnalysis.Emit;

namespace Hyperion
{
    public class ScriptCompiler
    {
        private string sourceDirectory;
        private string intermediateDirectory;
        private string binaryOutputDirectory;

        public ScriptCompiler(string sourceDirectory, string intermediateDirectory, string binaryOutputDirectory)
        {
            this.sourceDirectory = sourceDirectory;
            this.intermediateDirectory = intermediateDirectory;
            this.binaryOutputDirectory = binaryOutputDirectory;
        }

        public void BuildAllProjects()
        {
            string[] directories = System.IO.Directory.GetDirectories(sourceDirectory, "*", System.IO.SearchOption.AllDirectories)
                .Append(sourceDirectory)
                .Where(directory => System.IO.Directory.GetFiles(directory, "*.cs").Length > 0)
                .ToArray();

            foreach (string directory in directories)
            {
                BuildProject(directory, false);
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

            // Try to find the DLL in the output directory
            string[] dlls = System.IO.Directory.GetFiles(binaryOutputDirectory, $"{moduleName}.dll", System.IO.SearchOption.AllDirectories);

            if (dlls.Length == 0)
            {
                // DLL not found, needs rebuild
                return true;
            }

            foreach (string dll in dlls)
            {
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

            Logger.Log(LogType.Info, $"Relative path: {relativePath}");

            if (relativePath.EndsWith("/"))
            {
                relativePath = relativePath.Substring(0, relativePath.Length - 1);
            }

            // chop the last part of the relative path
            string moduleName = "GameName";

            if (relativePath.Length != 0)
            {
                moduleName += "." + relativePath.Replace("/", ".");
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

        private void BuildProject(string scriptDirectory, bool forceRebuild = false)
        {
            string moduleName = GetModuleNameForScriptDirectory(scriptDirectory);

            if (!forceRebuild && !DetectNeedsRebuild(scriptDirectory: scriptDirectory, moduleName: moduleName))
            {
                Logger.Log(LogType.Info, "Skipping rebuild of module {0}, no changes detected", moduleName);

                return;
            }

            Logger.Log(LogType.Info, "Rebuilding module {0}...", moduleName);

            string projectOutputDirectory = GetProjectOutputDirectory(
                moduleName: moduleName,
                createDirectory: true
            );

            string projectFilePath = GetProjectFilePath(
                moduleName: moduleName,
                projectOutputDirectory: projectOutputDirectory
            );

            GenerateCSharpProjectFile(
                projectFilePath: projectFilePath,
                scriptDirectory: scriptDirectory,
                moduleName: moduleName
            );

            RunDotNetCLI(projectOutputDirectory);
        }

        private void GenerateCSharpProjectFile(string projectFilePath, string scriptDirectory, string moduleName)
        {
            string projectContent =
                "<Project Sdk=\"Microsoft.NET.Sdk\">\n"
                    + "<PropertyGroup>\n"
                        + "<OutputType>Library</OutputType>\n"
                        + "<TargetFramework>net8.0</TargetFramework>\n"
                        + "<ImplicitUsings>enable</ImplicitUsings>\n"
                        + "<Nullable>enable</Nullable>\n"
                        + $"<AssemblyName>{moduleName}</AssemblyName>\n"
                    + "</PropertyGroup>\n"
                    + "<ItemGroup>\n"
                    + $"<ProjectReference Include=\"{System.IO.Path.Combine(intermediateDirectory, "HyperionCore", "HyperionCore.csproj")}\" />\n"
                    + $"<ProjectReference Include=\"{System.IO.Path.Combine(intermediateDirectory, "HyperionRuntime", "HyperionRuntime.csproj")}\" />\n"
                    +   string.Join("", System.IO.Directory.GetFiles(scriptDirectory, "*.cs").Select(script => $"<Compile Include=\"{script}\" />\n"))
                    + "</ItemGroup>\n"
                + "</Project>\n";

            System.IO.File.WriteAllText(projectFilePath, projectContent);
        }

        private void RunDotNetCLI(string projectOutputDirectory)
        {
            System.Diagnostics.Process process = new System.Diagnostics.Process();
            process.StartInfo.FileName = "dotnet";
            process.StartInfo.Arguments = $"build";
            process.StartInfo.UseShellExecute = false;
            process.StartInfo.RedirectStandardOutput = false;
            process.StartInfo.RedirectStandardError = false;
            process.StartInfo.WorkingDirectory = projectOutputDirectory;
            process.Start();

            process.WaitForExit();

            if (process.ExitCode != 0)
            {
                return;
            }

            Logger.Log(LogType.Info, "Script compiled successfully");

            // Grep all DLLs in the output directory
            string[] dlls = System.IO.Directory.GetFiles(System.IO.Path.Combine(projectOutputDirectory, "bin"), "*.dll", System.IO.SearchOption.AllDirectories);

            foreach (string dll in dlls)
            {
                // Copy the DLL to the output directory
                string outputDllPath = System.IO.Path.Combine(binaryOutputDirectory, System.IO.Path.GetFileName(dll));

                System.IO.File.Copy(dll, outputDllPath, true);

                Console.WriteLine("Copied {0} to {1}", dll, outputDllPath);
            }
        }

        public void Compile(string scriptPath)
        {
            BuildProject(
                scriptDirectory: System.IO.Path.GetDirectoryName(scriptPath),
                forceRebuild: true
            );
        }
    }
}