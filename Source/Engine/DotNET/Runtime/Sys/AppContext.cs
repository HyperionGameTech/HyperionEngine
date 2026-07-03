using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "AppContextBase")]
    public class AppContextBase : ObjectBase
    {
        private static AppContextBase? _instance = null;

        public static AppContextBase Instance
        {
            get
            {
                if (_instance == null)
                {
                    using (BoxedValueInternal resultData = ObjectBase.GetMethod(Class.GetClass(typeof(AppContextBase)), new Name("GetInstance", weak: true)).InvokeNative())
                    {
                        _instance = (AppContextBase)resultData.GetValue();

                        if (_instance == null)
                        {
                            throw new Exception("Failed to get AppContextBase instance");
                        }
                    }
                }

                return _instance;
            }
        }

        public AppContextBase()
        {
        }
    }

    [ClassBinding(Name = "Win32AppContext", Condition = "IsWindows")]
    public class Win32AppContext : AppContextBase
    {
        public Win32AppContext()
        {
        }
    }

    [ClassBinding(Name = "CocoaAppContext", Condition = "IsMacOS")]
    public class CocoaAppContext : AppContextBase
    {
        public CocoaAppContext()
        {
        }
    }

    [ClassBinding(Name = "AndroidAppContext", Condition = "IsAndroid")]
    public class AndroidAppContext : AppContextBase
    {
        public AndroidAppContext()
        {
        }
    }

    [ClassBinding(Name = "IOSAppContext", Condition = "IsIOS")]
    public class IOSAppContext : AppContextBase
    {
        public IOSAppContext()
        {
        }
    }
}
