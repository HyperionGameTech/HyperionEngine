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

        ExcludeFromParentBounds = 0x8,

        HideInSceneOutline = 0x1000, // Should this node be hidden in the editor's outline window?

        Mobility = 0xE000,
        MobilityStatic = 0x2000,
        MobilityStaticByProxy = 0x4000,
        MobilityDynamic = Mobility & ~(MobilityStatic | MobilityStaticByProxy),

        Default = MobilityStatic
    }

    [ClassBinding(Name = "TransformChangeType")]
    public enum TransformChangeType : byte
    {
        Default = 0,    // Default transform change, marks the node as dirty so the transform is saved and the editor is aware of the change when not in simulation mode.
        Simulation = 1  // Transform change caused by physics or other simulation (e.g scripts) - should not mark the node as modified.
    }

    [ClassBinding(Name = "Node")]
    public class Node : AssetObject
    {
        public Node()
        {
        }

        protected override void Dispose(bool isDisposing)
        {
            if (isDisposing)
            {
                foreach (Node? childNode in Children)
                {
                    childNode?.Dispose();
                }
            }

            base.Dispose(isDisposing);
        }

        public NodeFlags Flags
        {
            get => this.GetNodeFlags();
            set => this.SetNodeFlags(value);
        }

        public Transform LocalTransform
        {
            get => this.GetLocalTransform();
            set => this.SetLocalTransform(value, TransformChangeType.Default);
        }

        public Vec3f LocalTranslation
        {
            get => this.GetLocalTranslation();
            set => this.SetLocalTranslation(value, TransformChangeType.Default);
        }

        public Vec3f LocalScale
        {
            get => this.GetLocalScale();
            set => this.SetLocalScale(value, TransformChangeType.Default);
        }

        public Quat4f LocalRotation
        {
            get => this.GetLocalRotation();
            set => this.SetLocalRotation(value, TransformChangeType.Default);
        }

        public Mat4f WorldMatrix => this.GetWorldMatrix();

        public BoundingBox LocalBounds
        {
            get => this.GetLocalBounds();
            set => this.SetLocalBounds(value);
        }

        public Node? Parent => this.GetParent();

        public Scene? Scene => this.GetScene();

        public uint NumChildren => this.NumChildren();

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