using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.Versioning;

namespace Hyperion.Editor.Services
{
    [SupportedOSPlatform("macos")]
    public sealed partial class MacGameBuildPlatform : PosixGameBuildPlatform
    {
        public override string Name => "Darwin";

        public override string CookTargetPlatform => "mac";

        protected override string RuntimeIdentifierOs => "osx";

        private static readonly string[] s_dotNetInstallDirectories = { "/usr/local/share/dotnet", "/opt/homebrew/share/dotnet" };

        protected override IReadOnlyList<string> DotNetInstallDirectories => s_dotNetInstallDirectories;

        private static readonly string[] s_extraToolDirectories =
        {
            "/opt/homebrew/bin",
            "/usr/local/bin",
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".local", "bin")
        };

        protected override IReadOnlyList<string> ExtraToolDirectories => s_extraToolDirectories;

        protected override IReadOnlyList<string> GetCompilerNames(string sdkCompiler)
        {
            return sdkCompiler == "GNU"
                ? ["g++"]
                : ["clang++"];
        }

        protected override string DescribeCompiler(string sdkCompiler)
        {
            return sdkCompiler == "GNU"
                ? "GCC C++ compiler (g++)"
                : "Xcode command line tools (clang++)";
        }

        public override string? FindNativeAotToolchainError(out string? extraPath)
        {
            extraPath = null;

            return null;
        }
    }
}
