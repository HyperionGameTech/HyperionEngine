using System;

namespace Hyperion
{
    [ClassBinding(Name = "AnimationGraph")]
    public class AnimationGraph : AssetObject
    {
        public AnimationGraph()
        {
        }
    }

    [ClassBinding(Name = "AnimationGraphInstance")]
    public class AnimationGraphInstance : ObjectBase
    {
    }

    [ClassBinding(Name = "AnimationGraphNode")]
    public abstract class AnimationGraphNode : ObjectBase
    {
    }

    [ClassBinding(Name = "AnimationClipNode")]
    public class AnimationClipNode : AnimationGraphNode
    {
    }

    [ClassBinding(Name = "AnimationBlend1DNode")]
    public class AnimationBlend1DNode : AnimationGraphNode
    {
    }

    [ClassBinding(Name = "AnimationBlend2DNode")]
    public class AnimationBlend2DNode : AnimationGraphNode
    {
    }

    [ClassBinding(Name = "AnimationLayerNode")]
    public class AnimationLayerNode : AnimationGraphNode
    {
    }

    [ClassBinding(Name = "AnimationAdditiveNode")]
    public class AnimationAdditiveNode : AnimationGraphNode
    {
    }

    [ClassBinding(Name = "AnimationChainTwistNode")]
    public class AnimationChainTwistNode : AnimationGraphNode
    {
    }

    [ClassBinding(Name = "AnimationStateMachineNode")]
    public class AnimationStateMachineNode : AnimationGraphNode
    {
    }
}
