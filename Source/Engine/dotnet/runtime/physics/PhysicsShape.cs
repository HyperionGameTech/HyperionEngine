using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "PhysicsShape")]
    public abstract class PhysicsShape : ObjectBase
    {
        public PhysicsShape()
        {
        }
    }

    [ClassBinding(Name = "BoxPhysicsShape")]
    public class BoxPhysicsShape : PhysicsShape
    {
        public BoxPhysicsShape()
        {
        }
    }

    [ClassBinding(Name = "SpherePhysicsShape")]
    public class SpherePhysicsShape : PhysicsShape
    {
        public SpherePhysicsShape()
        {
        }
    }

    [ClassBinding(Name = "PlanePhysicsShape")]
    public class PlanePhysicsShape : PhysicsShape
    {
        public PlanePhysicsShape()
        {
        }
    }

    [ClassBinding(Name = "ConvexHullPhysicsShape")]
    public class ConvexHullPhysicsShape : PhysicsShape
    {
        public ConvexHullPhysicsShape()
        {
        }
    }
}