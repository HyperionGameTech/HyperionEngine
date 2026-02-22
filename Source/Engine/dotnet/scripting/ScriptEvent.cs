using System;
using System.IO;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum ScriptEventType : uint
    {
        None = 0,
        StateChanged = 1
    }

    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct ScriptEvent
    {
        private ScriptEventType type;
        private IntPtr scriptPtr;

        public ScriptEventType Type
        {
            get
            {
                return type;
            }
            set
            {
                type = value;
            }
        }

        public IntPtr ScriptPtr
        {
            get
            {
                return scriptPtr;
            }
            set
            {
                scriptPtr = value;
            }
        }
    }
}