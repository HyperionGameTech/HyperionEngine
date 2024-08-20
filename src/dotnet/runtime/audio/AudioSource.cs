using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum AudioSourceFormat : uint
    {
        Mono8 = 0,
        Mono16 = 1,
        Stereo8 = 2,
        Stereo16 = 3
    }

    public enum AudioSourceState : uint
    {
        Undefined = 0,
        Stopped = 1,
        Playing = 2,
        Paused = 3
    }

    [HypClassBinding(Name="AudioSource")]
    public class AudioSource : HypObject
    {
        public AudioSource()
        {
        }
    }
}