using System;

namespace Hyperion
{
    [ClassBinding(Name = "MotionDatabase")]
    public class MotionDatabase : AssetObject
    {
        public MotionDatabase()
        {
        }
    }

    [ClassBinding(Name = "AnimationMotionMatchingNode")]
    public class AnimationMotionMatchingNode : AnimationGraphNode
    {
    }
}
