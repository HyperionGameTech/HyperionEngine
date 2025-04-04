using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="ConsoleCommandBase")]
    public abstract class ConsoleCommandBase : HypObject
    {
        public ConsoleCommandBase()
        {
        }

        public abstract Result Execute(CommandLineArguments args);
        public abstract CommandLineArgumentDefinitions GetCommandLineArgumentDefinitions();
    }
}