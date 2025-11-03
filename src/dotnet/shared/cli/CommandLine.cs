using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="CommandLineArgumentDefinitions")]
    [StructLayout(LayoutKind.Explicit, Size = 8)]
    public struct CommandLineArgumentDefinitions
    {
        [FieldOffset(0)]
        private PimplPtr pImpl;

        public CommandLineArgumentDefinitions()
        {
        }
    }

    [ClassBinding(Name="CommandLineArguments")]
    public class CommandLineArguments : HypObject
    {
        public CommandLineArguments()
        {
        }
    }
}