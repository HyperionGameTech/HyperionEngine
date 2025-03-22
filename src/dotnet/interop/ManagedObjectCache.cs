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
        public GCHandle? gcHandleStrong;

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
                // Create initial weak handle
                this.gcHandle = GCHandle.Alloc(obj, GCHandleType.Weak);
            }

            this.weakReference = new WeakReference(obj);

            // If keepAlive is true, create a strong handle that can be used
            // to ensure the managed object exists as long as the unmanged object does.
            if (keepAlive)
            {
                gcHandleStrong = GCHandle.Alloc(obj, GCHandleType.Normal);
            }
        }

        public void Dispose()
        {
            Console.WriteLine("Disposing object \"" + guid + "\" (type: " + weakReference.Target?.GetType().Name + ")");

            gcHandle.Free();

            gcHandleStrong?.Free();
            gcHandleStrong = null;
        }

        public void MakeStrongReference()
        {
            if (gcHandleStrong != null)
            {
                return;
            }

            object? obj = weakReference.Target;

            if (obj == null)
            {
                throw new Exception("Failed to create strong reference, weak reference target returned null!");
            }

            gcHandleStrong = GCHandle.Alloc(obj, GCHandleType.Normal);
        }

        public void MakeWeakReference()
        {
            gcHandleStrong?.Free();
            gcHandleStrong = null;
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
                    if (objects[guid].Value == null)
                    {
                        throw new Exception("Failed to get object, weak reference target with guid \"" + guid + "\" returned null!");
                    }

                    return objects[guid].Value;
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

        public bool MakeStrongReference(Guid guid)
        {
            lock (lockObject)
            {
                if (objects.ContainsKey(guid))
                {
                    objects[guid].MakeStrongReference();
                    
                    return true;
                }
            }
            
            return false;
        }

        public bool MakeWeakReference(Guid guid)
        {
            lock (lockObject)
            {
                if (objects.ContainsKey(guid))
                {
                    objects[guid].MakeWeakReference();
                    
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