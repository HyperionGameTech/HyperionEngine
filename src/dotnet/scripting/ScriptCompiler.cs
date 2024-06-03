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

        private string GetProjectPath(string scriptPath)
        {
            string basePath = System.IO.Path.GetDirectoryName(scriptPath);
            string relativePath = System.IO.Path.GetRelativePath(basePath, intermediateDirectory);

            return System.IO.Path.Combine(intermediateDirectory, relativePath, new DirectoryInfo(scriptPath).Parent.Name);
        }

        private void GenerateCSharpProjectFile(string scriptPath)
        {
            string scriptBasePath = System.IO.Path.GetDirectoryName(scriptPath);

            // Get relative path to the source directory
            string relativePath = System.IO.Path.GetRelativePath(sourceDirectory, scriptBasePath)
                .Replace(" ", "")
                .Replace(".", "");

            // chop the last part of the relative path
            string moduleName = "";
            
            if (relativePath.Contains("/"))
            {
                moduleName = relativePath.Substring(relativePath.LastIndexOf("/") + 1);
                relativePath = relativePath.Substring(0, relativePath.LastIndexOf("/"));
            }

            if (moduleName == "")
            {
                moduleName = "Default";
            }

            string projectOutputDirectory = System.IO.Path.Combine(intermediateDirectory, relativePath, moduleName);

            if (!System.IO.Directory.Exists(projectOutputDirectory))
            {
                System.IO.Directory.CreateDirectory(projectOutputDirectory);
            }

            string projectFilePath = System.IO.Path.Combine(projectOutputDirectory, $"{moduleName}.csproj");

            string projectContent =
                "<Project Sdk=\"Microsoft.NET.Sdk\">\n"
                    + "<PropertyGroup>\n"
                        + "<OutputType>Library</OutputType>\n"
                        + "<TargetFramework>net8.0</TargetFramework>\n"
                        + "<ImplicitUsings>enable</ImplicitUsings>\n"
                        + "<Nullable>enable</Nullable>\n"
                        + "<CopyLocalLockFileAssemblies>true</CopyLocalLockFileAssemblies>\n"
                        + $"<AssemblyName>{moduleName}</AssemblyName>\n"
                    + "</PropertyGroup>\n"
                    + "<ItemGroup>\n"
                    + $"<ProjectReference Include=\"{System.IO.Path.Combine(intermediateDirectory, "HyperionCore", "HyperionCore.csproj")}\" />\n"
                    + $"<ProjectReference Include=\"{System.IO.Path.Combine(intermediateDirectory, "HyperionRuntime", "HyperionRuntime.csproj")}\" />\n"
                    +   string.Join("", System.IO.Directory.GetFiles(scriptBasePath, "*.cs").Select(script => $"<Compile Include=\"{script}\" />\n"))
                    + "</ItemGroup>\n"
                + "</Project>\n";

            System.IO.File.WriteAllText(projectFilePath, projectContent);

            RunDotNetCLI(projectOutputDirectory);
        }

        private void RunDotNetCLI(string projectPath)
        {
            System.Diagnostics.Process process = new System.Diagnostics.Process();
            process.StartInfo.FileName = "dotnet";
            process.StartInfo.Arguments = $"build";
            process.StartInfo.UseShellExecute = false;
            process.StartInfo.RedirectStandardOutput = false;
            process.StartInfo.RedirectStandardError = false;
            process.StartInfo.WorkingDirectory = projectPath;
            process.Start();

            process.WaitForExit();

            if (process.ExitCode != 0)
            {
                return;
            }

            Logger.Log(LogType.Info, "Script compiled successfully");

            // Grep all DLLs in the output directory
            string[] dlls = System.IO.Directory.GetFiles(System.IO.Path.Combine(projectPath, "bin"), "*.dll", System.IO.SearchOption.AllDirectories);

            foreach (string dll in dlls)
            {
                // Copy the DLL to the output directory
                string outputDllPath = System.IO.Path.Combine(binaryOutputDirectory, System.IO.Path.GetFileName(dll));

                System.IO.File.Copy(dll, outputDllPath, true);

                Console.WriteLine("Copied {0} to {1}", dll, outputDllPath);
            }
        }

        // just testing
        public void Compile(string scriptPath)
        {
            GenerateCSharpProjectFile(scriptPath);
        }
    }
}