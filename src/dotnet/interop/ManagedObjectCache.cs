using System;
using System.Runtime.InteropServices;
using System.Reflection;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

namespace Hyperion
{
    internal struct StoredManagedObject : IDisposable
    {
        public Guid guid;
        public Guid assemblyGuid;
        public WeakReference weakReference;
        public GCHandle gcHandle;

        public StoredManagedObject(Guid objectGuid, Guid assemblyGuid, object obj, bool keepAlive, GCHandle? gcHandle)
        {
            this.guid = objectGuid;
            this.assemblyGuid = assemblyGuid;

            if (gcHandle != null)
            {
                this.gcHandle = (GCHandle)gcHandle;
            }
            else
            {
                this.gcHandle = GCHandle.Alloc(obj, keepAlive ? GCHandleType.Normal : GCHandleType.Weak);
            }

            this.weakReference = new WeakReference(obj);
        }

        public void Dispose()
        {
            gcHandle.Free();
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

    internal class ManagedObjectCache
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
                    return objects[guid].weakReference.Target;
                }
            }

            return null;
        }

        public bool RemoveObject(Guid guid)
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

        public int RemoveObjectsForAssembly(Guid assemblyGuid)
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