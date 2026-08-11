using System;

namespace Hyperion
{
    [ClassBinding(Name = "SoundFormat")]
    public enum SoundFormat : uint
    {
        Mono8 = 0,
        Mono16 = 1,
        Stereo8 = 2,
        Stereo16 = 3
    }

    [ClassBinding(Name = "Sound")]
    public class Sound : AssetObject
    {
        public Sound()
        {
        }
    }
}
