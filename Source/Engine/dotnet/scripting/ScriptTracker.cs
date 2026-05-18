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

        private ScriptEventCallback? _callback;
        private IntPtr _callbackSelfPtr;

        private CSharpScriptCompiler? _csharpCompiler = null;
        private HypScriptCompiler? _hypCompiler = null;

        private Dictionary<string, ScriptInstance> _processingScripts = [];
        private Dictionary<string, CompileScriptEditorTask> _tasks = [];

        private List<string> _sourceDirectories = [];
        private string _intermediateDirectory = string.Empty;
        private string _binaryOutputDirectory = string.Empty;

        public void Initialize(Array sourceDirectoriesArray, string intermediateDirectory, string binaryOutputDirectory, IntPtr callbackPtr, IntPtr callbackSelfPtr)
        {
            Logger.Log(logChannel, LogLevel.Info, "Initializing script tracker...");

            _callback = Marshal.GetDelegateForFunctionPointer<ScriptEventCallback>(callbackPtr);
            _callbackSelfPtr = callbackSelfPtr;

            _sourceDirectories = sourceDirectoriesArray.Cast<string>().ToList();
            _intermediateDirectory = intermediateDirectory;
            _binaryOutputDirectory = binaryOutputDirectory;

            // Create language-specific compilers using the primary source directory
            if (_sourceDirectories.Count > 0)
            {
                Logger.Log(logChannel, LogLevel.Info, "Primary source directory: {0}", _sourceDirectories[0]);

                _csharpCompiler = new CSharpScriptCompiler(_sourceDirectories[0], _intermediateDirectory, _binaryOutputDirectory);
                _csharpCompiler.BuildAllProjects();

                _hypCompiler = new HypScriptCompiler(_sourceDirectories[0], _intermediateDirectory, _binaryOutputDirectory);
                _hypCompiler.BuildAllProjects();
            }

            Logger.Log(logChannel, LogLevel.Info, "Script tracker initialized with {0} source directories.", _sourceDirectories.Count);

            // Set up file system watchers for all source directories
            foreach (string sourceDir in _sourceDirectories)
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
            if (_processingScripts.Count == 0)
            {
                return;
            }

            Logger.Log(logChannel, LogLevel.Info, "Processing {0} scripts...", _processingScripts.Count);

            List<string> scriptsToRemove = [];

            foreach (KeyValuePair<string, ScriptInstance> entry in _processingScripts)
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

                ref ScriptDesc scriptDesc = ref entry.Value.Get();

                ScriptCompilerBase? compiler = scriptDesc.Language switch
                {
                    ScriptLanguage.CSharp => _csharpCompiler,
                    ScriptLanguage.HypScript => _hypCompiler,
                    _ => null
                };

                if (compiler != null)
                {
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
                    scriptDesc.CompileStatus = ScriptCompileStatus.Errored;
                }
            }

            foreach (string scriptPath in scriptsToRemove)
            {
                _processingScripts.Remove(scriptPath);

                if (_tasks.Remove(scriptPath, out CompileScriptEditorTask? task))
                {
                    task.SetIsCompleted(true);
                }
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
            if (_processingScripts.ContainsKey(filePath))
            {
                Logger.Log(logChannel, LogLevel.Info, "Script {0} is already being processed. Skipping...", filePath);

                return;
            }

            Logger.Log(logChannel, LogLevel.Info, "Adding script {0} to processing queue...", filePath);

            Debug.Assert(_sourceDirectories.Count >= 1, "Must have at least 1 directory to be able to compute relative path.");

            // @NOTE Path must be relative to Data/Scripts directory.
            // We assume the first directory is that.
            string relativePath = Path.GetRelativePath(_sourceDirectories[0], filePath);

            ScriptInstance scriptInstance = new ScriptInstance(new ScriptDesc
            {
                Path = relativePath,
                Language = language,
                CompileStatus = ScriptCompileStatus.Processing,
                HotReloadVersion = 0,
                LastModifiedTimestamp = 0
            });

            _processingScripts.Add(filePath, scriptInstance);

            TriggerCallback(new ScriptEvent
            {
                Type = ScriptEventType.StateChanged,
                ScriptPtr = scriptInstance.Address
            });

            StartCompilationTask(filePath, language);
        }

        private void StartCompilationTask(string filePath, ScriptLanguage language)
        {
            try
            {
                CompileScriptEditorTask task = new(filePath);
                task.SetIsForegroundTask(true);
                
                if (task.Commit())
                {
                    Logger.Log(logChannel, LogLevel.Info, "Started compilation task for {0}", filePath);

                    _tasks.Add(filePath, task);
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
            Debug.Assert(_callback != null);

            _callback(_callbackSelfPtr, scriptEvent);
        }
    }
}