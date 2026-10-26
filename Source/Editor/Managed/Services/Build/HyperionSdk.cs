using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection.PortableExecutable;
using System.Text.RegularExpressions;

namespace Hyperion.Editor.Services
{
    public sealed class HyperionSdk
    {
        private static string EditorDirectory => Path.GetDirectoryName(Environment.ProcessPath) ?? AppContext.BaseDirectory;

        public static string ExpectedDirectory => Path.Combine(EditorDirectory, "Sdk");

        public static string ExpectedShippingDirectory => Path.Combine(ExpectedDirectory, "Shipping");

        public string Directory { get; }

        // CMake's compiler id for the engine build.
        public string Compiler { get; }

        public bool IsShipping { get; }

        // The native libraries (and, for a shipping SDK, the managed assemblies) the engine was built with.
        public string RuntimeDirectory { get; }

        private HyperionSdk(string directory, string compiler, bool isShipping, string runtimeDirectory)
        {
            Directory = directory;
            Compiler = compiler;
            IsShipping = isShipping;
            RuntimeDirectory = runtimeDirectory;
        }

        public static HyperionSdk? Find(bool shipping = false)
        {
            if (!shipping)
            {
                return Load(ExpectedDirectory, isShipping: false);
            }

            // an editor package, or an engine build tree where `BuildHyperion sdk`
            // wrote Binaries/<Platform>/ShippingSdk beside the editor's folder
            return Load(ExpectedShippingDirectory, isShipping: true)
                ?? Load(Path.GetFullPath(Path.Combine(EditorDirectory, "..", "ShippingSdk", "Sdk")), isShipping: true);
        }

        public static HyperionSdk? Load(string sdkDirectory, bool isShipping)
        {
            string sdkFile = Path.Combine(sdkDirectory, "CMake", "HyperionSdk.cmake");

            if (!File.Exists(sdkFile))
            {
                return null;
            }

            string sdkText = File.ReadAllText(sdkFile);

            if (isShipping != Regex.IsMatch(sdkText, "set\\(HYPERION_SDK_SHIPPING ON\\)"))
            {
                return null;
            }

            Match compilerMatch = Regex.Match(sdkText, "set\\(HYPERION_SDK_COMPILER \"([^\"]*)\"\\)");

            string packagedRuntimeDirectory = Path.Combine(sdkDirectory, "Bin");
            string runtimeDirectory = isShipping && System.IO.Directory.Exists(packagedRuntimeDirectory)
                ? packagedRuntimeDirectory
                : Path.GetFullPath(Path.Combine(sdkDirectory, ".."));

            return new HyperionSdk(sdkDirectory, compilerMatch.Success ? compilerMatch.Groups[1].Value : string.Empty, isShipping, runtimeDirectory);
        }

        public (List<string> Libraries, List<string> LinkOptions) GetStaticLinkInputs()
        {
            List<string> libraries = new List<string>();
            List<string> linkOptions = new List<string>();

            string librariesFile = Path.Combine(Directory, "StaticLibraries.txt");

            if (!IsShipping || !File.Exists(librariesFile))
            {
                return (libraries, linkOptions);
            }

            foreach (string line in File.ReadAllLines(librariesFile).Select(line => line.Trim()).Where(line => line.Length > 0))
            {
                string packagedLibrary = Path.Combine(Directory, "Lib", line);

                if (File.Exists(packagedLibrary))
                {
                    libraries.Add(packagedLibrary);
                }
                else if (Path.IsPathRooted(line) && File.Exists(line))
                {
                    libraries.Add(line);
                }
                else if (line.StartsWith('-') || (OperatingSystem.IsWindows() && line.StartsWith('/')))
                {
                    linkOptions.Add(line);
                }
                else if (OperatingSystem.IsWindows())
                {
                    // a system library by name (eg d3d12, ws2_32.lib)
                    libraries.Add(Path.HasExtension(line) ? line : line + ".lib");
                }
                else
                {
                    linkOptions.Add("-l" + line);
                }
            }

            Match linkOptionsMatch = Regex.Match(File.ReadAllText(Path.Combine(Directory, "CMake", "HyperionSdk.cmake")), "set\\(HYPERION_SDK_LINK_OPTIONS \"([^\"]*)\"\\)");

            if (linkOptionsMatch.Success)
            {
                linkOptions.AddRange(linkOptionsMatch.Groups[1].Value.Split(';', StringSplitOptions.RemoveEmptyEntries));
            }

            return (libraries, linkOptions);
        }

        public List<string> GetStaticEnginePublishProperties(string intermediateDirectory, bool linksExecutable)
        {
            List<string> properties = new List<string>
            {
                "-p:HyperionBinDir=" + EnsureTrailingSeparator(RuntimeDirectory),
                "-p:HyperionStaticEngine=true"
            };

            if (!linksExecutable)
            {
                return properties;
            }

            (List<string> engineLibraries, List<string> engineLinkOptions) = GetStaticLinkInputs();

            if (engineLibraries.Count == 0)
            {
                throw new InvalidOperationException($"The shipping SDK at {Directory} contains no engine libraries");
            }

            string engineLibrariesFile = Path.Combine(intermediateDirectory, "EngineLibraries.txt");
            string engineLinkOptionsFile = Path.Combine(intermediateDirectory, "EngineLinkOptions.txt");

            System.IO.Directory.CreateDirectory(intermediateDirectory);
            File.WriteAllLines(engineLibrariesFile, engineLibraries);
            File.WriteAllLines(engineLinkOptionsFile, engineLinkOptions);

            properties.Add("-p:HyperionEngineLibrariesFile=" + engineLibrariesFile);
            properties.Add("-p:HyperionEngineLinkOptionsFile=" + engineLinkOptionsFile);

            return properties;
        }

        public IEnumerable<string> GetRuntimeLibraries()
        {
            return System.IO.Directory.GetFiles(RuntimeDirectory).Where(IsNativeLibrary);
        }

        public static bool IsNativeLibrary(string path)
        {
            string extension = Path.GetExtension(path);

            if (extension != ".dll" && extension != ".so" && extension != ".dylib")
            {
                return false;
            }

            try
            {
                using FileStream stream = File.OpenRead(path);
                using PEReader reader = new PEReader(stream);

                return !reader.HasMetadata;
            }
            catch (BadImageFormatException)
            {
                return true;
            }
        }

        public static string EnsureTrailingSeparator(string directory)
        {
            return Path.EndsInDirectorySeparator(directory) ? directory : directory + Path.DirectorySeparatorChar;
        }
    }
}
