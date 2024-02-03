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
        public object obj;
        public GCHandle gcHandle;

        public StoredManagedObject(Guid guid, object obj)
        {
            this.guid = guid;
            this.obj = obj;
            this.gcHandle = GCHandle.Alloc(this.obj);
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

        public ManagedObject AddObject(object obj)
        {
            lock (lockObject)
            {
                Guid guid = Guid.NewGuid();

                StoredManagedObject storedObject = new StoredManagedObject(guid, obj);

                objects.Add(guid, storedObject);

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

        public void RemoveObject(Guid guid)
        {
            lock (lockObject)
            {
                if (objects.ContainsKey(guid))
                {
                    objects[guid].Dispose();
                    objects.Remove(guid);
                }
            }
        }
    }
}