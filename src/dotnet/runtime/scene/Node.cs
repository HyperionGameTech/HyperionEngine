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

        public string Name
        {
            get
            {
                return (string)GetProperty(PropertyNames.Name)
                    .InvokeGetter(this)
                    .GetValue();
            }
            set
            {
                GetProperty(PropertyNames.Name)
                    .InvokeSetter(this, new HypData(value));
            }
        }

        public Entity Entity
        {
            get
            {
                return new Entity((IDBase)GetProperty(PropertyNames.Entity)
                    .InvokeGetter(this)
                    .GetValue());
            }
            set
            {
                GetProperty(PropertyNames.Entity)
                    .InvokeSetter(this, new HypData(value.ID));
            }
        }

        public BoundingBox EntityAABB
        {
            get
            {
                return (BoundingBox)GetProperty(PropertyNames.EntityAABB)
                    .InvokeGetter(this)
                    .GetValue();
            }
            set
            {
                GetProperty(PropertyNames.EntityAABB)
                    .InvokeSetter(this, new HypData(value));
            }
        }

        public Transform LocalTransform
        {
            get
            {
                return (Transform)GetProperty(PropertyNames.LocalTransform)
                    .InvokeGetter(this)
                    .GetValue();
            }
            set
            {
                GetProperty(PropertyNames.LocalTransform)
                    .InvokeSetter(this, new HypData(value));
            }
        }

        public Transform WorldTransform
        {
            get
            {
                return (Transform)GetProperty(PropertyNames.WorldTransform)
                    .InvokeGetter(this)
                    .GetValue();
            }
            set
            {
                GetProperty(PropertyNames.WorldTransform)
                    .InvokeSetter(this, new HypData(value));
            }
        }
    }
}