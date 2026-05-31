using System;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class Vec4fViewModel : VectorPropertyViewModelBase<Vec4f>
    {
        public Vec4fViewModel(ObjectBase target, Property property, bool isReadOnly)
            : base(target, property, isReadOnly, 4,
                  (v, i) => i switch
                  {
                      0 => v.x,
                      1 => v.y,
                      2 => v.z,
                      3 => v.w,
                      _ => 0f
                  },
                  (v, i, val) => i switch
                  {
                      0 => new Vec4f(val, v.y, v.z, v.w),
                      1 => new Vec4f(v.x, val, v.z, v.w),
                      2 => new Vec4f(v.x, v.y, val, v.w),
                      3 => new Vec4f(v.x, v.y, v.z, val),
                      _ => v
                  })
        {
        }

        public Vec4fViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly)
            : base(classAddress, targetAddressResolver, property, isReadOnly, 4,
                  (v, i) => i switch
                  {
                      0 => v.x,
                      1 => v.y,
                      2 => v.z,
                      3 => v.w,
                      _ => 0f
                  },
                  (v, i, val) => i switch
                  {
                      0 => new Vec4f(val, v.y, v.z, v.w),
                      1 => new Vec4f(v.x, val, v.z, v.w),
                      2 => new Vec4f(v.x, v.y, val, v.w),
                      3 => new Vec4f(v.x, v.y, v.z, val),
                      _ => v
                  })
        {
        }

        public Vec4fViewModel(string label, TypeInfo typeInfo, Func<BoxedValue> getter, Action<BoxedValue> setter, bool isReadOnly)
            : base(label, typeInfo, getter, setter, isReadOnly, 4,
                  (v, i) => i switch
                  {
                      0 => v.x,
                      1 => v.y,
                      2 => v.z,
                      3 => v.w,
                      _ => 0f
                  },
                  (v, i, val) => i switch
                  {
                      0 => new Vec4f(val, v.y, v.z, v.w),
                      1 => new Vec4f(v.x, val, v.z, v.w),
                      2 => new Vec4f(v.x, v.y, val, v.w),
                      3 => new Vec4f(v.x, v.y, v.z, val),
                      _ => v
                  })
        {
        }
    }
}