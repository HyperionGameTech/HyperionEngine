using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;

namespace Hyperion
{
    [Flags]
    [ClassBinding(Name = "NodeFlags")]
    public enum NodeFlags : uint
    {
        None = 0x0,

        IgnoreParentTranslation = 0x1,
        IgnoreParentScale = 0x2,
        IgnoreParentRotation = 0x4,
        IgnoreParentTransform = IgnoreParentTranslation | IgnoreParentScale | IgnoreParentRotation,

        ExcludeFromParentAABB = 0x8,

        Transient = 0x100, // Set if the node should not be serialized.

        HideInSceneOutline = 0x1000 // Should this node be hidden in the editor's outline window?
    }

    [ClassBinding(Name = "TransformChangeType")]
    public enum TransformChangeType : byte
    {
        Default = 0,    // Default transform change, marks the node as dirty so the transform is saved and the editor is aware of the change when not in simulation mode.
        Simulation = 1  // Transform change caused by physics or other simulation (e.g scripts) - should not mark the node as "dirty" for editor modifications.
    }

    [ClassBinding(Name = "Node")]
    public class Node : AssetObject
    {
        public Node()
        {
        }

        public NodeFlags Flags
        {
            get
            {
                return this.GetNodeFlags();
            }
            set
            {
                this.SetNodeFlags(value);
            }
        }

        public bool Dirty
        {
            get
            {
                return this.IsDirty();
            }
        }

        public Transform LocalTransform
        {
            get
            {
                return this.GetLocalTransform();
            }
            set
            {
                this.SetLocalTransform(value, TransformChangeType.Default);
            }
        }

        public Vec3f LocalTranslation
        {
            get
            {
                return this.GetLocalTranslation();
            }
            set
            {
                this.SetLocalTranslation(value, TransformChangeType.Default);
            }
        }

        public Vec3f LocalScale
        {
            get
            {
                return this.GetLocalScale();
            }
            set
            {
                this.SetLocalScale(value, TransformChangeType.Default);
            }
        }

        public Quat4f LocalRotation
        {
            get
            {
                return this.GetLocalRotation();
            }
            set
            {
                this.SetLocalRotation(value, TransformChangeType.Default);
            }
        }

        public Mat4f WorldMatrix
        {
            get
            {
                return this.GetWorldMatrix();
            }
        }

        public BoundingBox LocalBounds
        {
            get
            {
                return this.GetLocalBounds();
            }
            set
            {
                this.SetLocalBounds(value);
            }
        }

        public Node? Parent
        {
            get
            {
                return this.GetParent();
            }
        }

        public Scene? Scene
        {
            get
            {
                return this.GetScene();
            }
        }

        public uint NumChildren
        {
            get
            {
                return this.NumChildren();
            }
        }

        public IEnumerable<Node?> Children
        {
            get
            {
                uint count = NumChildren;
                for (uint i = 0; i < count; i++)
                {
                    Node? child = this.GetChild(i);
                    yield return child;
                }
            }
        }
    }
}