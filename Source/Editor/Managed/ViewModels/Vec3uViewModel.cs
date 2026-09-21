using System;
using System.Globalization;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class Vec3uViewModel : VectorPropertyViewModelBase<Vec3u>
    {
        public Vec3uViewModel(ObjectBase target, Property property, bool isReadOnly)
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
                      0 => new Vec3u((uint)val, v.y, v.z),
                      1 => new Vec3u(v.x, (uint)val, v.z),
                      2 => new Vec3u(v.x, v.y, (uint)val),
                      _ => v
                  })
        {
        }

        public Vec3uViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly)
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
                      0 => new Vec3u((uint)val, v.y, v.z),
                      1 => new Vec3u(v.x, (uint)val, v.z),
                      2 => new Vec3u(v.x, v.y, (uint)val),
                      _ => v
                  })
        {
        }

        // For a vector within a larger value (eg. transform subcomponents)
        public Vec3uViewModel(ObjectBase target, Property property, bool isReadOnly,
            Func<BoxedValue, Vec3u> extract, Func<BoxedValue, Vec3u, object?> inject)
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
                      0 => new Vec3u((uint)val, v.y, v.z),
                      1 => new Vec3u(v.x, (uint)val, v.z),
                      2 => new Vec3u(v.x, v.y, (uint)val),
                      _ => v
                  },
                  extract,
                  inject)
        {
        }

        // For a vector within a larger value, with component targets
        public Vec3uViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly,
            Func<BoxedValue, Vec3u> extract, Func<BoxedValue, Vec3u, object?> inject)
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
                      0 => new Vec3u((uint)val, v.y, v.z),
                      1 => new Vec3u(v.x, (uint)val, v.z),
                      2 => new Vec3u(v.x, v.y, (uint)val),
                      _ => v
                  },
                  extract,
                  inject)
        {
        }

        public Vec3uViewModel(string label, TypeInfo typeInfo, Func<BoxedValue> getter, Action<BoxedValue> setter, bool isReadOnly)
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
                      0 => new Vec3u((uint)val, v.y, v.z),
                      1 => new Vec3u(v.x, (uint)val, v.z),
                      2 => new Vec3u(v.x, v.y, (uint)val),
                      _ => v
                  })
        {
        }

        protected override string FormatComponent(float value)
        {
            return ((uint)value).ToString(CultureInfo.InvariantCulture);
        }

        protected override bool TryParseComponent(string text, out float result)
        {
            if (uint.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out uint uintResult))
            {
                result = uintResult;
                return true;
            }

            result = 0;
            return false;
        }
    }
}
