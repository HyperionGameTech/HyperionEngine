using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="NodeLinkComponent")]
    [StructLayout(LayoutKind.Explicit, Size = 16)]
    public struct NodeLinkComponent : IComponent
    {
        [FieldOffset(0)]
        private WeakRefCountedPtr<Node> node;

        public NodeLinkComponent()
        {
        }

        public void Dispose()
        {
            // node.Dispose();
        }

        public Node Node
        {
            get
            {
                throw new NotImplementedException();
            }
        }
    }
}
