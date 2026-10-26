using System;
using System.Collections.Generic;
using System.IO;

namespace Hyperion.Editor.Services
{
    /// The CMake configure + build of a C++ game project, run by NativeGameBuildService and by the game project tests
    public static class NativeGameBuild
    {
        public static string GetBuildDirectory(string projectDirectory, GameBuildPlatform platform, bool isPackage)
        {
            return Path.Combine(projectDirectory, "Build", isPackage ? platform.Name + "Package" : platform.Name);
        }

        /// Package Game's build: against the shipping SDK, with the engine (and the NativeAOT-compiled scripts, if any) linked in
        public static List<string> GetPackageArguments(string outputDirectory, string? scriptsLinkInputsFile, string? pluginModulesFile = null)
        {
            return new List<string>
            {
                "-DHYPERION_PACKAGE=ON",
                "-DGAME_OUTPUT_DIR=" + ToCMakePath(outputDirectory),
                "-DHYPERION_SCRIPTS_LINK_INPUTS=" + (scriptsLinkInputsFile != null ? ToCMakePath(scriptsLinkInputsFile) : ""),
                "-DHYPERION_PLUGIN_MODULES_FILE=" + (pluginModulesFile != null ? ToCMakePath(pluginModulesFile) : "")
            };
        }

        /// Package Game writes the project's enabled built-in plugin runtime modules here, beside the game module
        public static string? GetPluginModulesFile(string binariesDirectory)
        {
            string path = Path.Combine(binariesDirectory, "PluginModules.txt");

            return File.Exists(path) ? path : null;
        }

        /// Writes the script that configures and builds the game in buildDirectory inside the toolchain's environment, and returns its path
        public static string WriteBuildScript(
            GameBuildPlatform platform,
            NativeToolchain toolchain,
            HyperionSdk sdk,
            string projectDirectory,
            string buildDirectory,
            IReadOnlyList<string> cmakeArguments)
        {
            Directory.CreateDirectory(buildDirectory);

            List<string> configureCommand = new List<string>
            {
                toolchain.CMakePath!,
                "-S", Path.Combine(projectDirectory, "Source"),
                "-B", buildDirectory,
                "-G", toolchain.Generator,
                "-DCMAKE_MAKE_PROGRAM=" + ToCMakePath(toolchain.MakeProgramPath!),
                "-DCMAKE_BUILD_TYPE=Release",
                "-DCMAKE_CXX_COMPILER=" + ToCMakePath(toolchain.CompilerPath!),
                "-DHYPERION_SDK_DIR=" + ToCMakePath(sdk.Directory)
            };

            configureCommand.AddRange(cmakeArguments);

            string[] buildCommand = { toolchain.CMakePath!, "--build", buildDirectory, "--parallel", Environment.ProcessorCount.ToString() };

            return platform.WriteBuildScript(buildDirectory, toolchain, new IReadOnlyList<string>[] { configureCommand, buildCommand });
        }

        public static string ToCMakePath(string path)
        {
            return path.Replace('\\', '/');
        }
    }
}
