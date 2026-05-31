using System;

namespace Hyperion
{
    [ClassBinding(Name = "CommandletBase")]
    public abstract class CommandletBase : ObjectBase
    {
        public CommandletBase()
        {
        }

        public abstract Result Run(CommandLineArguments args);
    }
}