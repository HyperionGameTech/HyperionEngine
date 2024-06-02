using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class ScriptTracker
    {
        private Dictionary<Name, ManagedScriptWrapper> trackedScripts = new Dictionary<Name, ManagedScriptWrapper>();
        private Dictionary<Name, ManagedScriptWrapper> processingScripts = new Dictionary<Name, ManagedScriptWrapper>();
        private Dictionary<Name, ManagedScriptWrapper> erroredScripts = new Dictionary<Name, ManagedScriptWrapper>();

        private void UpdateTrackedScripts()
        {
            List<Name> erroredScriptsToRemove = new List<Name>();
            List<Name> scriptsToStartProcessing = new List<Name>();

            foreach (KeyValuePair<Name, ManagedScriptWrapper> trackedScript in trackedScripts)
            {
                if (!trackedScript.Value.IsValid)
                {
                    Console.WriteLine("Script {0} is not valid", trackedScript.Value.Get().Path);
                    continue;
                }

                if (trackedScript.Value.IsProcessing)
                {
                    continue;
                }

                if (trackedScript.Value.IsErrored)
                {
                    Console.WriteLine("Script {0} is in error state", trackedScript.Value.Get().Path);

                    erroredScriptsToRemove.Add(trackedScript.Key);

                    continue;
                }

                trackedScript.Value.UpdateState();

                if (trackedScript.Value.IsDirty)
                {
                    scriptsToStartProcessing.Add(trackedScript.Key);

                    continue;
                }
            }

            foreach (Name erroredScriptName in erroredScriptsToRemove)
            {
                erroredScripts.Add(erroredScriptName, trackedScripts[erroredScriptName]);
                trackedScripts.Remove(erroredScriptName);
            }

            foreach (Name processingScriptName in scriptsToStartProcessing)
            {
                ManagedScriptWrapper script = trackedScripts[processingScriptName];

                if (!script.IsValid)
                {
                    continue;
                }

                script.Get().State |= ManagedScriptState.Processing;

                processingScripts.Add(processingScriptName, script);
            }
        }

        private void UpdateProcessingScripts()
        {
            Console.WriteLine("ScriptTracker.UpdateProcessingScripts() called from C#. Currently have {0} processing scripts", processingScripts.Count);
        }

        public void Update()
        {
            Console.WriteLine("ScriptTracker.Update() called from C#. Currently have {0} tracked scripts", trackedScripts.Count);

            UpdateTrackedScripts();
            UpdateProcessingScripts();
        }

        public unsafe void StartTracking(Name name, IntPtr managedScriptPtr)
        {
            if (trackedScripts.ContainsKey(name))
            {
                throw new InvalidOperationException("Script already being tracked");
            }

            ManagedScriptWrapper managedScriptWrapper = new ManagedScriptWrapper(managedScriptPtr);
            trackedScripts.Add(name, managedScriptWrapper);
            
            Console.WriteLine("ScriptTracker.StartTracking() called from C# " + managedScriptWrapper.Get().Path);
        }

        public unsafe void StopTracking(Name name)
        {
            trackedScripts.Remove(name);
            processingScripts.Remove(name);
            erroredScripts.Remove(name);

            Console.WriteLine("ScriptTracker.StopTracking() called from C#");
        }
    }
}