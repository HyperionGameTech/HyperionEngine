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
                    Logger.Log(LogType.Info, "Found global assembly {0} (version: {1})", name.Name, name.Version);
                    
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

    internal class AssemblyInstance
    {
        private Guid guid;
        private string path;
        private AssemblyLoadContext? context;
        private Assembly? assembly;
        private bool isCoreAssembly;

        public AssemblyInstance(Guid guid, string path, bool isCoreAssembly)
        {
            this.guid = guid;
            this.path = path;
            this.isCoreAssembly = isCoreAssembly;
        }

        public Assembly? Assembly
        {
            get
            {
                return assembly;
            }
        }

        public string AssemblyPath
        {
            get
            {
                return path;
            }
        }

        public Guid Guid
        {
            get
            {
                return guid;
            }
        }

        public bool IsCoreAssembly
        {
            get
            {
                return isCoreAssembly;
            }
        }

        public void Load()
        {
            if (assembly != null)
            {
                return;
            }

            if (!isCoreAssembly)
            {
                context = new ScriptAssemblyContext(path);
            
                assembly = context.LoadFromAssemblyPath(path);

#if DEBUG
                // Load all referenced assemblies to ensure nothing will crash later
                Logger.Log(LogType.Info, "Loaded assembly: {0}, with {1} referenced assemblies.", assembly.FullName, assembly.GetReferencedAssemblies().Length);

                for (int i = 0; i < assembly.GetReferencedAssemblies().Length; i++)
                {
                    Logger.Log(LogType.Info, "Found referenced assembly for {0}: {1} (version: {2})", assembly.GetName().Name, assembly.GetReferencedAssemblies()[i].Name, assembly.GetReferencedAssemblies()[i].Version);

                    context.LoadFromAssemblyName(assembly.GetReferencedAssemblies()[i]);
                }
#endif
                return;
            }

            // Load core assemblies into the default context

            assembly = GlobalAssemblyHelper.LoadGlobalAssembly(path);

#if DEBUG
            Logger.Log(LogType.Info, "Loaded core assembly: {0} (version: {1}) into default context", assembly.GetName().Name, assembly.GetName().Version);

            for (int i = 0; i < assembly.GetReferencedAssemblies().Length; i++)
            {
                Logger.Log(LogType.Info, "Found referenced assembly for {0}: {1} (version: {2})", assembly.GetName().Name, assembly.GetReferencedAssemblies()[i].Name, assembly.GetReferencedAssemblies()[i].Version);

                GlobalAssemblyHelper.LoadGlobalAssembly(assembly.GetReferencedAssemblies()[i]);
                
                Logger.Log(LogType.Info, "Loaded referenced assembly for {0}: {1} (version: {2})", assembly.GetName().Name, assembly.GetReferencedAssemblies()[i].Name, assembly.GetReferencedAssemblies()[i].Version);
            }
#endif
        }

        public void Unload()
        {
            if (assembly == null)
            {
                return;
            }

            if (context == null)
            {
                throw new Exception($"Cannot unload assembly {guid} from {path} because it has no context");
            }

            context.Unload();

            int numObjectsRemoved = ManagedObjectCache.Instance.RemoveObjectsForAssembly(guid);

            Dictionary<Type, int> numCachedObjectsRemoved = new Dictionary<Type, int>();

            foreach (KeyValuePair<Type, IBasicCache> kvp in BasicCacheInstanceManager.Instance.GetInstances())
            {
                int numRemoved = kvp.Value.RemoveForAssembly(guid);

                numCachedObjectsRemoved.Add(kvp.Key, numRemoved);
            }

            Logger.Log(LogType.Info, $"Unloaded assembly {guid} from {path}:");
            Logger.Log(LogType.Info, $"\tRemoved {numObjectsRemoved} objects.");

            foreach (KeyValuePair<Type, int> kvp in numCachedObjectsRemoved)
            {
                Logger.Log(LogType.Info, $"\tRemoved {kvp.Value} {kvp.Key.Name} objects.");
            }

            assembly = null;
            context = null;
        }
    }

    internal class AssemblyCache
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
            foreach (KeyValuePair<Guid, AssemblyInstance> assembly in assemblies)
            {
                if (assembly.Value.AssemblyPath == path)
                {
                    return assembly.Value;
                }
            }

            return null;
        }

        public AssemblyInstance Add(Guid guid, string path, bool isCoreAssembly = false)
        {
            if (assemblies.ContainsKey(guid))
            {
                throw new Exception("Assembly already exists in cache");
            }

            AssemblyInstance assemblyInstance = new AssemblyInstance(guid, path, isCoreAssembly);
            assemblyInstance.Load();

            assemblies.Add(guid, assemblyInstance);

            Logger.Log(LogType.Info, $"Added assembly {guid} from {path}");

            return assemblyInstance;
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