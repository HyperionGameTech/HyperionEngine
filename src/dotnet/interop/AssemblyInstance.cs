using System;
using System.Reflection;
using System.Runtime.Loader;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace Hyperion
{
    internal class GlobalAssemblyHelper
    {
        public static Assembly? FindGlobalAssembly(AssemblyName name)
        {
            foreach (Assembly assembly in AppDomain.CurrentDomain.GetAssemblies())
            {
                if (assembly.GetName().Name == name.Name)
                {
                    return assembly;
                }
            }

            return null;
        }

        public static Assembly LoadGlobalAssembly(string path)
        {
            Logger.Log(LogType.Debug, "Loading global assembly from path: " + path);

            AssemblyName assemblyName = AssemblyName.GetAssemblyName(path);
            Assembly? assembly = FindGlobalAssembly(assemblyName);

            if (assembly != null)
            {
                return assembly;
            }

            assembly = Assembly.LoadFrom(path);

            if (assembly == null)
            {
                throw new Exception($"Failed to load assembly {assemblyName.Name} into default context");
            }

            Logger.Log(LogType.Debug, "Loaded assembly {0} (version: {1}) into default context", assemblyName.Name, assemblyName.Version);

            return assembly;
        }

        public static Assembly LoadGlobalAssembly(AssemblyName name)
        {
            Assembly? assembly = FindGlobalAssembly(name);

            if (assembly != null)
            {
                return assembly;
            }

            // Load into the default context
            Logger.Log(LogType.Debug, "Loading global assembly from name: {0} (version: {1})", name.Name, name.Version);

            assembly = Assembly.Load(name);

            if (assembly == null)
            {
                throw new Exception($"Failed to load assembly {name.Name} into default context");
            }

            Logger.Log(LogType.Debug, "Loaded assembly {0} (version: {1}) into default context", name.Name, name.Version);

            return assembly;
        }
    }

    internal class ScriptAssemblyContext : AssemblyLoadContext
    {
        private AssemblyDependencyResolver resolver;

        public ScriptAssemblyContext(string basePath) : base(isCollectible: true)
        {
            resolver = new AssemblyDependencyResolver(basePath);
        }

        protected override Assembly? Load(AssemblyName name)
        {
            // Check if the assembly is already loaded
            //
            // For instance, a script referencing Hyperion.Core or Hyperion.Runtime
            // should use those shared assemblies
            //
            Assembly? globalAssembly = GlobalAssemblyHelper.FindGlobalAssembly(name);

            if (globalAssembly != null)
            {
                Logger.Log(LogType.Debug, "Loaded assembly {0} (version: {1}) from global cached assemblies", name.Name, name.Version);

                return globalAssembly;
            }

            Logger.Log(LogType.Debug, "Loading assembly: {0} (version: {1})", name.Name, name.Version);

            string assemblyPath = resolver.ResolveAssemblyToPath(name);

            if (assemblyPath != null)
            {
                return LoadFromAssemblyPath(assemblyPath);
            }

            return null;
        }
    }

    public class AssemblyInstance
    {
        public static Guid thisAssemblyGuid;

        private string basePath;
        private Guid guid;
        private AssemblyName? assemblyName;
        private string? assemblyPath;
        private IntPtr assemblyPtr;
        private bool ownsAssemblyPtr; // whether we own the assemblyPtr and need to free it
        private AssemblyLoadContext? context;
        private bool ownsContext;
        private Assembly? assembly;
        private bool isCoreAssembly;
        private List<AssemblyInstance> referencedAssemblies;

        public AssemblyInstance(string basePath, Guid guid, string path, IntPtr assemblyPtr, bool ownsAssemblyPtr, bool isCoreAssembly)
        {
            this.basePath = basePath;
            this.ownsContext = true;
            this.guid = guid;
            this.assemblyPath = path;
            this.assemblyPtr = assemblyPtr;
            this.ownsAssemblyPtr = ownsAssemblyPtr;
            this.ownsContext = true;
            this.isCoreAssembly = isCoreAssembly;
            this.referencedAssemblies = new List<AssemblyInstance>();
        }

        public AssemblyInstance(string basePath, AssemblyLoadContext? context, Guid guid, string path, IntPtr assemblyPtr, bool ownsAssemblyPtr, bool isCoreAssembly)
        {
            this.basePath = basePath;
            this.guid = guid;
            this.assemblyPath = path;
            this.assemblyPtr = assemblyPtr;
            this.ownsAssemblyPtr = ownsAssemblyPtr;
            this.context = context;
            this.ownsContext = context != null;
            this.isCoreAssembly = isCoreAssembly;
            this.referencedAssemblies = new List<AssemblyInstance>();
        }

        public AssemblyInstance(string basePath, AssemblyLoadContext? context, Guid guid, AssemblyName assemblyName, IntPtr assemblyPtr, bool ownsAssemblyPtr, bool isCoreAssembly)
        {
            this.basePath = basePath;
            this.guid = guid;
            this.assemblyName = assemblyName;
            this.assemblyPtr = assemblyPtr;
            this.ownsAssemblyPtr = ownsAssemblyPtr;
            this.context = context;
            this.ownsContext = context != null;
            this.isCoreAssembly = isCoreAssembly;
            this.referencedAssemblies = new List<AssemblyInstance>();
        }

        ~AssemblyInstance()
        {
            if (ownsAssemblyPtr && assemblyPtr != IntPtr.Zero)
            {
                NativeInterop_FreeAssembly(assemblyPtr);
                assemblyPtr = IntPtr.Zero;
            }
        }

        public string BasePath
        {
            get
            {
                return basePath;
            }
        }

        public Guid Guid
        {
            get
            {
                return guid;
            }
        }

        public AssemblyName? AssemblyName
        {
            get
            {
                return assembly?.GetName() ?? assemblyName;
            }
        }

        public string? AssemblyPath
        {
            get
            {
                return assemblyPath;
            }
        }

        public IntPtr AssemblyPtr
        {
            get
            {
                return assemblyPtr;
            }
        }

        public Assembly? Assembly
        {
            get
            {
                return assembly;
            }
        }

        public bool IsCoreAssembly
        {
            get
            {
                return isCoreAssembly;
            }
        }

        public List<AssemblyInstance> ReferencedAssemblies
        {
            get
            {
                return referencedAssemblies;
            }
        }

        public void Load()
        {
            if (assembly != null)
            {
                return;
            }

            if (assemblyName == null && assemblyPath == null)
            {
                throw new Exception("Assembly name and path are null");
            }

            if (isCoreAssembly)
            {
                // Load core assemblies into the default (global) context. They won't be unloaded.
                // We only create core assemblies when the application starts, and all referenced assemblies from core assemblies
                // are also core assemblies.

                // When we attempt to load an assembly from a non-core assembly, it will be shared across all assemblies.
                if (assemblyName != null)
                {
                    assembly = GlobalAssemblyHelper.LoadGlobalAssembly(assemblyName);
                }
                else
                {
                    if (assemblyPath == null)
                    {
                        throw new Exception("Assembly path is null for core assembly");
                    }

                    assembly = GlobalAssemblyHelper.LoadGlobalAssembly(assemblyPath);
                }
            }
            else
            {
                if (context == null)
                {
                    context = new ScriptAssemblyContext(basePath);
                    ownsContext = true;
                }

                if (assemblyName != null)
                {
                    assembly = context.LoadFromAssemblyName(assemblyName);
                }
                else
                {
                    if (assemblyPath == null)
                    {
                        throw new Exception("Assembly path is null");
                    }

                    assembly = context.LoadFromAssemblyPath(assemblyPath);
                }

                // Load all referenced assemblies to ensure nothing will crash later
                Logger.Log(LogType.Debug, "Loaded assembly: {0}, with {1} referenced assemblies.", assembly.FullName, assembly.GetReferencedAssemblies().Length);
            }

            // Load referenced assemblies
            foreach (AssemblyName referencedAssemblyName in assembly.GetReferencedAssemblies())
            {
                AssemblyInstance? referencedAssembly = AssemblyCache.Instance.Get(referencedAssemblyName);

                if (referencedAssembly == null)
                {
                    Guid assemblyGuid = Guid.NewGuid();

                    Logger.Log(LogType.Debug, "Loading referenced assembly: {0} (version: {1})", referencedAssemblyName.Name, referencedAssemblyName.Version);

                    IntPtr assemblyPtr = IntPtr.Zero;
                    int res = NativeInterop_NewAssembly(assemblyGuid, out assemblyPtr);

                    if (res != (int)LoadAssemblyResult.Ok)
                    {
                        throw new Exception("Failed to create new assembly for referenced assembly " + referencedAssemblyName.Name + ". Error code: " + (LoadAssemblyResult)res);
                    }

                    if (assemblyPtr == IntPtr.Zero)
                    {
                        throw new Exception("NativeInterop_NewAssembly returned null assembly pointer for referenced assembly " + referencedAssemblyName.Name);
                    }

                    referencedAssembly = new AssemblyInstance(
                        basePath: basePath,
                        context: context,
                        guid: assemblyGuid,
                        assemblyName: referencedAssemblyName,
                        assemblyPtr: assemblyPtr,
                        ownsAssemblyPtr: true,
                        isCoreAssembly: isCoreAssembly);

                    referencedAssembly.Load();

                    AssemblyCache.Instance.Add(referencedAssembly);
                }

                referencedAssemblies.Add(referencedAssembly);
            }
        }

        public void Unload()
        {
            Logger.Log(LogType.Debug, $"Attempting to unload assembly {assembly?.FullName}");

            if (assembly == null)
            {
                return;
            }

            if (context == null)
            {
                throw new Exception($"Cannot unload assembly with GUID {guid} ({AssemblyName?.FullName?.ToString() ?? AssemblyPath}) because it has no context");
            }

            foreach (AssemblyInstance referencedAssembly in referencedAssemblies)
            {
                if (referencedAssembly.IsCoreAssembly)
                {
                    continue;
                }

                Logger.Log(LogType.Debug, $"Referenced assembly unloading: {referencedAssembly.AssemblyName?.FullName}");

                referencedAssembly.Unload();
            }

            /// \todo Remove DynamicStruct instances for the assembly

            int numMethodsRemoved = ManagedMethodCache.Instance.RemoveForAssembly(guid);
            int numDelegatesRemoved = DelegateCache.Instance.RemoveForAssembly(guid);

            Dictionary<Type, int> numCachedObjectsRemoved = new Dictionary<Type, int>();

            foreach (KeyValuePair<Type, IBasicCache> kvp in BasicCacheInstanceManager.Instance.GetInstances())
            {
                int numRemoved = kvp.Value.RemoveForAssembly(guid);

                numCachedObjectsRemoved.Add(kvp.Key, numRemoved);
            }

            Logger.Log(LogType.Debug, $"Unloaded assembly {guid}, removed:\n\t{numMethodsRemoved} method(s)\n\t{numDelegatesRemoved} delegate(s)");

            foreach (KeyValuePair<Type, int> kvp in numCachedObjectsRemoved)
            {
                Logger.Log(LogType.Debug, $"\t{kvp.Value} {kvp.Key.Name} object(s)");
            }

            if (ownsContext)
            {
                Logger.Log(LogType.Debug, "Unloading context");
                context.Unload();
                Logger.Log(LogType.Debug, "Context unloaded");
            }

            assembly = null;
            context = null;
            ownsContext = false;
        }

        [DllImport("hyperion")]
        private static extern int NativeInterop_NewAssembly(Guid guid, out IntPtr outAssemblyPtr);

        [DllImport("hyperion")]
        private static extern void NativeInterop_FreeAssembly(IntPtr assemblyPtr);
    }

    public class AssemblyCache
    {
        private static AssemblyCache? instance = null;

        public static AssemblyCache Instance
        {
            get
            {
                if (instance == null)
                {
                    instance = new AssemblyCache();
                }

                return instance;
            }
        }

        private Dictionary<Guid, AssemblyInstance> assemblies = new Dictionary<Guid, AssemblyInstance>();
        private object lockObject = new object();

        public int Count
        {
            get
            {
                lock (lockObject)
                {
                    return assemblies.Count;
                }
            }
        }

        public IEnumerable<KeyValuePair<Guid, AssemblyInstance>> GetAssemblies()
        {
            lock (lockObject)
            {
                foreach (KeyValuePair<Guid, AssemblyInstance> kvp in assemblies)
                {
                    yield return kvp;
                }
            }
        }

        public AssemblyInstance? GetCurrentAssemblyInstance()
        {
            Assembly? assembly = Assembly.GetCallingAssembly();

            if (assembly == null)
            {
                return null;
            }

            return Get(assembly);
        }

        public AssemblyInstance? Get(Guid guid)
        {
            lock (lockObject)
            {
                if (assemblies.ContainsKey(guid))
                {
                    return assemblies[guid];
                }
            }

            return null;
        }

        public AssemblyInstance? Get(string path)
        {
            lock (lockObject)
            {
                foreach (KeyValuePair<Guid, AssemblyInstance> kvp in assemblies)
                {
                    if (kvp.Value.AssemblyPath == path)
                    {
                        return kvp.Value;
                    }
                }
            }

            return null;
        }

        public AssemblyInstance? Get(Assembly assembly)
        {
            lock (lockObject)
            {
                foreach (KeyValuePair<Guid, AssemblyInstance> kvp in assemblies)
                {
                    if (kvp.Value.Assembly == assembly)
                    {
                        return kvp.Value;
                    }
                }
            }

            return null;
        }

        public AssemblyInstance? Get(AssemblyName assemblyName)
        {
            lock (lockObject)
            {
                foreach (KeyValuePair<Guid, AssemblyInstance> kvp in assemblies)
                {
                    if (kvp.Value.AssemblyName?.FullName == assemblyName.FullName)
                    {
                        return kvp.Value;
                    }
                }
            }

            return null;
        }

        public AssemblyInstance Add(Guid guid, string path, IntPtr assemblyPtr, bool ownsAssemblyPtr = false, bool isCoreAssembly = false)
        {
            lock (lockObject)
            {
                if (assemblies.ContainsKey(guid))
                {
                    throw new Exception("Assembly already exists in cache");
                }

                string? basePath = System.IO.Path.GetDirectoryName(path);

                if (basePath == null)
                {
                    throw new Exception("Failed to get base path from assembly path: " + path);
                }

                AssemblyInstance assemblyInstance = new AssemblyInstance(
                    basePath: basePath,
                    guid: guid,
                    path: path,
                    assemblyPtr: assemblyPtr,
                    ownsAssemblyPtr: ownsAssemblyPtr,
                    isCoreAssembly: isCoreAssembly);

                assemblyInstance.Load();

                assemblies.Add(guid, assemblyInstance);

                return assemblyInstance;
            }
        }

        public void Add(AssemblyInstance assemblyInstance)
        {
            lock (lockObject)
            {
                if (assemblies.ContainsKey(assemblyInstance.Guid))
                {
                    throw new Exception("Assembly already exists in cache");
                }

                assemblies.Add(assemblyInstance.Guid, assemblyInstance);
            }
        }

        public void Remove(Guid guid)
        {
            lock (lockObject)
            {
                if (assemblies.ContainsKey(guid))
                {
                    assemblies[guid].Unload();
                    assemblies.Remove(guid);

                    Logger.Log(LogType.Debug, $"Removed assembly {guid}");
                }
            }
        }
    }
}