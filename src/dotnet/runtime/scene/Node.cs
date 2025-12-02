using System;
using System.Runtime.InteropServices;

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

    [ClassBinding(Name = "Node")]
    public class Node : ObjectBase
    {
        public Node()
        {
        }

        public Uuid Uuid
        {
            get
            {
                return this.GetUUID();
            }
        }

        public Name Name
        {
            get
            {
                return this.GetName();
            }
            set
            {
                this.SetName(value);
            }
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

        public Transform LocalTransform
        {
            get
            {
                return this.GetLocalTransform();
            }
            set
            {
                this.SetLocalTransform(value);
            }
        }

        public Transform WorldTransform
        {
            get
            {
                return this.GetWorldTransform();
            }
            set
            {
                this.SetWorldTransform(value);
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
    }
}