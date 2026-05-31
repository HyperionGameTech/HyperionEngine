using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "AnimationTrack")]
    public class AnimationTrack : AssetObject
    {
    }

    [ClassBinding(Name = "Animation")]
    public class Animation : AssetObject
    {
        public Animation()
        {
        }
    }
}