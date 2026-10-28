using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System.Collections.Concurrent;
using System.Diagnostics;

namespace Hyperion
{
    public delegate void ScriptEventCallback(IntPtr selfPtr, ScriptEvent scriptEvent);

    public class ScriptTracker
    {
        private static LogChannel logChannel = LogChannel.ByName("ScriptTracker");

        private List<FileSystemWatcher> watchers = [];

        private ScriptEventCallback? callback;
        private IntPtr callbackSelfPtr;

        private CSharpScriptCompiler? csharpCompiler = null;
        private StrataScriptCompiler? strataCompiler = null;

        private Dictionary<string, ScriptDescWrapper> processingScripts = [];
        private Dictionary<string, CompileScriptEditorTask> tasks = [];

        // watchers raise events on thread pool threads. these changes are picked up on the sim thread in Update()
        private ConcurrentQueue<KeyValuePair<string, ScriptLanguage>> changedFiles = new();

        // events point at these, so they're kept until the next Update()
        private List<ScriptDescWrapper> finishedScripts = [];

        private Dictionary<string, DateTime> processedWriteTimes = [];

        private List<string> forcedRebuildFiles = [];

        private HashSet<string> missingSourceDirectories = [];
        private ConcurrentQueue<string> createdSourceDirectories = new();

        private List<string> sourceDirectories = [];
        private string intermediateDirectory = string.Empty;
        private string binaryOutputDirectory = string.Empty;

        private List<ScriptReferenceAssembly> referenceAssemblies = [];

        private List<string> pluginModules = [];

        public void Initialize(Array sourceDirectoriesArray, string intermediateDirectory, string binaryOutputDirectory, IntPtr callbackPtr, IntPtr callbackSelfPtr)
        {
            Logger.Log(logChannel, LogLevel.Info, "Initializing script tracker...");

            callback = Marshal.GetDelegateForFunctionPointer<ScriptEventCallback>(callbackPtr);
            this.callbackSelfPtr = callbackSelfPtr;

            sourceDirectories = sourceDirectoriesArray.Cast<string>().Select(System.IO.Path.GetFullPath).ToList();
            this.intermediateDirectory = intermediateDirectory;
            this.binaryOutputDirectory = binaryOutputDirectory;

            if (sourceDirectories.Count > 0)
            {
                Logger.Log(logChannel, LogLevel.Info, "Primary source directory: {0}", sourceDirectories[0]);

                csharpCompiler = new CSharpScriptCompiler(sourceDirectories[0], intermediateDirectory, binaryOutputDirectory);
                csharpCompiler.SetReferenceAssemblies(referenceAssemblies);
                csharpCompiler.SetPluginModules(pluginModules);
                csharpCompiler.BuildAllProjects();


                strataCompiler = new StrataScriptCompiler(sourceDirectories[0], intermediateDirectory, binaryOutputDirectory);
                strataCompiler.BuildAllProjects();
            }

            Logger.Log(logChannel, LogLevel.Info, "Script tracker initialized with {0} source directories.", sourceDirectories.Count);

            CreateWatchers();
        }

        // Just the C# compiler, without file watchers or building every module: for one-off assembly resolution, eg loading a
        // project's script modules before its scenes are read
        public void InitializeCompiler(string sourceDirectory, string intermediateDirectory, string binaryOutputDirectory)
        {
            this.intermediateDirectory = intermediateDirectory;
            this.binaryOutputDirectory = binaryOutputDirectory;

            csharpCompiler = new CSharpScriptCompiler(sourceDirectory, intermediateDirectory, binaryOutputDirectory);
            csharpCompiler.SetReferenceAssemblies(referenceAssemblies);
            csharpCompiler.SetPluginModules(pluginModules);
        }

        public void SetPluginModules(Array moduleNamesArray)
        {
            pluginModules = moduleNamesArray.Cast<string>().ToList();

            csharpCompiler?.SetPluginModules(pluginModules);
        }

        public void SetReferenceAssembly(string scopeDirectory, string assemblyPath)
        {
            string fullScopeDirectory = System.IO.Path.GetFullPath(scopeDirectory);

            referenceAssemblies.RemoveAll(reference => string.Equals(reference.ScopeDirectory, fullScopeDirectory, StringComparison.OrdinalIgnoreCase));

            if (assemblyPath.Length != 0)
            {
                referenceAssemblies.Add(new ScriptReferenceAssembly(fullScopeDirectory, System.IO.Path.GetFullPath(assemblyPath)));
            }

            csharpCompiler?.SetReferenceAssemblies(referenceAssemblies);
        }

