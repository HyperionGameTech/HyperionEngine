using System;
using System.Reflection;
using System.Runtime.Loader;

namespace Hyperion
{
    internal class ScriptAssemblyContext : AssemblyLoadContext
    {
        private AssemblyDependencyResolver resolver;

        public ScriptAssemblyContext(string basePath) : base(isCollectible: true)
        {
            resolver = new AssemblyDependencyResolver(basePath);
        }

        protected override Assembly? Load(AssemblyName name)
        {
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

        public AssemblyInstance(Guid guid, string path)
        {
            this.guid = guid;
            this.path = path;
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

        public void Load()
        {
            if (assembly != null)
            {
                return;
            }

            context = new ScriptAssemblyContext(path);
            assembly = context.LoadFromAssemblyPath(path);

#if DEBUG // Load all referenced assemblies to ensure nothing will crash later
            Logger.Log(LogType.Info, "Loaded assembly: {0}, with {1} referenced assemblies.", assembly.FullName, assembly.GetReferencedAssemblies().Length);

            for (int i = 0; i < assembly.GetReferencedAssemblies().Length; i++)
            {
                Logger.Log(LogType.Info, "Found referenced assembly: {0}", assembly.GetReferencedAssemblies()[i].Name);

                context.LoadFromAssemblyName(assembly.GetReferencedAssemblies()[i]);
            }
#endif
        }

        public void Unload()
        {
            if (assembly == null)
            {
                return;
            }

            context.Unload();

            int numObjectsRemoved = ManagedObjectCache.Instance.RemoveObjectsForAssembly(guid);
            int numMethodsRemoved = ManagedMethodCache.Instance.RemoveMethodsForAssembly(guid);

            Logger.Log(LogType.Info, $"Unloaded assembly {guid} from {path}. Removed {numObjectsRemoved} objects, {numMethodsRemoved} methods");

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

        public AssemblyInstance Add(Guid guid, string path)
        {
            if (assemblies.ContainsKey(guid))
            {
                throw new Exception("Assembly already exists in cache");
            }

            AssemblyInstance assemblyInstance = new AssemblyInstance(guid, path);
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