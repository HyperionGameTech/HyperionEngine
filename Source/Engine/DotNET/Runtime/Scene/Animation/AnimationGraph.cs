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
}
