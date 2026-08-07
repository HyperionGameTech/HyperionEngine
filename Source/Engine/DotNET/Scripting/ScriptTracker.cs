using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Collections.Generic;
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
        private HypScriptCompiler? hypScriptCompiler = null;
        private StrataScriptCompiler? strataCompiler = null;

        private Dictionary<string, ScriptDescWrapper> processingScripts = [];
        private Dictionary<string, CompileScriptEditorTask> tasks = [];

        private List<string> sourceDirectories = [];
        private string intermediateDirectory = string.Empty;
        private string binaryOutputDirectory = string.Empty;

        public void Initialize(Array sourceDirectoriesArray, string intermediateDirectory, string binaryOutputDirectory, IntPtr callbackPtr, IntPtr callbackSelfPtr)
        {
            Logger.Log(logChannel, LogLevel.Info, "Initializing script tracker...");

            callback = Marshal.GetDelegateForFunctionPointer<ScriptEventCallback>(callbackPtr);
            this.callbackSelfPtr = callbackSelfPtr;

            sourceDirectories = sourceDirectoriesArray.Cast<string>().ToList();
            this.intermediateDirectory = intermediateDirectory;
            this.binaryOutputDirectory = binaryOutputDirectory;

            if (sourceDirectories.Count > 0)
            {
                Logger.Log(logChannel, LogLevel.Info, "Primary source directory: {0}", sourceDirectories[0]);

                csharpCompiler = new CSharpScriptCompiler(sourceDirectories[0], intermediateDirectory, binaryOutputDirectory);
                csharpCompiler.BuildAllProjects();

                hypScriptCompiler = new HypScriptCompiler(sourceDirectories[0], intermediateDirectory, binaryOutputDirectory);
                hypScriptCompiler.BuildAllProjects();

                strataCompiler = new StrataScriptCompiler(sourceDirectories[0], intermediateDirectory, binaryOutputDirectory);
                strataCompiler.BuildAllProjects();
            }

            Logger.Log(logChannel, LogLevel.Info, "Script tracker initialized with {0} source directories.", sourceDirectories.Count);

            // Set up file system watchers for all source directories
            foreach (string sourceDir in sourceDirectories)
            {
                if (!System.IO.Directory.Exists(sourceDir))
                {
                    Logger.Log(logChannel, LogLevel.Warning, "Source directory does not exist: {0}", sourceDir);
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

                // Watch for HypScript files
                var hypWatcher = new FileSystemWatcher(sourceDir)
                {
                    NotifyFilter = NotifyFilters.LastWrite | NotifyFilters.FileName,
                    Filter = "*.hyp",
                    EnableRaisingEvents = true,
                    IncludeSubdirectories = true
                };
                hypWatcher.Changed += OnHypFileChanged;
                hypWatcher.Created += OnHypFileChanged;
                watchers.Add(hypWatcher);

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

        public void Shutdown()
        {
            Logger.Log(logChannel, LogLevel.Info, "Shutting down script tracker...");

            foreach (FileSystemWatcher watcher in watchers)
            {
                watcher.EnableRaisingEvents = false;
                watcher.Dispose();
            }

            watchers.Clear();

            processingScripts.Clear();

            callback = null;
            callbackSelfPtr = IntPtr.Zero;

            csharpCompiler = null;
            hypScriptCompiler = null;
            strataCompiler = null;

            sourceDirectories.Clear();
            intermediateDirectory = string.Empty;
            binaryOutputDirectory = string.Empty;
        }

        public void Update()
        {
            if (processingScripts.Count == 0)
            {
                return;
            }

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
                    ScriptLanguage.HypScript => hypScriptCompiler,
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

                bool removedFromProcessing = processingScripts.Remove(scriptPath);

                if (language == ScriptLanguage.CSharp) // HypScript editor tasks are managed on the native side.
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

        private void OnCsFileChanged(object source, FileSystemEventArgs e)
        {
            Logger.Log(logChannel, LogLevel.Info, "ScriptTracker: C# file changed: {0} {1}", e.FullPath, e.ChangeType);

            ProcessScriptFile(e.FullPath, ScriptLanguage.CSharp);
        }

        private void OnHypFileChanged(object source, FileSystemEventArgs e)
        {
            Logger.Log(logChannel, LogLevel.Info, "ScriptTracker: HypScript file changed: {0} {1}", e.FullPath, e.ChangeType);

            ProcessScriptFile(e.FullPath, ScriptLanguage.HypScript);
        }

        private void OnStrataFileChanged(object source, FileSystemEventArgs e)
        {
            Logger.Log(logChannel, LogLevel.Info, "ScriptTracker: Strata file changed: {0} {1}", e.FullPath, e.ChangeType);

            ProcessScriptFile(e.FullPath, ScriptLanguage.Strata);
        }

        private void ProcessScriptFile(string filePath, ScriptLanguage language)
        {
            if (processingScripts.ContainsKey(filePath))
            {
                Logger.Log(logChannel, LogLevel.Info, "Script {0} is already being processed. Skipping...", filePath);

                return;
            }

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
                // We only do this for C# since HypScript is compiled from C++ so we manage the editor task from there.
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