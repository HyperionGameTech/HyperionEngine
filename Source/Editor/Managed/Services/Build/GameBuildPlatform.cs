using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;

namespace Hyperion.Editor.Services
{
    public abstract partial class GameBuildPlatform
    {
        private static readonly Lazy<GameBuildPlatform> s_current = new Lazy<GameBuildPlatform>(CreateForHost);

        public static GameBuildPlatform Current => s_current.Value;

        /// CMAKE_SYSTEM_NAME
        public abstract string Name { get; }

        public abstract string CookTargetPlatform { get; }

        public abstract string ExecutableExtension { get; }

        protected abstract string RuntimeIdentifierOs { get; }

        protected abstract IReadOnlyList<string> StripToolNames { get; }

        public string RuntimeIdentifier
        {
            get
            {
                string archName = RuntimeInformation.OSArchitecture switch
                {
                    Architecture.Arm64 => "arm64",
                    Architecture.X64 => "x64",
                    _ => "invalid"
                };

                return RuntimeIdentifierOs + "-" + archName;
            }
        }

        public abstract NativeToolchain FindNativeToolchain(string sdkCompiler);

        public abstract string? FindNativeAotToolchainError(out string? extraPath);

        public abstract string WriteBuildScript(string buildDirectory, NativeToolchain toolchain, IReadOnlyList<IReadOnlyList<string>> commands);

        public abstract ProcessStartInfo CreateBuildScriptStartInfo(string scriptPath, string workingDirectory);

        public virtual string FindDotNet()
        {
            return "dotnet";
        }

        public virtual string? FindStripTool()
        {
            foreach (string toolName in StripToolNames)
            {
                string? toolPath = FindOnPath(toolName);

                if (toolPath != null)
                {
                    return toolPath;
                }
            }

            return null;
        }

        public string GetExecutableFileName(string name)
        {
            return name + ExecutableExtension;
        }

        public static string? FindOnPath(string fileName)
        {
            var iter = (Environment.GetEnvironmentVariable("PATH") ?? string.Empty)
                .Split(Path.PathSeparator, StringSplitOptions.RemoveEmptyEntries);

            foreach (string directory in iter)
            {
                try
                {
                    string candidate = Path.Combine(directory.Trim(), fileName);

                    if (File.Exists(candidate))
                    {
                        return candidate;
                    }
                }
                catch (ArgumentException)
                {
                }
            }

            return null;
        }

        protected static string? FirstExisting(params string[] paths)
        {
            foreach (string path in paths)
            {
                if (File.Exists(path))
                {
                    return path;
                }
            }

            return null;
        }

        private static GameBuildPlatform CreateForHost()
        {
            if (OperatingSystem.IsWindows())
            {
                return new WindowsGameBuildPlatform();
            }

            if (OperatingSystem.IsLinux())
            {
                return new LinuxGameBuildPlatform();
            }

            if (OperatingSystem.IsMacOS())
            {
                return new MacGameBuildPlatform();
            }

            throw new PlatformNotSupportedException("Building games is not supported on " + RuntimeInformation.OSDescription);
        }
    }
}
