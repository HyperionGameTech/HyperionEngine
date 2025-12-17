using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "ConsoleCommandBase")]
    public abstract class ConsoleCommandBase : ObjectBase
    {
        public ConsoleCommandBase()
        {
        }

        public abstract Result Execute(CommandLineArguments args);
        public abstract CommandLineArgumentDefinitions GetCommandLineArgumentDefinitions();
    }
}