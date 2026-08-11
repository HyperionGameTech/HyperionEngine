using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public enum AudioSourceState : uint
    {
        Undefined = 0,
        Stopped = 1,
        Playing = 2,
        Paused = 3
    }

    [ClassBinding(Name = "AudioSource")]
    public class AudioSource : ObjectBase
    {
        public AudioSource()
        {
        }
    }
}