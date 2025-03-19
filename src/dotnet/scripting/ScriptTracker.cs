using System;
using System.IO;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public delegate void ScriptEventCallback(IntPtr selfPtr, ScriptEvent scriptEvent);

    public class ScriptTracker
    {
        private static LogChannel logChannel = LogChannel.ByName("ScriptTracker");

        private FileSystemWatcher watcher;

        private ScriptEventCallback callback;
        private IntPtr callbackSelfPtr;

        private ScriptCompiler scriptCompiler = null;

        private Dictionary<string, ManagedScriptWrapper> processingScripts = new Dictionary<string, ManagedScriptWrapper>();

        public void Initialize(string sourceDirectory, string intermediateDirectory, string binaryOutputDirectory, IntPtr callbackPtr, IntPtr callbackSelfPtr)
        {
            Logger.Log(logChannel, LogType.Info, "Initializing script tracker...");

            this.callback = Marshal.GetDelegateForFunctionPointer<ScriptEventCallback>(callbackPtr);
            this.callbackSelfPtr = callbackSelfPtr;

            Logger.Log(logChannel, LogType.Info, "Source directory: {0}", sourceDirectory);

            scriptCompiler = new ScriptCompiler(sourceDirectory, intermediateDirectory, binaryOutputDirectory);
            scriptCompiler.BuildAllProjects();

            Logger.Log(logChannel, LogType.Info, "Script tracker initialized.");

            watcher = new FileSystemWatcher(sourceDirectory);
            watcher.NotifyFilter = NotifyFilters.LastWrite;
            watcher.Filter = "*.cs";
            watcher.Changed += OnFileChanged;
            watcher.EnableRaisingEvents = true;
            watcher.IncludeSubdirectories = true;
        }

        public void Update()
        {
            Logger.Log(logChannel, LogType.Debug, "Update script tracker");

            if (processingScripts.Count == 0)
            {
                return;
            }

            Logger.Log(logChannel, LogType.Info, "Processing {0} scripts...", processingScripts.Count);

            List<string> scriptsToRemove = new List<string>();

            foreach (KeyValuePair<string, ManagedScriptWrapper> entry in processingScripts)
            {
                if (!entry.Value.IsValid)
                {
                    scriptsToRemove.Add(entry.Key);

                    continue;
                }

                if (entry.Value.Get().State != ManagedScriptState.Processing)
                {
                    TriggerCallback(new ScriptEvent
                    {
                        Type = ScriptEventType.StateChanged,
                        ScriptPtr = entry.Value.Address
                    });

                    scriptsToRemove.Add(entry.Key);

                    continue;
                }

                // script processing - compile using Roslyn
                // just testing for now
                if (scriptCompiler != null)
                {
                    ref ManagedScript managedScript = ref entry.Value.Get();

                    try
                    {
                        if (scriptCompiler.Compile(ref managedScript))
                        {
                            managedScript.State |= ManagedScriptState.Compiled;
                        }
                        else
                        {
                            managedScript.State |= ManagedScriptState.Errored;
                        }
                    }
                    catch (Exception e)
                    {
                        Logger.Log(logChannel, LogType.Error, "Error compiling script {0}: {1}", entry.Key, e.Message);

                        managedScript.State |= ManagedScriptState.Errored;
                    }

                    managedScript.State &= ~ManagedScriptState.Processing;
                    managedScript.State &= ~ManagedScriptState.Dirty;
                }
                else
                {
                    entry.Value.Get().State = ManagedScriptState.Errored;
                }
            }

            foreach (string scriptPath in scriptsToRemove)
            {
                processingScripts.Remove(scriptPath);
            }
        }

        private void OnFileChanged(object source, FileSystemEventArgs e)
        {
            Logger.Log(logChannel, LogType.Info, "ScriptTracker: File changed: {0} {1}", e.FullPath, e.ChangeType);

            if (processingScripts.ContainsKey(e.FullPath))
            {
                Logger.Log(logChannel, LogType.Info, "Script {0} is already being processed. Skipping...", e.FullPath);

                return;
            }

            Logger.Log(logChannel, LogType.Info, "Adding script {0} to processing queue...", e.FullPath);

            ManagedScriptWrapper managedScriptWrapper = new ManagedScriptWrapper(new ManagedScript
            {
                Path = e.FullPath,
                State = ManagedScriptState.Processing,
                HotReloadVersion = 0,
                LastModifiedTimestamp = 0
            });

            processingScripts.Add(e.FullPath, managedScriptWrapper);

            TriggerCallback(new ScriptEvent
            {
                Type = ScriptEventType.StateChanged,
                ScriptPtr = managedScriptWrapper.Address
            });
        }

        private void TriggerCallback(ScriptEvent scriptEvent)
        {
            callback(callbackSelfPtr, scriptEvent);
        }
    }
}