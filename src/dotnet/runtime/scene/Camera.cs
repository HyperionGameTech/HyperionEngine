using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name="CameraProjectionMode")]
    public enum CameraProjectionMode : uint
    {
        None = 0,
        Perspective = 1,
        Orthographic = 2
    }

    [ClassBinding(Name="CameraFlags")]
    [Flags]
    public enum CameraFlags : uint
    {
        None = 0x0,
        MatchWindowSize = 0x1
    }

    [ClassBinding(Name="Camera")]
    public class Camera : Entity
    {
        public Camera()
        {
        }
    }
}