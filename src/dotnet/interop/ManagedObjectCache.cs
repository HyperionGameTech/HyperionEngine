using System;
using System.Runtime.InteropServices;
using System.Reflection;
using System.Collections.Generic;
using System.Collections.Concurrent;
using System.Runtime.CompilerServices;

namespace Hyperion
{
    internal class StoredManagedObject : IDisposable
    {
        public Guid guid;
        public Guid assemblyGuid;
        public GCHandle gcHandle;
        public GCHandle? gcHandleStrong;
        public Type objectType;

        public StoredManagedObject(Guid objectGuid, Guid assemblyGuid, object obj, GCHandle? gcHandleStrong)
        {
            if (obj == null)
                throw new ArgumentNullException(nameof(obj), "Object cannot be null");

            this.guid = objectGuid;
            this.assemblyGuid = assemblyGuid;

            // Create initial weak handle
            this.gcHandle = GCHandle.Alloc(obj, GCHandleType.Weak);
            this.gcHandleStrong = gcHandleStrong;

            if (this.gcHandleStrong != null && !this.gcHandleStrong.Value.IsAllocated)
            {
                this.gcHandleStrong = null;
            }

            this.objectType = obj.GetType();
        }

        public void Dispose()
        {
            gcHandle.Free();

            gcHandleStrong?.Free();
            gcHandleStrong = null;
        }

        public bool MakeStrongReference()
        {
            Assert.Throw(gcHandle.IsAllocated, "GCHandle is not allocated");

            if (gcHandleStrong != null)
            {
#if DEBUG
                // debugging
                Assert.Throw(gcHandleStrong.Value.IsAllocated, "GCHandle is not allocated");
                Assert.Throw(gcHandleStrong.Value.Target != null, "GCHandle target is null");
#endif

                return true;
            }

            object? obj = gcHandle.Target;

            if (obj == null)
                return false;

            gcHandleStrong = GCHandle.Alloc((object)obj, GCHandleType.Normal);

#if DEBUG
            // debugging
            Assert.Throw(gcHandleStrong.Value.IsAllocated, "GCHandle is not allocated");
            Assert.Throw(gcHandleStrong.Value.Target != null, "GCHandle target is null");
#endif

            Logger.Log(LogType.Debug, "Strong reference created for guid \"" + guid + "\" (Object type: " + objectType.Name + ")");

            return true;
        }

        public bool MakeWeakReference()
        {
            Assert.Throw(gcHandle.IsAllocated, "GCHandle is not allocated");

            if (gcHandleStrong == null)
                return true;

            gcHandleStrong?.Free();
            gcHandleStrong = null;

            Logger.Log(LogType.Debug, "Weak reference created for guid \"" + guid + "\" (Object type: " + objectType.Name + ")");

            return true;
        }

        public object? Value
        {
            get
            {
                return gcHandle.Target;
            }
        }

        public Type ObjectType
        {
            get
            {
                return objectType;
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

        public ObjectReference AddObject(Guid assemblyGuid, Guid objectGuid, object obj, GCHandle? gcHandle)
        {
            if (obj == null)
            {
                throw new ArgumentNullException(nameof(obj), "Object cannot be null");
            }

            StoredManagedObject storedObject = new StoredManagedObject(objectGuid, assemblyGuid, obj, gcHandle);

            lock (lockObject)
            {
                if (objects.ContainsKey(objectGuid))
                {
                    throw new Exception("Failed to add object, guid \"" + objectGuid + "\" already exists in cache!");
                }

                objects.Add(objectGuid, storedObject);
            }
            
            return storedObject.ToObjectReference();
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
                StoredManagedObject? storedObject = null;

                if (!objects.TryGetValue(guid, out storedObject))
                {
                    throw new Exception("Failed to make strong reference for guid \"" + guid + "\", not found in cache!");
                }
                
                Assert.Throw(storedObject != null, "Stored object is null");

                bool result = ((StoredManagedObject)storedObject).MakeStrongReference();

                if (!result)
                {
                    Logger.Log(LogType.Warn, "Failed to make strong reference for guid \"" + guid + "\", weak reference target returned null! (Object type: " + ((StoredManagedObject)storedObject).ObjectType.Name + ")");

                    ((StoredManagedObject)storedObject).Dispose();
                    objects.Remove(guid);
                }

                return result;
            }
        }

        public bool MakeWeakReference(Guid guid)
        {
            lock (lockObject)
            {
                if (!objects.ContainsKey(guid))
                {
                    throw new Exception("Failed to make weak reference for guid \"" + guid + "\", not found in cache!");
                }

                return objects[guid].MakeWeakReference();
            }
        }

        public int RemoveForAssembly(Guid assemblyGuid)
        {
            Logger.Log(LogType.Debug, "Removing objects for assembly \"" + assemblyGuid + "\"");

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