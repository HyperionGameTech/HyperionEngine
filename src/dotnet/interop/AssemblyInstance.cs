using System;
using System.Reflection;
using System.Runtime.Loader;

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
            return LoadGlobalAssembly(AssemblyName.GetAssemblyName(path));
        }

        public static Assembly LoadGlobalAssembly(AssemblyName name)
        {
            Assembly? assembly = FindGlobalAssembly(name);

            if (assembly != null)
            {
                return assembly;
            }

            // Load into the default context

            assembly = Assembly.Load(name);

            if (assembly == null)
            {
                throw new Exception($"Failed to load assembly {name.Name} into default context");
            }

            Logger.Log(LogType.Info, "Loaded assembly {0} (version: {1}) into default context", name.Name, name.Version);

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
                Logger.Log(LogType.Info, "Loaded assembly {0} (version: {1}) from global cached assemblies", name.Name, name.Version);

                return globalAssembly;
            }
            
            Logger.Log(LogType.Info, "Loading assembly: {0} (version: {1})", name.Name, name.Version);

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
        private string basePath;
        private Guid guid;
        private AssemblyName? assemblyName;
        private string? assemblyPath;
        private IntPtr classHolderPtr;
        private AssemblyLoadContext? context;
        private bool ownsContext;
        private Assembly? assembly;
        private bool isCoreAssembly;
        private List<AssemblyInstance> referencedAssemblies;

        public AssemblyInstance(string basePath, Guid guid, string path, IntPtr classHolderPtr, bool isCoreAssembly)
        {
            this.basePath = basePath;
            this.ownsContext = true;
            this.guid = guid;
            this.assemblyPath = path;
            this.classHolderPtr = classHolderPtr;
            this.ownsContext = true;
            this.isCoreAssembly = isCoreAssembly;
            this.referencedAssemblies = new List<AssemblyInstance>();
        }

        public AssemblyInstance(string basePath, AssemblyLoadContext context, Guid guid, string path, IntPtr classHolderPtr, bool isCoreAssembly)
        {
            this.basePath = basePath;
            this.guid = guid;
            this.assemblyPath = path;
            this.classHolderPtr = classHolderPtr;
            this.context = context;
            this.ownsContext = context != null;
            this.isCoreAssembly = isCoreAssembly;
            this.referencedAssemblies = new List<AssemblyInstance>();
        }

        public AssemblyInstance(string basePath, AssemblyLoadContext context, Guid guid, AssemblyName assemblyName, IntPtr classHolderPtr, bool isCoreAssembly)
        {
            this.basePath = basePath;
            this.guid = guid;
            this.assemblyName = assemblyName;
            this.classHolderPtr = classHolderPtr;
            this.context = context;
            this.ownsContext = context != null;
            this.isCoreAssembly = isCoreAssembly;
            this.referencedAssemblies = new List<AssemblyInstance>();
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

        public IntPtr ClassHolderPtr
        {
            get
            {
                return classHolderPtr;
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
                    assembly = GlobalAssemblyHelper.LoadGlobalAssembly((AssemblyName)assemblyName);
                }
                else
                {
                    assembly = GlobalAssemblyHelper.LoadGlobalAssembly((string)assemblyPath);
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
                    assembly = context.LoadFromAssemblyName((AssemblyName)assemblyName);
                }
                else
                {
                    assembly = context.LoadFromAssemblyPath((string)assemblyPath);
                }

                // Load all referenced assemblies to ensure nothing will crash later
                Logger.Log(LogType.Info, "Loaded assembly: {0}, with {1} referenced assemblies.", assembly.FullName, assembly.GetReferencedAssemblies().Length);
            }

            foreach (AssemblyName referencedAssemblyName in assembly.GetReferencedAssemblies())
            {
                // Logger.Log(LogType.Info, "Found referenced assembly for {0}: {1} (version: {2})", assembly.GetName().Name, assembly.GetReferencedAssemblies()[i].Name, assembly.GetReferencedAssemblies()[i].Version);

                // context.LoadFromAssemblyName(assembly.GetReferencedAssemblies()[i]);

                AssemblyInstance? referencedAssembly = AssemblyCache.Instance.Get(referencedAssemblyName);

                if (referencedAssembly == null)
                {
                    Logger.Log(LogType.Info, "Loading referenced assembly: {0} (version: {1})", referencedAssemblyName.Name, referencedAssemblyName.Version);

                    referencedAssembly = new AssemblyInstance(basePath, context, Guid.NewGuid(), referencedAssemblyName, classHolderPtr, isCoreAssembly);
                    referencedAssembly.Load();

                    AssemblyCache.Instance.Add(referencedAssembly);
                }

                referencedAssemblies.Add(referencedAssembly);
            }

// #if DEBUG
//             Logger.Log(LogType.Info, "Loaded core assembly: {0} (version: {1}) into default context", assembly.GetName().Name, assembly.GetName().Version);

//             for (int i = 0; i < assembly.GetReferencedAssemblies().Length; i++)
//             {
//                 Logger.Log(LogType.Info, "Found referenced assembly for {0}: {1} (version: {2})", assembly.GetName().Name, assembly.GetReferencedAssemblies()[i].Name, assembly.GetReferencedAssemblies()[i].Version);

//                 GlobalAssemblyHelper.LoadGlobalAssembly(assembly.GetReferencedAssemblies()[i]);
                
//                 Logger.Log(LogType.Info, "Loaded referenced assembly for {0}: {1} (version: {2})", assembly.GetName().Name, assembly.GetReferencedAssemblies()[i].Name, assembly.GetReferencedAssemblies()[i].Version);
//             }
// #endif
        }

        public void Unload()
        {
            Logger.Log(LogType.Info, $"Attempting to unload assembly {assembly?.FullName}");

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

                Logger.Log(LogType.Info, $"Referenced assembly unloading: {referencedAssembly.AssemblyName?.FullName}");

                referencedAssembly.Unload();
            }

            int numObjectsRemoved = ManagedObjectCache.Instance.RemoveObjectsForAssembly(guid);

            Dictionary<Type, int> numCachedObjectsRemoved = new Dictionary<Type, int>();

            foreach (KeyValuePair<Type, IBasicCache> kvp in BasicCacheInstanceManager.Instance.GetInstances())
            {
                int numRemoved = kvp.Value.RemoveForAssembly(guid);

                numCachedObjectsRemoved.Add(kvp.Key, numRemoved);
            }

            Logger.Log(LogType.Info, $"Unloaded assembly {guid}, removed {numObjectsRemoved} objects.");

            foreach (KeyValuePair<Type, int> kvp in numCachedObjectsRemoved)
            {
                Logger.Log(LogType.Info, $"\tRemoved {kvp.Value} {kvp.Key.Name} objects.");
            }

            if (ownsContext)
            {
                Logger.Log(LogType.Info, "Unloading context");
                context.Unload();
                Logger.Log(LogType.Info, "Context unloaded");
            }

            assembly = null;
            context = null;
            ownsContext = false;
        }
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

        public Dictionary<Guid, AssemblyInstance> Assemblies
        {
            get
            {
                return assemblies;
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
            if (assemblies.ContainsKey(guid))
            {
                return assemblies[guid];
            }

            return null;
        }

        public AssemblyInstance? Get(string path)
        {
            foreach (KeyValuePair<Guid, AssemblyInstance> kvp in assemblies)
            {
                if (kvp.Value.AssemblyPath == path)
                {
                    return kvp.Value;
                }
            }

            return null;
        }

        public AssemblyInstance? Get(Assembly assembly)
        {
            foreach (KeyValuePair<Guid, AssemblyInstance> kvp in assemblies)
            {
                if (kvp.Value.Assembly == assembly)
                {
                    return kvp.Value;
                }
            }

            return null;
        }

        public AssemblyInstance? Get(AssemblyName assemblyName)
        {
            foreach (KeyValuePair<Guid, AssemblyInstance> kvp in assemblies)
            {
                if (kvp.Value.AssemblyName?.FullName == assemblyName.FullName)
                {
                    return kvp.Value;
                }
            }

            return null;
        }

        public AssemblyInstance Add(Guid guid, string path, IntPtr classHolderPtr, bool isCoreAssembly = false)
        {
            if (assemblies.ContainsKey(guid))
            {
                throw new Exception("Assembly already exists in cache");
            }

            string basePath = System.IO.Path.GetDirectoryName(path);

            AssemblyInstance assemblyInstance = new AssemblyInstance(basePath, guid, path, classHolderPtr, isCoreAssembly);
            assemblyInstance.Load();

            assemblies.Add(guid, assemblyInstance);

            Logger.Log(LogType.Info, $"Added assembly {guid} from {path}");

            return assemblyInstance;
        }

        public void Add(AssemblyInstance assemblyInstance)
        {
            if (assemblies.ContainsKey(assemblyInstance.Guid))
            {
                throw new Exception("Assembly already exists in cache");
            }

            assemblies.Add(assemblyInstance.Guid, assemblyInstance);
        }

        public void Remove(Guid guid)
        {
            if (!assemblies.ContainsKey(guid))
            {
                throw new Exception("Assembly does not exist in cache");
            }

            assemblies[guid].Unload();
            assemblies.Remove(guid);

            Logger.Log(LogType.Info, $"Removed assembly {guid}");
        }
    }
}