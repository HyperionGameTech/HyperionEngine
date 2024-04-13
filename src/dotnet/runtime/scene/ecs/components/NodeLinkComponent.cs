using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [StructLayout(LayoutKind.Explicit, Size = 8)]
    public struct NodeLinkComponent : IComponent
    {
        [FieldOffset(0)]
        private WeakRefCountedPtr node = WeakRefCountedPtr.Null;

        public NodeLinkComponent()
        {
        }

        public Node Node
        {
            get
            {
                ManagedNode managedNode = NodeLinkComponent_LockReference(node.Address);

                if (!managedNode.Valid)
                {
                    return null;
                }

                return new Node(managedNode);
            }
        }

        [DllImport("hyperion", EntryPoint = "NodeLinkComponent_LockReference")]
        private static extern ManagedNode NodeLinkComponent_LockReference(IntPtr ctrlBlockPtr);
    }
}
