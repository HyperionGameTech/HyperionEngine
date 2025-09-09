using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="CommandLineArgumentDefinitions")]
    [StructLayout(LayoutKind.Explicit, Size = 8)]
    public struct CommandLineArgumentDefinitions
    {
        [FieldOffset(0)]
        private PimplPtr pImpl;

        public CommandLineArgumentDefinitions()
        {
        }
    }

    [HypClassBinding(Name="CommandLineArguments")]
    public class CommandLineArguments : HypObject
    {
        public CommandLineArguments()
        {
        }
    }
}