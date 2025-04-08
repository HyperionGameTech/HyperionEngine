using System;
using System.Runtime.InteropServices;
using System.Reflection;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

namespace Hyperion
{
    internal struct StoredManagedMethod
    {
        public Guid AssemblyGuid { get; set; }
        public MethodInfo MethodInfo { get; set; }
        public InvokeMethodDelegate InvokeMethodDelegate { get; set; }
        public GCHandle DelegateGCHandle { get; set; }
    }

    public sealed class ManagedMethodCache : IBasicCache
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

                return (ManagedMethodCache)instance;
            }
        }
        
        private Dictionary<Guid, StoredManagedMethod> cache = new Dictionary<Guid, StoredManagedMethod>();
        private object lockObject = new object();

        public MethodInfo Get(Guid guid)
        {
            lock (lockObject)
            {
                if (cache.ContainsKey(guid))
                {
                    return cache[guid].MethodInfo;
                }
            }

            return default(MethodInfo);
        }

        public void Add(Guid assemblyGuid, Guid guid, MethodInfo methodInfo, InvokeMethodDelegate invokeMethodDelegate)
        {
            lock (lockObject)
            {
                if (cache.ContainsKey(guid))
                {
                    throw new Exception("Method already exists in cache");
                }

                cache.Add(guid, new StoredManagedMethod {
                    AssemblyGuid = assemblyGuid, 
                    MethodInfo = methodInfo,
                    InvokeMethodDelegate = invokeMethodDelegate,
                    DelegateGCHandle = GCHandle.Alloc(invokeMethodDelegate)
                });
            }
        }

        public int RemoveForAssembly(Guid assemblyGuid)
        {
            Console.WriteLine("Removing methods for assembly: " + assemblyGuid);
            Console.Out.Flush();

            List<Guid> keysToRemove = new List<Guid>();
            int numKeysToRemove = 0;

            lock (lockObject)
            {
                foreach (KeyValuePair<Guid, StoredManagedMethod> kvp in cache)
                {
                    if (kvp.Value.AssemblyGuid == assemblyGuid)
                    {
                        keysToRemove.Add(kvp.Key);

                        kvp.Value.DelegateGCHandle.Free();
                    }
                }

                numKeysToRemove = keysToRemove.Count;

                foreach (Guid key in keysToRemove)
                {
                    cache.Remove(key);
                }
            }

            return numKeysToRemove;
        }
    }
}