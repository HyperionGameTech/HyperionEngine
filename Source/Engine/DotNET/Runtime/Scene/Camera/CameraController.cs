using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "CameraController")]
    public class CameraController : ObjectBase
    {
        public CameraController()
        {
        }
    }

    [ClassBinding(Name = "NullCameraController")]
    public class NullCameraController : CameraController
    {
        public NullCameraController()
        {
        }
    }

    [ClassBinding(Name = "OrthoCameraController")]
    public class OrthoCameraController : CameraController
    {
        public OrthoCameraController()
        {
        }
    }

    [ClassBinding(Name = "PerspectiveCameraController")]
    public class PerspectiveCameraController : CameraController
    {
        public PerspectiveCameraController()
        {
        }
    }

    [ClassBinding(Name = "FirstPersonCameraControllerMode")]
    public enum FirstPersonCameraControllerMode : uint
    {
        MouseLocked = 0,
        MouseFree = 1
    }

    [ClassBinding(Name = "FollowCameraController")]
    public class FollowCameraController : PerspectiveCameraController
    {
        public FollowCameraController()
        {
        }
    }

    [ClassBinding(Name = "FirstPersonCameraController")]
    public class FirstPersonCameraController : PerspectiveCameraController
    {
        public FirstPersonCameraController()
        {
        }
    }

    [ClassBinding(Name = "ThirdPersonCameraController")]
    public class ThirdPersonCameraController : PerspectiveCameraController
    {
        public ThirdPersonCameraController()
        {
        }
    }

    [ClassBinding(Name = "CameraTrackController")]
    public class CameraTrackController : PerspectiveCameraController
    {
        public CameraTrackController()
        {
        }
    }
}