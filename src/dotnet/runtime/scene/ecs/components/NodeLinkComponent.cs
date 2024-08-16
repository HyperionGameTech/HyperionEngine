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
                throw new NotImplementedException();
                // ManagedNode managedNode = new ManagedNode();
                // NodeLinkComponent_LockReference(node.Address, out managedNode);

                // if (!managedNode.IsValid)
                // {
                //     throw new Exception("NodeLinkComponent is not valid");
                // }

                // return new Node(managedNode);
            }
        }

        [DllImport("hyperion", EntryPoint = "NodeLinkComponent_LockReference")]
        private static extern void NodeLinkComponent_LockReference(IntPtr ctrlBlockPtr, [Out] out ManagedNode node);
    }
}
