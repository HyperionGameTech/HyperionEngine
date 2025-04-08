using System;
using System.Runtime.InteropServices;
using System.Reflection;
using System.Collections.Generic;
using System.Collections.Concurrent;
using System.Runtime.CompilerServices;

namespace Hyperion
{
    internal struct StoredManagedObject : IDisposable
    {
        public Guid guid;
        public Guid assemblyGuid;
        public WeakReference weakReference;
        public GCHandle gcHandle;
        public GCHandle? gcHandleStrong;

        public StoredManagedObject(Guid objectGuid, Guid assemblyGuid, object obj, bool keepAlive, GCHandle? gcHandleStrong)
        {
            // if (obj == null)
            // {
            //     throw new ArgumentNullException(nameof(obj), "Object cannot be null");
            // }

            this.guid = objectGuid;
            this.assemblyGuid = assemblyGuid;

            // Create initial weak handle
            this.gcHandle = GCHandle.Alloc(obj, GCHandleType.Weak);

            // Create initial weak handle
            this.weakReference = new WeakReference(obj);

            // If keepAlive is true, create a strong handle that can be used
            // to ensure the managed object exists as long as the unmanged object does.
            if (gcHandleStrong != null)
            {
                this.gcHandleStrong = (GCHandle)gcHandleStrong;
            }
            else if (keepAlive)
            {
                this.gcHandleStrong = GCHandle.Alloc(obj, GCHandleType.Normal);
            }
        }

        public void Dispose()
        {
            gcHandle.Free();

            gcHandleStrong?.Free();
            gcHandleStrong = null;
        }

        public bool MakeStrongReference()
        {
            if (gcHandleStrong != null)
            {
                return true;
            }

            object? obj = weakReference.Target;

            if (obj == null)
            {
                // throw new Exception("Failed to create strong reference, weak reference target returned null!");

                return false;
            }

            gcHandleStrong = GCHandle.Alloc(obj, GCHandleType.Normal);

            return true;
        }

        public bool MakeWeakReference()
        {
            gcHandleStrong?.Free();
            gcHandleStrong = null;

            return true;
        }

        public object? Value
        {
            get
            {
                return weakReference.Target;
            }
        }

        public ObjectReference ToObjectReference()
        {
            return new ObjectReference
            {
                guid = guid,
                ptr = GCHandle.ToIntPtr(gcHandle)
            };
        }
    }

    internal class ManagedObjectCache : IBasicCache
    {
        private static ManagedObjectCache? instance = null;

        public static ManagedObjectCache Instance
        {
            get
            {
                if (instance == null)
                {
                    instance = new ManagedObjectCache();
                }

                return instance;
            }
        }

        private Dictionary<Guid, StoredManagedObject> objects = new Dictionary<Guid, StoredManagedObject>();
        private object lockObject = new object();

        public int Count
        {
            get
            {
                lock (lockObject)
                {
                    return objects.Count;
                }
            }
        }

        public ObjectReference AddObject(Guid assemblyGuid, Guid objectGuid, object obj, bool keepAlive, GCHandle? gcHandle)
        {
            StoredManagedObject storedObject = new StoredManagedObject(objectGuid, assemblyGuid, obj, keepAlive, gcHandle);

            lock (lockObject)
            {
                objects.Add(objectGuid, storedObject);
            }
            
            return storedObject.ToObjectReference();
        }

        public object? GetObject(Guid guid)
        {
            lock (lockObject)
            {
                if (objects.ContainsKey(guid))
                {
                    object? value = objects[guid].Value;

                    if (value == null)
                    {
                        throw new Exception("Failed to get object, weak reference target with guid \"" + guid + "\" returned null!");
                    }

                    return value;
                }
            }

            return null;
        }

        public bool Remove(Guid guid)
        {
            lock (lockObject)
            {
                if (objects.ContainsKey(guid))
                {
                    objects[guid].Dispose();
                    objects.Remove(guid);
                    
                    return true;
                }
            }
            
            return false;
        }

        public bool MakeStrongReference(Guid guid)
        {
            lock (lockObject)
            {
                StoredManagedObject stored;

                if (!objects.TryGetValue(guid, out stored))
                {
                    throw new Exception("Failed to make strong reference for guid \"" + guid + "\", not found in cache!");
                }

                bool result = objects[guid].MakeStrongReference();

                // Update the stored object in the cache
                objects[guid] = stored;

                if (!result)
                {
                    Logger.Log(LogType.Warn, "Failed to make strong reference for guid \"" + guid + "\", weak reference target returned null!");
                }

                return result;
            }
        }

        public bool MakeWeakReference(Guid guid)
        {
            StoredManagedObject stored;
            lock (lockObject)
            {
                if (!objects.TryGetValue(guid, out stored))
                {
                    throw new Exception("Failed to make weak reference for guid \"" + guid + "\", not found in cache!");
                }
            }

            // NOTE: modify outside of the lock to prevent deadlocks
            bool result = stored.MakeWeakReference();

            lock (lockObject)
            {
                objects[guid] = stored;
            }
            
            return result;
        }

        public int RemoveForAssembly(Guid assemblyGuid)
        {
            lock (lockObject)
            {
                List<Guid> keysToRemove = new List<Guid>();

                foreach (KeyValuePair<Guid, StoredManagedObject> entry in objects)
                {
                    if (entry.Value.assemblyGuid == assemblyGuid)
                    {
                        keysToRemove.Add(entry.Key);
                    }
                }

                foreach (Guid key in keysToRemove)
                {
                    objects[key].Dispose();
                    objects.Remove(key);
                }

                return keysToRemove.Count;
            }
        }
    }
}