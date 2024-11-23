using System;
using System.Runtime.InteropServices;
using System.Reflection;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

namespace Hyperion
{
    public struct StoredCachedObject<T>
    {
        public Guid AssemblyGuid { get; set; }
        public T Value { get; set; }
    }

    public interface IBasicCache
    {
        public int RemoveForAssembly(Guid assemblyGuid);
    }

    internal class BasicCacheInstanceManager
    {
        private static BasicCacheInstanceManager? instance = null;

        public static BasicCacheInstanceManager Instance
        {
            get
            {
                if (instance == null)
                {
                    instance = new BasicCacheInstanceManager();
                }

                return instance;
            }
        }

        private Dictionary<Type, IBasicCache> instances = new Dictionary<Type, IBasicCache>();
        private object lockObject = new object();

        public void AddInstance<T>(BasicCache<T> instance)
        {
            Type type = typeof(T);

            lock (lockObject)
            {
                if (instances.ContainsKey(type))
                {
                    throw new Exception("Instance already exists for type!");
                }

                instances.Add(type, instance);
            }
        }

        public BasicCache<T>? GetInstance<T>()
        {
            Type type = typeof(T);

            lock (lockObject)
            {
                if (instances.ContainsKey(type))
                {
                    return (BasicCache<T>)instances[type];
                }
            }

            return null;
        }

        public IEnumerable<KeyValuePair<Type, IBasicCache>> GetInstances()
        {
            lock (lockObject)
            {
                foreach (KeyValuePair<Type, IBasicCache> kvp in instances)
                {
                    yield return kvp;
                }
            }
        }
    }

    public sealed class BasicCache<T> : IBasicCache
    {
        private static BasicCache<T>? instance = null;

        public static BasicCache<T> Instance
        {
            get
            {
                if (instance == null)
                {
                    instance = new BasicCache<T>();

                    BasicCacheInstanceManager.Instance.AddInstance<T>(instance);
                }

                return instance;
            }
        }
        
        private Dictionary<Guid, StoredCachedObject<T>> cache = new Dictionary<Guid, StoredCachedObject<T>>();
        private object lockObject = new object();

        public T Get(Guid guid)
        {
            lock (lockObject)
            {
                if (cache.ContainsKey(guid))
                {
                    return cache[guid].Value;
                }
            }

            return default(T);
        }

        public void Add(Guid assemblyGuid, Guid guid, T value)
        {
            lock (lockObject)
            {
                if (cache.ContainsKey(guid))
                {
                    if (cache[guid].AssemblyGuid == assemblyGuid)
                    {
                        return;
                    }

                    throw new Exception("Object already exists in cache for a different assembly!");
                }

                cache.Add(guid, new StoredCachedObject<T> { AssemblyGuid = assemblyGuid, Value = value });
            }
        }

        public int RemoveForAssembly(Guid assemblyGuid)
        {
            List<Guid> keysToRemove = new List<Guid>();
            int numKeysToRemove = 0;

            lock (lockObject)
            {
                foreach (KeyValuePair<Guid, StoredCachedObject<T>> kvp in cache)
                {
                    if (kvp.Value.AssemblyGuid == assemblyGuid)
                    {
                        keysToRemove.Add(kvp.Key);
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