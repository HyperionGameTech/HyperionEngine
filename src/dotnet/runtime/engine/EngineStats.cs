using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "EngineStats")]
    public class EngineStats : ObjectBase
    {
        private static EngineStats? _instance = null;

        public static EngineStats Instance
        {
            get
            {
                if (_instance == null)
                {
                    using (HypDataBuffer resultData = ObjectBase.GetMethod(Class.GetClass(typeof(EngineStats)), new Name("GetInstance", weak: true)).InvokeNative())
                    {
                        _instance = (EngineStats)resultData.GetValue();

                        if (_instance == null)
                        {
                            throw new Exception("Failed to get EngineStats instance");
                        }
                    }
                }

                return _instance;
            }
        }

        public EngineStats()
        {
        }
    }
}