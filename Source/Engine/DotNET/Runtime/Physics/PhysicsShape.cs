using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "PhysicsShapeType")]
    public enum PhysicsShapeType : byte
    {
        Box,
        Sphere,
        Plane,
        ConvexHull,
        Capsule,

        Count
    }

    [ClassBinding(Name = "PhysicsShape")]
    public abstract class PhysicsShape : AssetObject
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

    [ClassBinding(Name = "CapsulePhysicsShape")]
    public class CapsulePhysicsShape : PhysicsShape
    {
        public CapsulePhysicsShape()
        {
        }

        public float Radius
        {
            get
            {
                using BoxedValue value = GetProperty(new Name("Radius")).Get(this);
                return (float)value.GetValue()!;
            }
            set => GetProperty(new Name("Radius")).Set(this, new BoxedValue(value));
        }

        public float Height
        {
            get
            {
                using BoxedValue value = GetProperty(new Name("Height")).Get(this);
                return (float)value.GetValue()!;
            }
            set => GetProperty(new Name("Height")).Set(this, new BoxedValue(value));
        }
    }
}