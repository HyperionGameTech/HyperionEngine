using System;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class Vec3fViewModel : VectorPropertyViewModelBase<Vec3f>
    {
        public Vec3fViewModel(ObjectBase target, Property property, bool isReadOnly)
            : base(target, property, isReadOnly, 3,
                  (v, i) => i switch
                  {
                      0 => v.x,
                      1 => v.y,
                      2 => v.z,
                      _ => 0f
                  },
                  (v, i, val) => i switch
                  {
                      0 => new Vec3f(val, v.y, v.z),
                      1 => new Vec3f(v.x, val, v.z),
                      2 => new Vec3f(v.x, v.y, val),
                      _ => v
                  })
        {
        }

        public Vec3fViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly)
            : base(classAddress, targetAddressResolver, property, isReadOnly, 3,
                  (v, i) => i switch
                  {
                      0 => v.x,
                      1 => v.y,
                      2 => v.z,
                      _ => 0f
                  },
                  (v, i, val) => i switch
                  {
                      0 => new Vec3f(val, v.y, v.z),
                      1 => new Vec3f(v.x, val, v.z),
                      2 => new Vec3f(v.x, v.y, val),
                      _ => v
                  })
        {
        }

        // For delegated use (eg. transform subcomponents)
        public Vec3fViewModel(ObjectBase target, Property property, bool isReadOnly,
            System.Func<Vec3f> readOverride, System.Action<Vec3f> writeOverride)
            : base(target, property, isReadOnly, 3,
                  (v, i) => i switch
                  {
                      0 => v.x,
                      1 => v.y,
                      2 => v.z,
                      _ => 0f
                  },
                  (v, i, val) => i switch
                  {
                      0 => new Vec3f(val, v.y, v.z),
                      1 => new Vec3f(v.x, val, v.z),
                      2 => new Vec3f(v.x, v.y, val),
                      _ => v
                  },
                  readOverride,
                  writeOverride)
        {
        }

        // For delegated use with component targets
        public Vec3fViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly,
            System.Func<Vec3f> readOverride, System.Action<Vec3f> writeOverride)
            : base(classAddress, targetAddressResolver, property, isReadOnly, 3,
                  (v, i) => i switch
                  {
                      0 => v.x,
                      1 => v.y,
                      2 => v.z,
                      _ => 0f
                  },
                  (v, i, val) => i switch
                  {
                      0 => new Vec3f(val, v.y, v.z),
                      1 => new Vec3f(v.x, val, v.z),
                      2 => new Vec3f(v.x, v.y, val),
                      _ => v
                  },
                  readOverride,
                  writeOverride)
        {
        }

        public Vec3fViewModel(string label, TypeInfo typeInfo, Func<BoxedValue> getter, Action<BoxedValue> setter, bool isReadOnly)
            : base(label, typeInfo, getter, setter, isReadOnly, 3,
                  (v, i) => i switch
                  {
                      0 => v.x,
                      1 => v.y,
                      2 => v.z,
                      _ => 0f
                  },
                  (v, i, val) => i switch
                  {
                      0 => new Vec3f(val, v.y, v.z),
                      1 => new Vec3f(v.x, val, v.z),
                      2 => new Vec3f(v.x, v.y, val),
                      _ => v
                  })
        {
        }
    }
}