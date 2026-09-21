using System;
using System.Globalization;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class Vec3iViewModel : VectorPropertyViewModelBase<Vec3i>
    {
        public Vec3iViewModel(ObjectBase target, Property property, bool isReadOnly)
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
                      0 => new Vec3i((int)val, v.y, v.z),
                      1 => new Vec3i(v.x, (int)val, v.z),
                      2 => new Vec3i(v.x, v.y, (int)val),
                      _ => v
                  })
        {
        }

        public Vec3iViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly)
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
                      0 => new Vec3i((int)val, v.y, v.z),
                      1 => new Vec3i(v.x, (int)val, v.z),
                      2 => new Vec3i(v.x, v.y, (int)val),
                      _ => v
                  })
        {
        }

        // For a vector within a larger value (eg. transform subcomponents)
        public Vec3iViewModel(ObjectBase target, Property property, bool isReadOnly,
            Func<BoxedValue, Vec3i> extract, Func<BoxedValue, Vec3i, object?> inject)
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
                      0 => new Vec3i((int)val, v.y, v.z),
                      1 => new Vec3i(v.x, (int)val, v.z),
                      2 => new Vec3i(v.x, v.y, (int)val),
                      _ => v
                  },
                  extract,
                  inject)
        {
        }

        // For a vector within a larger value, with component targets
        public Vec3iViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly,
            Func<BoxedValue, Vec3i> extract, Func<BoxedValue, Vec3i, object?> inject)
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
                      0 => new Vec3i((int)val, v.y, v.z),
                      1 => new Vec3i(v.x, (int)val, v.z),
                      2 => new Vec3i(v.x, v.y, (int)val),
                      _ => v
                  },
                  extract,
                  inject)
        {
        }

        public Vec3iViewModel(string label, TypeInfo typeInfo, Func<BoxedValue> getter, Action<BoxedValue> setter, bool isReadOnly)
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
                      0 => new Vec3i((int)val, v.y, v.z),
                      1 => new Vec3i(v.x, (int)val, v.z),
                      2 => new Vec3i(v.x, v.y, (int)val),
                      _ => v
                  })
        {
        }

        protected override string FormatComponent(float value)
        {
            return ((int)value).ToString(CultureInfo.InvariantCulture);
        }

        protected override bool TryParseComponent(string text, out float result)
        {
            if (int.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out int intResult))
            {
                result = intResult;
                return true;
            }

            result = 0;
            return false;
        }
    }
}
