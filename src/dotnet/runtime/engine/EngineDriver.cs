using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "EngineDriver")]
    public class EngineDriver : ObjectBase
    {
        private static EngineDriver? _instance = null;

        public static EngineDriver Instance
        {
            get
            {
                if (_instance == null)
                {
                    using (HypDataBuffer resultData = ObjectBase.GetMethod(Class.GetClass(typeof(EngineDriver)), new Name("GetInstance", weak: true)).InvokeNative())
                    {
                        _instance = (EngineDriver)resultData.GetValue();

                        if (_instance == null)
                        {
                            throw new Exception("Failed to get EngineDriver instance");
                        }
                    }
                }

                return _instance;
            }
        }

        public EngineDriver()
        {
        }

        [MainThreadOnly]
        public Game? GameInstance
        {
            get => this.GetGameInstance();      // extension method
            set => this.SetGameInstance(value); // extension method
        }
    }
}