using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="Mesh")]
    public class Mesh : HypObject
    {
        public Mesh()
        {
        }

        public IDBase ID
        {
            get
            {
                return (IDBase)GetProperty(PropertyNames.ID)
                    .InvokeGetter(this)
                    .GetValue();
            }
        }

        public uint NumIndices
        {
            get
            {
                return (uint)GetProperty(PropertyNames.NumIndices)
                    .InvokeGetter(this)
                    .GetValue();
            }
        }

        public BoundingBox AABB
        {
            get
            {
                return (BoundingBox)GetProperty(PropertyNames.AABB)
                    .InvokeGetter(this)
                    .GetValue();
            }
            set
            {
                GetProperty(PropertyNames.AABB)
                    .InvokeSetter(this, new HypData(value));
            }
        }
    }
}