using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="PhysicsShape")]
    public abstract class PhysicsShape : HypObject
    {
        public PhysicsShape()
        {
        }
    }

    [HypClassBinding(Name="BoxPhysicsShape")]
    public class BoxPhysicsShape : PhysicsShape
    {
        public BoxPhysicsShape()
        {
        }
    }

    [HypClassBinding(Name="SpherePhysicsShape")]
    public class SpherePhysicsShape : PhysicsShape
    {
        public SpherePhysicsShape()
        {
        }
    }

    [HypClassBinding(Name="PlanePhysicsShape")]
    public class PlanePhysicsShape : PhysicsShape
    {
        public PlanePhysicsShape()
        {
        }
    }

    [HypClassBinding(Name="ConvexHullPhysicsShape")]
    public class ConvexHullPhysicsShape : PhysicsShape
    {
        public ConvexHullPhysicsShape()
        {
        }
    }
}