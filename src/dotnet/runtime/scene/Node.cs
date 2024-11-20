using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [Flags]
    [HypClassBinding(Name="NodeFlags")]
    public enum NodeFlags : uint
    {
        None = 0x0,

        IgnoreParentTranslation = 0x1,
        IgnoreParentScale = 0x2,
        IgnoreParentRotation = 0x4,
        IgnoreParentTransform = IgnoreParentTranslation | IgnoreParentScale | IgnoreParentRotation,

        ExcludeFromParentAABB = 0x8,

        HideInSceneOutline = 0x100 // Should this node be hidden in the editor's outline window?
    }

    [HypClassBinding(Name="Node")]
    public class Node : HypObject
    {
        public Node()
        {
        }
    }
}