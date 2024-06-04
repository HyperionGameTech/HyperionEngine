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
        public Guid assemblyGuid;
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

        public void AddMethod(Guid assemblyGuid, Guid methodGuid, MethodInfo methodInfo)
        {
            if (methodCache.ContainsKey(methodGuid))
            {
                if (methodCache[methodGuid].assemblyGuid == assemblyGuid)
                {
                    return;
                }

                throw new Exception("Method already exists in cache for a different assembly!");
            }

            methodCache.Add(methodGuid, new StoredManagedMethod
            {
                methodInfo = methodInfo,
                assemblyGuid = assemblyGuid
            });
        }

        public int RemoveMethodsForAssembly(Guid assemblyGuid)
        {
            List<Guid> keysToRemove = new List<Guid>();

            foreach (KeyValuePair<Guid, StoredManagedMethod> kvp in methodCache)
            {
                if (kvp.Value.assemblyGuid == assemblyGuid)
                {
                    keysToRemove.Add(kvp.Key);
                }
            }

            int numKeysToRemove = keysToRemove.Count;

            foreach (Guid key in keysToRemove)
            {
                methodCache.Remove(key);
            }

            return numKeysToRemove;
        }
    }
}