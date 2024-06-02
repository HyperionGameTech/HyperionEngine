using System;
using System.IO;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public delegate void ScriptEventCallback(IntPtr selfPtr, ScriptEvent scriptEvent);

    public class ScriptTracker
    {
        private FileSystemWatcher watcher;
        private IntPtr callbackPtr;
        private IntPtr callbackSelfPtr;

        private Dictionary<string, ManagedScriptWrapper> processingScripts = new Dictionary<string, ManagedScriptWrapper>();

        public void Initialize(string path, IntPtr callbackPtr, IntPtr callbackSelfPtr)
        {
            this.callbackPtr = callbackPtr;
            this.callbackSelfPtr = callbackSelfPtr;

            watcher = new FileSystemWatcher(path);
            watcher.NotifyFilter = NotifyFilters.LastWrite;
            watcher.Filter = "*.cs";
            watcher.Changed += OnFileChanged;
            watcher.EnableRaisingEvents = true;
            watcher.IncludeSubdirectories = true;
        }

        public void Update()
        {
            Console.WriteLine("ScriptTracker.Update() called from C#. Currently have {0} scripts processing", processingScripts.Count);

            List<string> scriptsToRemove = new List<string>();

            foreach (KeyValuePair<string, ManagedScriptWrapper> entry in processingScripts)
            {
                if (!entry.Value.IsValid)
                {
                    scriptsToRemove.Add(entry.Key);

                    continue;
                }

                Console.WriteLine("ScriptTracker.Update() : {0} is processing", entry.Key);

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

                // @TODO : Implement script processing - compile using Roslyn
            }

            foreach (string scriptPath in scriptsToRemove)
            {
                processingScripts.Remove(scriptPath);
            }
        }

        private void OnFileChanged(object source, FileSystemEventArgs e)
        {
            Console.WriteLine("ScriptTracker.OnFileChanged() : {0}", e.FullPath);

            if (processingScripts.ContainsKey(e.FullPath))
            {
                Console.WriteLine("ScriptTracker.OnFileChanged() : {0} is already being processed", e.FullPath);

                return;
            }

            ManagedScriptWrapper managedScriptWrapper = new ManagedScriptWrapper(new ManagedScript
            {
                Path = e.FullPath,
                State = ManagedScriptState.Processing
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
            if (callbackPtr == IntPtr.Zero)
            {
                throw new InvalidOperationException("ScriptTracker callback pointer is not set");
            }

            ScriptEventCallback callback = Marshal.GetDelegateForFunctionPointer<ScriptEventCallback>(callbackPtr);
            callback(callbackSelfPtr, scriptEvent);
        }
    }
}