using System;
using System.Runtime.InteropServices;
using System.Reflection;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

namespace Hyperion
{
    internal class StoredManagedObject : IDisposable
    {
        public Guid guid;
        public Guid assemblyGuid;
        public object obj;
        public GCHandle gcHandle;

        public StoredManagedObject(Guid objectGuid, Guid assemblyGuid, object obj, bool keepAlive)
        {
            this.guid = objectGuid;
            this.assemblyGuid = assemblyGuid;
            this.obj = obj;
            this.gcHandle = GCHandle.Alloc(this.obj, keepAlive ? GCHandleType.Normal : GCHandleType.Weak);
        }

        public void Dispose()
        {
            gcHandle.Free();
        }

        public ManagedObject ToManagedObject()
        {
            return new ManagedObject
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

        public ManagedObject AddObject(Guid assemblyGuid, Guid objectGuid, object obj, bool keepAlive)
        {
            Logger.Log(LogType.Debug, $"Adding object {obj} to cache for assembly {assemblyGuid}");

            lock (lockObject)
            {
                StoredManagedObject storedObject = new StoredManagedObject(objectGuid, assemblyGuid, obj, keepAlive);

                objects.Add(objectGuid, storedObject);

                return storedObject.ToManagedObject();
            }
        }

        public StoredManagedObject? GetObject(Guid guid)
        {
            lock (lockObject)
            {
                if (objects.ContainsKey(guid))
                {
                    return objects[guid];
                }

                return null;
            }
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
                    Logger.Log(LogType.Debug, $"Checking object {entry.Key} for assembly {assemblyGuid}. Object assembly: {entry.Value.assemblyGuid}");

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