        public void RebuildModulesIn(string directory)
        {
            if (csharpCompiler == null || !System.IO.Directory.Exists(directory))
            {
                return;
            }

            foreach (string sourceDirectory in sourceDirectories)
            {
                if (!System.IO.Directory.Exists(sourceDirectory))
                {
                    continue;
                }

                IEnumerable<string> moduleDirectories = System.IO.Directory.EnumerateDirectories(sourceDirectory, "*", SearchOption.AllDirectories)
                    .Append(sourceDirectory)
                    .Where(moduleDirectory => CSharpScriptCompiler.IsInDirectory(moduleDirectory, directory));

                foreach (string moduleDirectory in moduleDirectories)
                {
                    string? scriptFile = System.IO.Directory.EnumerateFiles(moduleDirectory, "*.cs").FirstOrDefault();

                    if (scriptFile != null && !forcedRebuildFiles.Contains(scriptFile))
                    {
                        forcedRebuildFiles.Add(scriptFile);
                    }
                }
            }
        }

        public void UpdateSourceDirectories(Array sourceDirectoriesArray)
        {
            Logger.Log(logChannel, LogLevel.Info, "Updating script source directories...");

            DisposeWatchers();

            sourceDirectories = sourceDirectoriesArray.Cast<string>().Select(System.IO.Path.GetFullPath).ToList();

            CreateWatchers();
        }

        private void DisposeWatchers()
        {
            foreach (FileSystemWatcher watcher in watchers)
            {
                watcher.EnableRaisingEvents = false;
                watcher.Dispose();
            }

            watchers.Clear();
        }

        private void CreateWatchers()
        {
            missingSourceDirectories.Clear();

            // Set up file system watchers for all source directories
            foreach (string sourceDir in sourceDirectories)
            {
                if (!System.IO.Directory.Exists(sourceDir))
                {
                    Logger.Log(logChannel, LogLevel.Info, "Source directory does not exist yet, waiting for it to be created: {0}", sourceDir);

                    WatchForDirectoryCreation(sourceDir);

                    continue;
                }

                Logger.Log(logChannel, LogLevel.Info, "Watching source directory: {0}", sourceDir);

                // Watch for C# files
                var csWatcher = new FileSystemWatcher(sourceDir)
                {
                    NotifyFilter = NotifyFilters.LastWrite | NotifyFilters.FileName,
                    Filter = "*.cs",
                    EnableRaisingEvents = true,
                    IncludeSubdirectories = true
                };
                csWatcher.Changed += OnCsFileChanged;
                csWatcher.Created += OnCsFileChanged;
                watchers.Add(csWatcher);


                // Watch for Strata files
                var strataWatcher = new FileSystemWatcher(sourceDir)
                {
                    NotifyFilter = NotifyFilters.LastWrite | NotifyFilters.FileName,
                    Filter = "*.strata",
                    EnableRaisingEvents = true,
                    IncludeSubdirectories = true
                };
                strataWatcher.Changed += OnStrataFileChanged;
                strataWatcher.Created += OnStrataFileChanged;
                watchers.Add(strataWatcher);
            }
        }

        private void WatchForDirectoryCreation(string directory)
        {
            string? existingAncestor = System.IO.Path.GetDirectoryName(directory);

            while (existingAncestor != null && !System.IO.Directory.Exists(existingAncestor))
            {
                existingAncestor = System.IO.Path.GetDirectoryName(existingAncestor);
            }

            if (existingAncestor == null)
            {
                Logger.Log(logChannel, LogLevel.Warning, "No existing parent directory to watch for the creation of {0}", directory);

                return;
            }

            missingSourceDirectories.Add(directory);

            var directoryWatcher = new FileSystemWatcher(existingAncestor)
            {
                NotifyFilter = NotifyFilters.DirectoryName,
                IncludeSubdirectories = true
            };
            directoryWatcher.Created += (_, _) => OnWatchedDirectoryChanged(directory);
            directoryWatcher.Renamed += (_, _) => OnWatchedDirectoryChanged(directory);
            directoryWatcher.EnableRaisingEvents = true;
            watchers.Add(directoryWatcher);
        }

        private void OnWatchedDirectoryChanged(string directory)
        {
            if (System.IO.Directory.Exists(directory))
            {
                createdSourceDirectories.Enqueue(directory);
            }
        }

