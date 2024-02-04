using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    // Care must be taken to ensure this object is disposed of properly,
    // so it must only be held by one object at a time.
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct ManagedNode
    {
        public IntPtr refPtr;

        public bool Valid
        {
            get
            {
                return refPtr != IntPtr.Zero;
            }
        }

        public void Dispose()
        {
            ManagedNode_Dispose(this);

            refPtr = IntPtr.Zero;
        }

        [DllImport("libhyperion", EntryPoint = "ManagedNode_Dispose")]
        private static extern void ManagedNode_Dispose(ManagedNode managed_node);
    }

    public class Node : IDisposable
    {
        private ManagedNode managedNode;

        public Node()
        {
            this.managedNode = Node_Create();
        }

        public Node(ManagedNode managedNode)
        {
            this.managedNode = managedNode;
        }

        public void Dispose()
        {
            managedNode.Dispose();
        }

        public string Name
        {
            get
            {
                return Marshal.PtrToStringAnsi(Node_GetName(managedNode));
            }
            set
            {
                Node_SetName(managedNode, value);
            }
        }

        public Node AddChild()
        {
            return AddChild(new Node());
        }

        public Node AddChild(Node node)
        {
            if (node == null)
            {
                throw new ArgumentNullException(nameof(node));
            }

            Node_AddChild(managedNode, node.managedNode);

            return node;
        }

        public Node? FindChild(string name)
        {
            ManagedNode child = Node_FindChild(managedNode, name);

            if (child.refPtr == IntPtr.Zero)
            {
                return null;
            }

            return new Node(child);
        }

        public Node? FindChildWithEntity(Entity entity)
        {
            ManagedNode child = Node_FindChildWithEntity(managedNode, entity);

            if (child.refPtr == IntPtr.Zero)
            {
                return null;
            }

            return new Node(child);
        }

        public bool RemoveChild(Node child)
        {
            if (child == null)
            {
                throw new ArgumentNullException(nameof(child));
            }

            return Node_RemoveChild(managedNode, child.managedNode);
        }
        
        public Transform WorldTransform
        {
            get
            {
                return Node_GetWorldTransform(managedNode);
            }
            set
            {
                Node_SetWorldTransform(managedNode, value);
            }
        }

        public Transform LocalTransform
        {
            get
            {
                return Node_GetLocalTransform(managedNode);
            }
            set
            {
                Node_SetLocalTransform(managedNode, value);
            }
        }

        public Vec3f WorldTranslation
        {
            get
            {
                return Node_GetWorldTranslation(managedNode);
            }
            set
            {
                Node_SetWorldTranslation(managedNode, value);
            }
        }

        public Vec3f LocalTranslation
        {
            get
            {
                return Node_GetLocalTranslation(managedNode);
            }
            set
            {
                Node_SetLocalTranslation(managedNode, value);
            }
        }

        public Quaternion WorldRotation
        {
            get
            {
                return Node_GetWorldRotation(managedNode);
            }
            set
            {
                Node_SetWorldRotation(managedNode, value);
            }
        }

        public Quaternion LocalRotation
        {
            get
            {
                return Node_GetLocalRotation(managedNode);
            }
            set
            {
                Node_SetLocalRotation(managedNode, value);
            }
        }

        public Vec3f WorldScale
        {
            get
            {
                return Node_GetWorldScale(managedNode);
            }
            set
            {
                Node_SetWorldScale(managedNode, value);
            }
        }

        public Vec3f LocalScale
        {
            get
            {
                return Node_GetLocalScale(managedNode);
            }
            set
            {
                Node_SetLocalScale(managedNode, value);
            }
        }

        public Entity Entity
        {
            get
            {
                return Node_GetEntity(managedNode);
            }
            set
            {
                Node_SetEntity(managedNode, value);
            }
        }

        public BoundingBox WorldAABB
        {
            get
            {
                return Node_GetWorldAABB(managedNode);
            }
        }

        public BoundingBox LocalAABB
        {
            get
            {
                return Node_GetLocalAABB(managedNode);
            }
        }

        [DllImport("libhyperion", EntryPoint = "Node_Create")]
        private static extern ManagedNode Node_Create();

        [DllImport("libhyperion", EntryPoint = "Node_GetName")]
        private static extern IntPtr Node_GetName(ManagedNode managedNode);

        [DllImport("libhyperion", EntryPoint = "Node_SetName")]
        private static extern void Node_SetName(ManagedNode managedNode, string name);

        [DllImport("libhyperion", EntryPoint = "Node_AddChild")]
        private static extern ManagedNode Node_AddChild(ManagedNode parent, ManagedNode child);

        [DllImport("libhyperion", EntryPoint = "Node_FindChild")]
        private static extern ManagedNode Node_FindChild(ManagedNode managedNode, string name);

        [DllImport("libhyperion", EntryPoint = "Node_FindChildWithEntity")]
        private static extern ManagedNode Node_FindChildWithEntity(ManagedNode managedNode, Entity entity);

        [DllImport("libhyperion", EntryPoint = "Node_RemoveChild")]
        private static extern bool Node_RemoveChild(ManagedNode parent, ManagedNode child);

        [DllImport("libhyperion", EntryPoint = "Node_GetWorldTransform")]
        private static extern Transform Node_GetWorldTransform(ManagedNode managedNode);

        [DllImport("libhyperion", EntryPoint = "Node_SetWorldTransform")]
        private static extern void Node_SetWorldTransform(ManagedNode managedNode, Transform transform);

        [DllImport("libhyperion", EntryPoint = "Node_GetLocalTransform")]
        private static extern Transform Node_GetLocalTransform(ManagedNode managedNode);

        [DllImport("libhyperion", EntryPoint = "Node_SetLocalTransform")]
        private static extern void Node_SetLocalTransform(ManagedNode managedNode, Transform transform);

        [DllImport("libhyperion", EntryPoint = "Node_GetWorldTranslation")]
        private static extern Vec3f Node_GetWorldTranslation(ManagedNode managedNode);

        [DllImport("libhyperion", EntryPoint = "Node_SetWorldTranslation")]
        private static extern void Node_SetWorldTranslation(ManagedNode managedNode, Vec3f translation);

        [DllImport("libhyperion", EntryPoint = "Node_GetLocalTranslation")]
        private static extern Vec3f Node_GetLocalTranslation(ManagedNode managedNode);

        [DllImport("libhyperion", EntryPoint = "Node_SetLocalTranslation")]
        private static extern void Node_SetLocalTranslation(ManagedNode managedNode, Vec3f translation);

        [DllImport("libhyperion", EntryPoint = "Node_Translate")]
        private static extern void Node_Translate(ManagedNode managedNode, Vec3f translation);

        [DllImport("libhyperion", EntryPoint = "Node_GetWorldRotation")]
        private static extern Quaternion Node_GetWorldRotation(ManagedNode managedNode);

        [DllImport("libhyperion", EntryPoint = "Node_SetWorldRotation")]
        private static extern void Node_SetWorldRotation(ManagedNode managedNode, Quaternion rotation);

        [DllImport("libhyperion", EntryPoint = "Node_GetLocalRotation")]
        private static extern Quaternion Node_GetLocalRotation(ManagedNode managedNode);

        [DllImport("libhyperion", EntryPoint = "Node_SetLocalRotation")]
        private static extern void Node_SetLocalRotation(ManagedNode managedNode, Quaternion rotation);

        [DllImport("libhyperion", EntryPoint = "Node_Rotate")]
        private static extern void Node_Rotate(ManagedNode managedNode, Quaternion rotation);

        [DllImport("libhyperion", EntryPoint = "Node_GetWorldScale")]
        private static extern Vec3f Node_GetWorldScale(ManagedNode managedNode);

        [DllImport("libhyperion", EntryPoint = "Node_SetWorldScale")]
        private static extern void Node_SetWorldScale(ManagedNode managedNode, Vec3f scale);

        [DllImport("libhyperion", EntryPoint = "Node_GetLocalScale")]
        private static extern Vec3f Node_GetLocalScale(ManagedNode managedNode);

        [DllImport("libhyperion", EntryPoint = "Node_SetLocalScale")]
        private static extern void Node_SetLocalScale(ManagedNode managedNode, Vec3f scale);

        [DllImport("libhyperion", EntryPoint = "Node_Scale")]
        private static extern void Node_Scale(ManagedNode managedNode, Vec3f scale);

        [DllImport("libhyperion", EntryPoint = "Node_GetEntity")]
        private static extern Entity Node_GetEntity(ManagedNode managedNode);

        [DllImport("libhyperion", EntryPoint = "Node_SetEntity")]
        private static extern void Node_SetEntity(ManagedNode managedNode, Entity entity);

        [DllImport("libhyperion", EntryPoint = "Node_GetWorldAABB")]
        private static extern BoundingBox Node_GetWorldAABB(ManagedNode managedNode);

        [DllImport("libhyperion", EntryPoint = "Node_GetLocalAABB")]
        private static extern BoundingBox Node_GetLocalAABB(ManagedNode managedNode);
    }
}