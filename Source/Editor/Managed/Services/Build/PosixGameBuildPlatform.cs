using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.Versioning;

namespace Hyperion.Editor.Services
{
    [SupportedOSPlatform("linux")]
    [SupportedOSPlatform("macos")]
    public abstract partial class PosixGameBuildPlatform : GameBuildPlatform
    {
        private static readonly string[] s_stripToolNames = { "strip", "llvm-strip" };

        public override string ExecutableExtension => string.Empty;

        protected override IReadOnlyList<string> StripToolNames => s_stripToolNames;

        protected abstract IReadOnlyList<string> DotNetInstallDirectories { get; }

        protected virtual IReadOnlyList<string> ExtraToolDirectories => Array.Empty<string>();

        // in order of preference, for the SDK's CMake compiler id
        protected abstract IReadOnlyList<string> GetCompilerNames(string sdkCompiler);

        protected abstract string DescribeCompiler(string sdkCompiler);

        public override NativeToolchain FindNativeToolchain(string sdkCompiler)
        {
            NativeToolchain toolchain = new NativeToolchain
            {
                CMakePath = FindTool("cmake"),
                CompilerPath = GetCompilerNames(sdkCompiler).Select(FindOnPath).FirstOrDefault(path => path != null)
            };

            string? ninjaPath = FindTool("ninja") ?? FindTool("ninja-build");

            if (ninjaPath != null)
            {
                toolchain.Generator = "Ninja";
                toolchain.MakeProgramPath = ninjaPath;
            }
            else
            {
                toolchain.Generator = "Unix Makefiles";
                toolchain.MakeProgramPath = FindTool("make");
            }

            if (toolchain.CompilerPath == null)
            {
                toolchain.Missing.Add(DescribeCompiler(sdkCompiler));
            }

            if (toolchain.CMakePath == null)
            {
                toolchain.Missing.Add("CMake");
            }

            if (toolchain.MakeProgramPath == null)
            {
                toolchain.Missing.Add("Ninja (or Make)");
            }

            return toolchain;
        }

        private string? FindTool(string fileName)
        {
            IEnumerable<string> pathDirectories = (Environment.GetEnvironmentVariable("PATH") ?? string.Empty)
                .Split(Path.PathSeparator, StringSplitOptions.RemoveEmptyEntries)
                .Select(directory => directory.Trim());

            foreach (string directory in pathDirectories.Concat(ExtraToolDirectories).Distinct())
            {
                string candidate;

                try
                {
                    candidate = Path.Combine(directory, fileName);
                }
                catch (ArgumentException)
                {
                    continue;
                }

                if (File.Exists(candidate) && RunsSuccessfully(candidate))
                {
                    return candidate;
                }
            }

            return null;
        }

        private static bool RunsSuccessfully(string toolPath)
        {
            try
            {
                ProcessStartInfo startInfo = new ProcessStartInfo(toolPath)
                {
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    UseShellExecute = false
                };

                startInfo.ArgumentList.Add("--version");

                using Process? process = Process.Start(startInfo);

                if (process == null)
                {
                    return false;
                }

                // drain so a chatty tool can't block on a full pipe
                process.OutputDataReceived += (_, _) => { };
                process.ErrorDataReceived += (_, _) => { };
                
                process.BeginOutputReadLine();
                process.BeginErrorReadLine();

                if (!process.WaitForExit(5000))
                {
                    process.Kill();

                    return false;
                }

                return process.ExitCode == 0;
            }
            catch (Exception)
            {
                return false;
            }
        }

        public override string FindDotNet()
        {
            string?[] userDirectories =
            {
                Environment.GetEnvironmentVariable("DOTNET_ROOT"),
                Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".dotnet")
            };

            return FindOnPath("dotnet")
                ?? userDirectories.Concat(DotNetInstallDirectories)
                    .Where(directory => !string.IsNullOrEmpty(directory))
                    .Select(directory => Path.Combine(directory!, "dotnet"))
                    .FirstOrDefault(File.Exists)
                ?? "dotnet";
        }

        public override string WriteBuildScript(string buildDirectory, NativeToolchain toolchain, IReadOnlyList<IReadOnlyList<string>> commands)
        {
            List<string> lines = ["#!/bin/sh", "set -e"];

            if (toolchain.EnvironmentScriptPath != null)
            {
                lines.Add(". " + Quote(toolchain.EnvironmentScriptPath));
            }

            foreach (IReadOnlyList<string> command in commands)
            {
                lines.Add(string.Join(" ", command.Select(Quote)));
            }

            string scriptPath = Path.Combine(buildDirectory, "Build.sh");
            File.WriteAllText(scriptPath, string.Join("\n", lines) + "\n");

            File.SetUnixFileMode(scriptPath, File.GetUnixFileMode(scriptPath) | UnixFileMode.UserExecute | UnixFileMode.GroupExecute | UnixFileMode.OtherExecute);

            return scriptPath;
        }

        public override ProcessStartInfo CreateBuildScriptStartInfo(string scriptPath, string workingDirectory)
        {
            return BuildProcess.CreateStartInfo("/bin/sh", workingDirectory, [scriptPath]);
        }

        protected static string FormatMissing(IReadOnlyList<string> missing)
        {
            return "Building the game needs:\n  - " + string.Join("\n  - ", missing);
        }

        private static string Quote(string argument)
        {
            return "'" + argument.Replace("'", "'\\''") + "'";
        }
    }
}