        private void WatchCreatedSourceDirectories()
        {
            bool anyCreated = false;

            while (createdSourceDirectories.TryDequeue(out string? createdDirectory))
            {
                anyCreated |= missingSourceDirectories.Contains(createdDirectory);
            }

            if (!anyCreated)
            {
                return;
            }

            List<string> createdDirectories = missingSourceDirectories.Where(System.IO.Directory.Exists).ToList();

            DisposeWatchers();
            CreateWatchers();

            // files written along with the directory came before its watchers
            foreach (string directory in createdDirectories)
            {
                Logger.Log(logChannel, LogLevel.Info, "Source directory created: {0}", directory);

                foreach (string file in System.IO.Directory.EnumerateFiles(directory, "*.cs", SearchOption.AllDirectories))
                {
                    changedFiles.Enqueue(new(file, ScriptLanguage.CSharp));
                }

                foreach (string file in System.IO.Directory.EnumerateFiles(directory, "*.strata", SearchOption.AllDirectories))
                {
                    changedFiles.Enqueue(new(file, ScriptLanguage.Strata));
                }
            }
        }

        public void Shutdown()
        {
            Logger.Log(logChannel, LogLevel.Info, "Shutting down script tracker...");

            DisposeWatchers();

            missingSourceDirectories.Clear();
            createdSourceDirectories.Clear();

            foreach (ScriptDescWrapper wrapper in processingScripts.Values)
            {
                wrapper.Dispose();
            }

            foreach (ScriptDescWrapper wrapper in finishedScripts)
            {
                wrapper.Dispose();
            }

            processingScripts.Clear();
            finishedScripts.Clear();
            changedFiles.Clear();
            processedWriteTimes.Clear();
            forcedRebuildFiles.Clear();

            callback = null;
            callbackSelfPtr = IntPtr.Zero;

            csharpCompiler = null;
            strataCompiler = null;

            sourceDirectories.Clear();
            intermediateDirectory = string.Empty;
            binaryOutputDirectory = string.Empty;
        }

        public void Update()
        {
            foreach (ScriptDescWrapper wrapper in finishedScripts)
            {
                wrapper.Dispose();
            }

            finishedScripts.Clear();

            if (processingScripts.Count != 0)
            {
                ProcessScripts();
            }

            WatchCreatedSourceDirectories();

            // compiled on the next Update(), so the compile task shows before the build blocks this thread
            while (changedFiles.TryDequeue(out KeyValuePair<string, ScriptLanguage> changedFile))
            {
                ProcessScriptFile(changedFile.Key, changedFile.Value);
            }

            foreach (string forcedRebuildFile in forcedRebuildFiles)
            {
                ProcessScriptFile(forcedRebuildFile, ScriptLanguage.CSharp, force: true);
            }

            forcedRebuildFiles.Clear();
        }

        private void ProcessScripts()
        {
            Logger.Log(logChannel, LogLevel.Info, "Processing {0} scripts...", processingScripts.Count);

            List<KeyValuePair<string, ScriptLanguage>> scriptsToRemove = [];

            foreach (KeyValuePair<string, ScriptDescWrapper> entry in processingScripts)
            {
                if (!entry.Value.IsValid)
                {
                    scriptsToRemove.Add(new(entry.Key, entry.Value.Get().Language));

                    continue;
                }

                if (entry.Value.Get().CompileStatus != ScriptCompileStatus.Processing)
                {
                    TriggerCallback(new ScriptEvent
                    {
                        Type = ScriptEventType.StateChanged,
                        ScriptPtr = entry.Value.Address
                    });

                    scriptsToRemove.Add(new(entry.Key, entry.Value.Get().Language));

                    continue;
                }

                ScriptCompilerBase? compiler = entry.Value.Get().Language switch
                {
                    ScriptLanguage.CSharp => csharpCompiler,
                    ScriptLanguage.Strata => strataCompiler,
                    _ => null
                };

                if (compiler != null)
                {
                    ref ScriptDesc scriptDesc = ref entry.Value.Get();

                    try
                    {
                        if (compiler.Compile(ref scriptDesc))
                        {
                            scriptDesc.CompileStatus |= ScriptCompileStatus.Compiled;
                        }
                        else
                        {
                            scriptDesc.CompileStatus |= ScriptCompileStatus.Errored;
                        }
                    }
                    catch (Exception e)
                    {
                        Logger.Log(logChannel, LogLevel.Error, "Error compiling script {0}: {1}", entry.Key, e.Message);

                        scriptDesc.CompileStatus |= ScriptCompileStatus.Errored;
                    }

                    scriptDesc.CompileStatus &= ~ScriptCompileStatus.Processing;
                    scriptDesc.CompileStatus &= ~ScriptCompileStatus.Dirty;

                    // C# scripts reload once their assembly is built; Strata scripts already reloaded from source when they changed
                    if (scriptDesc.Language == ScriptLanguage.CSharp)
                    {
                        TriggerCallback(new ScriptEvent
                        {
                            Type = ScriptEventType.StateChanged,
                            ScriptPtr = entry.Value.Address
                        });
                    }
                }
                else
                {
                    entry.Value.Get().CompileStatus = ScriptCompileStatus.Errored;
                }

                scriptsToRemove.Add(new(entry.Key, entry.Value.Get().Language));
            }

            foreach (KeyValuePair<string, ScriptLanguage> kvp in scriptsToRemove)
            {
                string scriptPath = kvp.Key;
                ScriptLanguage language = kvp.Value;

                bool removedFromProcessing = processingScripts.Remove(scriptPath, out ScriptDescWrapper? finishedScript);

                if (finishedScript != null)
                {
                    finishedScripts.Add(finishedScript);
                }

                if (language == ScriptLanguage.CSharp)
                {
                    if (tasks.Remove(scriptPath, out CompileScriptEditorTask? task))
                    {
                        task.SetIsCompleted(true);
                    } else
                    {
                        Logger.Log(LogLevel.Warning, "Editor task not found for script {}", scriptPath);
                    }
                }

                Debug.Assert(removedFromProcessing);
            }
        }

