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

        private List<FileSystemWatcher> watchers = new List<FileSystemWatcher>();

        private ScriptEventCallback? callback;
        private IntPtr callbackSelfPtr;

        private ScriptCompiler? scriptCompiler = null;

        private Dictionary<string, ScriptInstance> processingScripts = new Dictionary<string, ScriptInstance>();

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

            // For C# compilation, use the first source directory (typically Data/Scripts)
            // In the future, ScriptCompiler should support multiple directories
            if (sourceDirectories.Count > 0)
            {
                Logger.Log(logChannel, LogLevel.Info, "Primary source directory: {0}", sourceDirectories[0]);

                scriptCompiler = new ScriptCompiler(sourceDirectories[0], intermediateDirectory, binaryOutputDirectory);
                scriptCompiler.BuildAllProjects();
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

                // Watch for C# files
                var csWatcher = new FileSystemWatcher(sourceDir)
                {
                    NotifyFilter = NotifyFilters.LastWrite,
                    Filter = "*.cs",
                    EnableRaisingEvents = true,
                    IncludeSubdirectories = true
                };
                csWatcher.Changed += OnCsFileChanged;
                watchers.Add(csWatcher);

                Logger.Log(logChannel, LogLevel.Info, "Watching C# files in: {0}", sourceDir);

                // Watch for HypScript files
                var hypWatcher = new FileSystemWatcher(sourceDir)
                {
                    NotifyFilter = NotifyFilters.LastWrite,
                    Filter = "*.hyp",
                    EnableRaisingEvents = true,
                    IncludeSubdirectories = true
                };
                hypWatcher.Changed += OnHypFileChanged;
                watchers.Add(hypWatcher);

                Logger.Log(logChannel, LogLevel.Info, "Watching HypScript files in: {0}", sourceDir);
            }
        }

        public void Update()
        {
            if (processingScripts.Count == 0)
            {
                return;
            }

            Logger.Log(logChannel, LogLevel.Info, "Processing {0} scripts...", processingScripts.Count);

            List<string> scriptsToRemove = [];

            foreach (KeyValuePair<string, ScriptInstance> entry in processingScripts)
            {
                if (!entry.Value.IsValid)
                {
                    scriptsToRemove.Add(entry.Key);

                    continue;
                }

                if (entry.Value.Get().CompileStatus != ScriptCompileStatus.Processing)
                {
                    TriggerCallback(new ScriptEvent
                    {
                        Type = ScriptEventType.StateChanged,
                        ScriptPtr = entry.Value.Address
                    });

                    scriptsToRemove.Add(entry.Key);

                    continue;
                }

                if (scriptCompiler != null)
                {
                    ref ScriptDesc scriptDesc = ref entry.Value.Get();

                    try
                    {
                        if (scriptCompiler.Compile(ref scriptDesc))
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
            }

            foreach (string scriptPath in scriptsToRemove)
            {
                processingScripts.Remove(scriptPath);
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

        private void ProcessScriptFile(string filePath, ScriptLanguage language)
        {
            if (processingScripts.ContainsKey(filePath))
            {
                Logger.Log(logChannel, LogLevel.Info, "Script {0} is already being processed. Skipping...", filePath);

                return;
            }

            Logger.Log(logChannel, LogLevel.Info, "Adding script {0} to processing queue...", filePath);

            ScriptInstance scriptInstance = new ScriptInstance(new ScriptDesc
            {
                Path = filePath,
                Language = language,
                CompileStatus = ScriptCompileStatus.Processing,
                HotReloadVersion = 0,
                LastModifiedTimestamp = 0
            });

            processingScripts.Add(filePath, scriptInstance);

            TriggerCallback(new ScriptEvent
            {
                Type = ScriptEventType.StateChanged,
                ScriptPtr = scriptInstance.Address
            });

            // Start editor task for script compilation
            StartCompilationTask(filePath, language);
        }

        private void StartCompilationTask(string filePath, ScriptLanguage language)
        {
            try
            {
                // Create and commit a compilation task
                CompileScriptEditorTask task = new(filePath);
                task.SetIsForegroundTask(true);
                
                // Commit the task - this registers it with the editor state and shows it as foreground
                if (task.Commit())
                {
                    Logger.Log(logChannel, LogLevel.Info, "Started compilation task for {0}", filePath);
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