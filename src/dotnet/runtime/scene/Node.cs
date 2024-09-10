using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    // Care must be taken to ensure this object is disposed of properly
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct ManagedNode
    {
        internal IntPtr refPtr;

        public bool IsValid
        {
            get
            {
                return refPtr != IntPtr.Zero;
            }
        }

        public void Free()
        {
            ManagedNode_Dispose(this);

            refPtr = IntPtr.Zero;
        }

        [DllImport("hyperion", EntryPoint = "ManagedNode_Dispose")]
        private static extern void ManagedNode_Dispose(ManagedNode managed_node);
    }

    [HypClassBinding(Name="Node")]
    public class Node : HypObject
    {
        public Node()
        {
        }
    }
}