using System;
using System.Runtime.InteropServices;
using System.Reflection;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

namespace Hyperion
{
    internal struct StoredManagedMethod
    {
        public MethodInfo methodInfo;
    }

    internal class ManagedMethodCache
    {
        private static ManagedMethodCache? instance = null;

        public static ManagedMethodCache Instance
        {
            get
            {
                if (instance == null)
                {
                    instance = new ManagedMethodCache();
                }

                return instance;
            }
        }

        private Dictionary<Guid, StoredManagedMethod> methodCache = new Dictionary<Guid, StoredManagedMethod>();

        public MethodInfo GetMethod(Guid guid)
        {
            if (methodCache.ContainsKey(guid))
            {
                return methodCache[guid].methodInfo;
            }

            return null;
        }

        public void AddMethod(Guid guid, MethodInfo methodInfo)
        {
            methodCache.Add(guid, new StoredManagedMethod
            {
                methodInfo = methodInfo
            });
        }
    }
}