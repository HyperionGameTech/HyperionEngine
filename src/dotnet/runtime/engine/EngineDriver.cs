using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "EngineDriver")]
    public class EngineDriver : ObjectBase
    {
        private static EngineDriver? instance = null;

        public static EngineDriver Instance
        {
            get
            {
                if (instance == null)
                {
                    using (HypDataBuffer resultData = ObjectBase.GetMethod(Class.GetClass(typeof(EngineDriver)), new Name("GetInstance", weak: true)).InvokeNative())
                    {
                        instance = (EngineDriver)resultData.GetValue();
                    }
                }

                return (EngineDriver)instance;
            }
        }

        public EngineDriver()
        {
        }
    }
}