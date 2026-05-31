using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "EditorCommandBase")]
    public abstract class EditorCommandBase : ObjectBase
    {
        public EditorCommandBase()
        {
        }

        public IEnumerable<string> Arguments
        {
            get
            {
                int argCount = this.NumArguments();
                for (int i = 0; i < argCount; i++)
                {
                    yield return this.GetArgument(i);
                }
            }
        }
    }
}