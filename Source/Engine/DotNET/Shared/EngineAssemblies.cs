using System;

namespace Hyperion
{
    public static class EngineAssemblies
    {
        public const string Shared = "Hyperion.NET.Shared";
        public const string Runtime = "Hyperion.NET.Runtime";
        public const string Generators = "Hyperion.NET.Generators";

        // Hyperion.Plugin.<Module>: the C# of a built-in plugin's runtime module
        public const string PluginPrefix = "Hyperion.Plugin.";

        // What script and game assemblies are built against; a newer one of these means they need building again
        public static ReadOnlySpan<string> BuildDependencies => new[] { Shared, Runtime, Generators };

        public static string FileName(string assemblyName)
        {
            return assemblyName + ".dll";
        }

        public static string[] FindPluginAssemblies(string directory)
        {
            return Directory.Exists(directory)
                ? Array.FindAll(
                    Directory.GetFiles(directory, FileName(PluginPrefix + "*")),
                    path => !path.EndsWith(FileName("Editor"), StringComparison.Ordinal))
                : Array.Empty<string>();
        }
    }
}
