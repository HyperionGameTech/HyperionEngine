using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "EditorActionStackState")]
    [Flags]
    public enum EditorActionStackState : byte
    {
        None = 0,
        CanUndo = 0x1,
        CanRedo = 0x2
    }


    [ClassBinding(Name = "EditorActionStack")]
    public class EditorActionStack : ObjectBase
    {
        public EditorActionStack()
        {
        }
    }
}