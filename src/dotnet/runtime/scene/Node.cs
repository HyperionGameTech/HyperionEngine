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

    public class Node
    {
        private ManagedNode managedNode;

        public Node()
        {
            this.managedNode = new ManagedNode();
            Node_Create(out this.managedNode);
        }

        public Node(ManagedNode managedNode)
        {
            this.managedNode = managedNode;
        }

        ~Node()
        {
            managedNode.Free();
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

            ManagedNode childManagedNode = new ManagedNode();
            Node_AddChild(managedNode, node.managedNode, out childManagedNode);

            return node;
        }

        public Node? FindChild(string name)
        {
            ManagedNode childManagedNode = new ManagedNode();
            Node_FindChild(managedNode, name, out childManagedNode);

            if (childManagedNode.refPtr == IntPtr.Zero)
            {
                return null;
            }

            return new Node(childManagedNode);
        }

        public Node? FindChildWithEntity(Entity entity)
        {
            ManagedNode childManagedNode = new ManagedNode();
            Node_FindChildWithEntity(managedNode, ref entity, out childManagedNode);

            if (childManagedNode.refPtr == IntPtr.Zero)
            {
                return null;
            }

            return new Node(childManagedNode);
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
                Transform transform = new Transform();
                Node_GetWorldTransform(managedNode, out transform);
                return transform;
            }
            set
            {
                Node_SetWorldTransform(managedNode, ref value);
            }
        }

        public Transform LocalTransform
        {
            get
            {
                Transform transform = new Transform();
                Node_GetLocalTransform(managedNode, out transform);
                return transform;
            }
            set
            {
                Node_SetLocalTransform(managedNode, ref value);
            }
        }

        public Vec3f WorldTranslation
        {
            get
            {
                Vec3f translation = new Vec3f();
                Node_GetWorldTranslation(managedNode, out translation);
                return translation;
            }
            set
            {
                Node_SetWorldTranslation(managedNode, ref value);
            }
        }

        public Vec3f LocalTranslation
        {
            get
            {
                Vec3f translation = new Vec3f();
                Node_GetLocalTranslation(managedNode, out translation);
                return translation;
            }
            set
            {
                Node_SetLocalTranslation(managedNode, ref value);
            }
        }

        public Quaternion WorldRotation
        {
            get
            {
                Quaternion rotation = new Quaternion();
                Node_GetWorldRotation(managedNode, out rotation);
                return rotation;
            }
            set
            {
                Node_SetWorldRotation(managedNode, ref value);
            }
        }

        public Quaternion LocalRotation
        {
            get
            {
                Quaternion rotation = new Quaternion();
                Node_GetLocalRotation(managedNode, out rotation);
                return rotation;
            }
            set
            {
                Node_SetLocalRotation(managedNode, ref value);
            }
        }

        public Vec3f WorldScale
        {
            get
            {
                Vec3f scale = new Vec3f();
                Node_GetWorldScale(managedNode, out scale);
                return scale;
            }
            set
            {
                Node_SetWorldScale(managedNode, ref value);
            }
        }

        public Vec3f LocalScale
        {
            get
            {
                Vec3f scale = new Vec3f();
                Node_GetLocalScale(managedNode, out scale);
                return scale;
            }
            set
            {
                Node_SetLocalScale(managedNode, ref value);
            }
        }

        public bool TransformLocked
        {
            get
            {
                return Node_IsTransformLocked(managedNode);
            }
            set
            {
                if (value)
                {
                    Node_LockTransform(managedNode);
                }
                else
                {
                    Node_UnlockTransform(managedNode);
                }
            }
        }

        public Entity Entity
        {
            get
            {
                Entity entity = new Entity();
                Node_GetEntity(managedNode, out entity);
                return entity;
            }
            set
            {
                Node_SetEntity(managedNode, ref value);
            }
        }

        public BoundingBox EntityAABB
        {
            get
            {
                BoundingBox aabb = new BoundingBox();
                Node_GetEntityAABB(managedNode, out aabb);
                return aabb;
            }
            set
            {
                Node_SetEntityAABB(managedNode, ref value);
            }
        }

        public BoundingBox WorldAABB
        {
            get
            {
                BoundingBox aabb = new BoundingBox();
                Node_GetWorldAABB(managedNode, out aabb);
                return aabb;
            }
        }

        public BoundingBox LocalAABB
        {
            get
            {
                BoundingBox aabb = new BoundingBox();
                Node_GetLocalAABB(managedNode, out aabb);
                return aabb;
            }
        }

        [DllImport("hyperion", EntryPoint = "Node_Create")]
        private static extern void Node_Create([Out] out ManagedNode managedNode);

        [DllImport("hyperion", EntryPoint = "Node_GetName")]
        private static extern IntPtr Node_GetName(ManagedNode managedNode);

        [DllImport("hyperion", EntryPoint = "Node_SetName")]
        private static extern void Node_SetName(ManagedNode managedNode, [MarshalAs(UnmanagedType.LPStr)] string namePtr);

        [DllImport("hyperion", EntryPoint = "Node_AddChild")]
        private static extern void Node_AddChild(ManagedNode parent, ManagedNode child, [Out] out ManagedNode result);

        [DllImport("hyperion", EntryPoint = "Node_FindChild")]
        private static extern void Node_FindChild(ManagedNode managedNode, [MarshalAs(UnmanagedType.LPStr)] string namePtr, [Out] out ManagedNode result);

        [DllImport("hyperion", EntryPoint = "Node_FindChildWithEntity")]
        private static extern void Node_FindChildWithEntity(ManagedNode managedNode, [In] ref Entity entity, [Out] out ManagedNode result);

        [DllImport("hyperion", EntryPoint = "Node_RemoveChild")]
        private static extern bool Node_RemoveChild(ManagedNode parent, ManagedNode child);

        [DllImport("hyperion", EntryPoint = "Node_GetWorldTransform")]
        private static extern void Node_GetWorldTransform(ManagedNode managedNode, [Out] out Transform transform);

        [DllImport("hyperion", EntryPoint = "Node_SetWorldTransform")]
        private static extern void Node_SetWorldTransform(ManagedNode managedNode, [In] ref Transform transform);

        [DllImport("hyperion", EntryPoint = "Node_GetLocalTransform")]
        private static extern void Node_GetLocalTransform(ManagedNode managedNode, [Out] out Transform transform);

        [DllImport("hyperion", EntryPoint = "Node_SetLocalTransform")]
        private static extern void Node_SetLocalTransform(ManagedNode managedNode, [In] ref Transform transform);

        [DllImport("hyperion", EntryPoint = "Node_GetWorldTranslation")]
        private static extern void Node_GetWorldTranslation(ManagedNode managedNode, [Out] out Vec3f translation);

        [DllImport("hyperion", EntryPoint = "Node_SetWorldTranslation")]
        private static extern void Node_SetWorldTranslation(ManagedNode managedNode, [In] ref Vec3f translation);

        [DllImport("hyperion", EntryPoint = "Node_GetLocalTranslation")]
        private static extern void Node_GetLocalTranslation(ManagedNode managedNode, [Out] out Vec3f translation);

        [DllImport("hyperion", EntryPoint = "Node_SetLocalTranslation")]
        private static extern void Node_SetLocalTranslation(ManagedNode managedNode, [In] ref Vec3f translation);

        [DllImport("hyperion", EntryPoint = "Node_Translate")]
        private static extern void Node_Translate(ManagedNode managedNode, [In] ref Vec3f translation);

        [DllImport("hyperion", EntryPoint = "Node_GetWorldRotation")]
        private static extern void Node_GetWorldRotation(ManagedNode managedNode, [Out] out Quaternion rotation);

        [DllImport("hyperion", EntryPoint = "Node_SetWorldRotation")]
        private static extern void Node_SetWorldRotation(ManagedNode managedNode, [In] ref Quaternion rotation);

        [DllImport("hyperion", EntryPoint = "Node_GetLocalRotation")]
        private static extern void Node_GetLocalRotation(ManagedNode managedNode, [Out] out Quaternion rotation);

        [DllImport("hyperion", EntryPoint = "Node_SetLocalRotation")]
        private static extern void Node_SetLocalRotation(ManagedNode managedNode, [In] ref Quaternion rotation);

        [DllImport("hyperion", EntryPoint = "Node_Rotate")]
        private static extern void Node_Rotate(ManagedNode managedNode, [In] ref Quaternion rotation);

        [DllImport("hyperion", EntryPoint = "Node_GetWorldScale")]
        private static extern void Node_GetWorldScale(ManagedNode managedNode, [Out] out Vec3f scale);

        [DllImport("hyperion", EntryPoint = "Node_SetWorldScale")]
        private static extern void Node_SetWorldScale(ManagedNode managedNode, [In] ref Vec3f scale);

        [DllImport("hyperion", EntryPoint = "Node_GetLocalScale")]
        private static extern void Node_GetLocalScale(ManagedNode managedNode, [Out] out Vec3f scale);

        [DllImport("hyperion", EntryPoint = "Node_SetLocalScale")]
        private static extern void Node_SetLocalScale(ManagedNode managedNode, [In] ref Vec3f scale);

        [DllImport("hyperion", EntryPoint = "Node_Scale")]
        private static extern void Node_Scale(ManagedNode managedNode, [In] ref Vec3f scale);

        [DllImport("hyperion", EntryPoint = "Node_GetEntity")]
        private static extern void Node_GetEntity(ManagedNode managedNode, [Out] out Entity entity);

        [DllImport("hyperion", EntryPoint = "Node_SetEntity")]
        private static extern void Node_SetEntity(ManagedNode managedNode, [In] ref Entity entity);

        [DllImport("hyperion", EntryPoint = "Node_GetEntityAABB")]
        private static extern void Node_GetEntityAABB(ManagedNode managedNode, [Out] out BoundingBox aabb);

        [DllImport("hyperion", EntryPoint = "Node_SetEntityAABB")]
        private static extern void Node_SetEntityAABB(ManagedNode managedNode, [In] ref BoundingBox aabb);

        [DllImport("hyperion", EntryPoint = "Node_GetWorldAABB")]
        private static extern void Node_GetWorldAABB(ManagedNode managedNode, [Out] out BoundingBox aabb);

        [DllImport("hyperion", EntryPoint = "Node_GetLocalAABB")]
        private static extern void Node_GetLocalAABB(ManagedNode managedNode, [Out] out BoundingBox aabb);

        [DllImport("hyperion", EntryPoint = "Node_IsTransformLocked")]
        private static extern bool Node_IsTransformLocked(ManagedNode managedNode);

        [DllImport("hyperion", EntryPoint = "Node_LockTransform")]
        private static extern void Node_LockTransform(ManagedNode managedNode);

        [DllImport("hyperion", EntryPoint = "Node_UnlockTransform")]
        private static extern void Node_UnlockTransform(ManagedNode managedNode);
    }
}