        public bool ResolveAssembly(string scriptPath, IntPtr scriptDescPtr)
        {
            if (csharpCompiler == null || scriptDescPtr == IntPtr.Zero)
            {
                return false;
            }

            try
            {
                unsafe
                {
                    return csharpCompiler.ResolveAssembly(scriptPath, ref *(ScriptDesc*)scriptDescPtr);
                }
            }
            catch (Exception e)
            {
                Logger.Log(logChannel, LogLevel.Error, "Error resolving assembly for script {0}: {1}", scriptPath, e.Message);

                return false;
            }
        }

        private void OnCsFileChanged(object source, FileSystemEventArgs e)
        {
            Logger.Log(logChannel, LogLevel.Info, "ScriptTracker: C# file changed: {0} {1}", e.FullPath, e.ChangeType);

            changedFiles.Enqueue(new(e.FullPath, ScriptLanguage.CSharp));
        }


        private void OnStrataFileChanged(object source, FileSystemEventArgs e)
        {
            Logger.Log(logChannel, LogLevel.Info, "ScriptTracker: Strata file changed: {0} {1}", e.FullPath, e.ChangeType);

            changedFiles.Enqueue(new(e.FullPath, ScriptLanguage.Strata));
        }

        private void ProcessScriptFile(string filePath, ScriptLanguage language, bool force = false)
        {
            if (processingScripts.ContainsKey(filePath))
            {
                Logger.Log(logChannel, LogLevel.Info, "Script {0} is already being processed. Skipping...", filePath);

                return;
            }

            // one save often raises several change events, some only after the build they triggered
            DateTime writeTime = File.Exists(filePath) ? File.GetLastWriteTimeUtc(filePath) : DateTime.MinValue;

            if (!force && processedWriteTimes.TryGetValue(filePath, out DateTime processedWriteTime) && processedWriteTime == writeTime)
            {
                return;
            }

            processedWriteTimes[filePath] = writeTime;

            Logger.Log(logChannel, LogLevel.Info, "Adding script {0} to processing queue...", filePath);

            ScriptDescWrapper wrapper = new(new ScriptDesc
            {
                Path = filePath,
                Language = language,
                CompileStatus = ScriptCompileStatus.Processing,
                HotReloadVersion = 0,
                LastModifiedTimestamp = 0
            });

            processingScripts.Add(filePath, wrapper);

            TriggerCallback(new ScriptEvent
            {
                Type = ScriptEventType.StateChanged,
                ScriptPtr = wrapper.Address
            });

            if (language == ScriptLanguage.CSharp)
            {
                // Start editor task for script compilation
                // Only C# compilation runs as an editor task.
                StartCompilationTask(filePath, language);
            }
        }

        private void StartCompilationTask(string filePath, ScriptLanguage language)
        {
            try
            {
                // Create and commit a compilation StartCompilationTask
                CompileScriptEditorTask task = new(filePath);
                task.SetIsForegroundTask(true);
                
                // Commit the task - this registers it with the editor state and shows it as foreground
                if (task.Commit())
                {
                    Logger.Log(logChannel, LogLevel.Info, "Started compilation task for {0}", filePath);

                    tasks.Add(filePath, task);
                }
                else
                {
                    Logger.Log(logChannel, LogLevel.Error, "Failed to commit compilation task for {0}", filePath);
                }
            }
            catch (Exception e)
            {
                Logger.Log(logChannel, LogLevel.Error, "Failed to start compilation task: {0}", e.Message);
            }
        }

        private void TriggerCallback(ScriptEvent scriptEvent)
        {
            Debug.Assert(callback != null);
            callback(callbackSelfPtr, scriptEvent);
        }
    }
}
