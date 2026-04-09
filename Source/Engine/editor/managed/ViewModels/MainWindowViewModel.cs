using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;
using System;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Linq;
using System.Threading;
using System.Windows.Input;

namespace Hyperion.Editor.ViewModels
{
    public class MainWindowViewModel : ViewModelBase, IDisposable
    {
        private string _title = "Hyperion Editor";
        public string Title
        {
            get => _title;
            set => SetProperty(ref _title, value);
        }
        public SceneHierarchyViewModel SceneHierarchy { get; private set; }
        public InspectorViewModel Inspector { get; private set; }
        public ContentBrowserViewModel ContentBrowser { get; private set; }

        public EditorCommand NewProject => new EditorCommand("NewProject");
        public EditorCommand OpenProject => new EditorCommand("OpenProject");
        public EditorCommand SaveProject => new EditorCommand("SaveProject");
        public EditorCommand SaveProjectAs => new EditorCommand("SaveProjectAs");
        public EditorCommand Exit => new EditorCommand("Exit");
        public EditorCommand Undo => new EditorCommand("Undo");
        public EditorCommand Redo => new EditorCommand("Redo");

        private string _undoHeader = "_Undo";
        public string UndoHeader
        {
            get => _undoHeader;
            set => SetProperty(ref _undoHeader, value);
        }

        private string _redoHeader = "_Redo";
        public string RedoHeader
        {
            get => _redoHeader;
            set => SetProperty(ref _redoHeader, value);
        }
        public EditorCommand Copy => new EditorCommand("Copy");
        public EditorCommand Paste => new EditorCommand("Paste");

        public EditorCommand AddEmptyNode => new EditorCommand("AddEmptyNode");
        public EditorCommand AddEntity => new EditorCommand("AddEntity");
        private EditorCommand _addInstance = new EditorCommand("AddInstance");
        public EditorCommand AddInstance => _addInstance;

        private bool _canAddInstance = false;
        public bool CanAddInstance
        {
            get => _canAddInstance;
            private set
            {
                if (SetProperty(ref _canAddInstance, value))
                {
                    _addInstance.RaiseCanExecuteChanged();
                }
            }
        }
        public EditorCommand AddCamera => new EditorCommand("AddCamera");

        public EditorCommand AddPointLight => new EditorCommand("AddPointLight");
        public EditorCommand AddDirectionalLight => new EditorCommand("AddDirectionalLight");
        public EditorCommand AddSpotLight => new EditorCommand("AddSpotLight");
        public EditorCommand AddAreaRectLight => new EditorCommand("AddAreaRectLight");

        public EditorCommand AddLightmapVolume => new EditorCommand("AddLightmapVolume");
        public EditorCommand AddReflectionProbe => new EditorCommand("AddReflectionProbe");
        public EditorCommand AddParticleVolume => new EditorCommand("AddParticleVolume");
        public EditorCommand AddFogVolume => new EditorCommand("AddFogVolume");

        public ICommand SelectTransformModeTranslate { get; private set; }
        public ICommand SelectTransformModeRotate { get; private set; }
        public ICommand SelectTransformModeScale { get; private set; }

        public bool CanSelectTransformModeTranslate => CanSelectGizmo;// && _editorSubsystem.GetSelectedGizmo()?.ManipulationMode != EditorManipulationMode.Translate;
        public bool CanSelectTransformModeRotate => CanSelectGizmo;// && _editorSubsystem.GetSelectedGizmo()?.ManipulationMode != EditorManipulationMode.Rotate;
        public bool CanSelectTransformModeScale => CanSelectGizmo;// && _editorSubsystem.GetSelectedGizmo()?.ManipulationMode != EditorManipulationMode.Scale;

        public bool CanSelectGizmo = true; // temp hax

        public ICommand SetGameModePlaying { get; private set; }
        public bool CanSetGameModePlaying
        {
            get
            {
                EditorProject? project = EngineManager.CurrentProject;

                if (project == null)
                {
                    return false;
                }

                return project.World.GetGameState().Mode != GameStateMode.Simulating;
            }
        }

        public ICommand SetGameModePaused { get; private set; }
        public bool CanSetGameModePaused
        {
            get
            {
                EditorProject? project = EngineManager.CurrentProject;

                if (project == null)
                {
                    return false;
                }

                return project.World.GetGameState().Mode == GameStateMode.Simulating;
            }
        }

        public ICommand SetGameModeStopped { get; private set; }
        public bool CanSetGameModeStopped
        {
            get
            {
                EditorProject? project = EngineManager.CurrentProject;

                if (project == null)
                {
                    return false;
                }

                return !project.World.GetGameState().Stopped;
            }
        }

        private DelegateHandler? _gameInstanceLaunchedHandler;
        private DelegateHandler? _gameModeChangedHandler;
        private DelegateHandler? _focusedNodeChangedHandler;
        private DelegateHandler? _currentProjectChangedHandler;
        private DelegateHandler? _selectedGizmoChangedHandler;
        private DelegateHandler? _activeSceneChangedHandler;
        private DelegateHandler? _actionStackStateChangedHandler;

        private int _isUpdatingSelectionFromEngine = 0; // atomic
        private bool _isReady = false;

        private EditorSubsystem _editorSubsystem;

        public ObservableCollection<SceneViewModel> Scenes { get; } = new();

        public ObservableCollection<TaskItemViewModel> Tasks { get; } = new();

        public ForegroundTaskViewModel ForegroundTask { get; private set; }
        
        private SceneViewModel? _activeScene;
        public SceneViewModel? ActiveScene
        {
            get => _activeScene;
            set
            {
                Logger.Log(LogLevel.Info, $"Setting ActiveScene to {(value != null ? value.Scene.Name.ToString() : "null")}");
                
                if (_activeScene == value)
                    return;

                Debug.Assert(value == null || (Scenes.Contains(value) && value.Scene.SceneFlags.HasFlag(SceneFlags.Foreground)),
                    "ActiveScene must be one of the scenes in the Scenes collection or null (only foreground scenes can be active)");

                _activeScene = value;

                _ = EngineManager.PostToSimThread(() =>
                {
                    try
                    {
                        _editorSubsystem.SetActiveScene(_activeScene?.Scene);
                    }
                    catch (Exception ex)
                    {
                        Logger.Log(LogLevel.Warning, $"Failed to set active scene: {ex.Message}");
                    }
                });

                OnPropertyChanged(nameof(ActiveScene));
            }
        }

        public ICommand SetActiveSceneCommand { get; private set; }

        public MainWindowViewModel()
        {
            SceneHierarchy = new SceneHierarchyViewModel();
            Inspector = new InspectorViewModel();
            ForegroundTask = new ForegroundTaskViewModel();

            SelectTransformModeTranslate = new SetGizmoCommand(EditorManipulationMode.Translate);
            SelectTransformModeRotate = new SetGizmoCommand(EditorManipulationMode.Rotate);
            SelectTransformModeScale = new SetGizmoCommand(EditorManipulationMode.Scale);

            SetGameModePlaying = new SetGameModeCommand(GameStateMode.Simulating);
            SetGameModePaused = new SetGameModeCommand(GameStateMode.Paused);
            SetGameModeStopped = new SetGameModeCommand(GameStateMode.Stopped);

            SetActiveSceneCommand = new RelayCommand<SceneViewModel>(scene =>
            {
                if (scene != null)
                {
                    ActiveScene = scene;
                }
            });

            HyperionEditorGame? editorGame = EngineManager.EditorGame;
            if (editorGame == null)
                throw new InvalidOperationException("Editor game instance is not initialized.");

            if (editorGame.IsLaunched())
            {
                Init(editorGame);

                return;
            }

            Logger.Log(LogLevel.Verbose, "Game instance not yet launched, setting up callback to be notified when ready");

            _gameInstanceLaunchedHandler = editorGame.GetOnLaunchedDelegate().Bind(() =>
            {
                Dispatcher.UIThread.Post(() =>
                {
                    Init(editorGame);
                });
            });
        }

        private void Init(HyperionEditorGame editorGame)
        {
            Dispatcher.UIThread.VerifyAccess();

            World? world = editorGame.World;
            if (world == null)
                throw new InvalidOperationException("Editor world is not initialized.");

            EditorSubsystem? editorSubsystem = world.GetSubsystem<EditorSubsystem>();
            if (editorSubsystem == null)
                throw new InvalidOperationException("EditorSubsystem is not available in the world.");

            _editorSubsystem = editorSubsystem;

            ContentBrowser = new ContentBrowserViewModel(_editorSubsystem);
            ContentBrowser.LoadPackages();
            OnPropertyChanged(nameof(ContentBrowser));
            
            HandleCurrentProjectChanged(_editorSubsystem.CurrentProject);

            EditorState editorState = EditorState.Instance;

            _currentProjectChangedHandler?.Remove();
            _currentProjectChangedHandler = editorState.GetOnCurrentProjectChangedDelegate()
                .Bind(HandleCurrentProjectChanged);

            // handle active scene changes
            _activeSceneChangedHandler?.Remove();
            _activeSceneChangedHandler = _editorSubsystem.GetOnActiveSceneChangedDelegate()
                .Bind(HandleActiveSceneChanged);

            SceneHierarchy.SelectedNodeChanged += OnSceneHierarchyNodeSelected;

            EngineManager.SceneAdded += OnSceneAdded;
            EngineManager.SceneRemoved += OnSceneRemoved;

            BindFocusedNodeChanged();

            EngineManager.TaskStarted += OnTaskStarted;
            EngineManager.TaskEnded += OnTaskEnded;
            EngineManager.TaskProgressUpdated += OnTaskProgressUpdated;

            _ = EngineManager.PostToSimThread(() =>
            {
                Scene? activeScene = _editorSubsystem.GetActiveScene();
                Node? focusedNode = _editorSubsystem.GetFocusedNode();

                HandleActiveSceneChanged(activeScene);
                HandleFocusedNodeUpdate(focusedNode);
            });

            _isReady = true;
        }

        public void Dispose()
        {
            EngineManager.SceneAdded -= OnSceneAdded;
            EngineManager.SceneRemoved -= OnSceneRemoved;
            EngineManager.TaskStarted -= OnTaskStarted;
            EngineManager.TaskEnded -= OnTaskEnded;
            EngineManager.TaskProgressUpdated -= OnTaskProgressUpdated;

            _gameModeChangedHandler?.Remove();
            _focusedNodeChangedHandler?.Remove();
            _currentProjectChangedHandler?.Remove();
            _selectedGizmoChangedHandler?.Remove();
            _activeSceneChangedHandler?.Remove();
            _actionStackStateChangedHandler?.Remove();
            SceneHierarchy.SelectedNodeChanged -= OnSceneHierarchyNodeSelected;
            ContentBrowser.Dispose();
        }

        private void OnTaskStarted(EditorTaskBase task, bool isForegroundTask)
        {
            Dispatcher.UIThread.Post(() =>
            {
                if (isForegroundTask)
                {
                    ForegroundTask.SetTask(task);
                }
                else
                {
                    if (Tasks.Any(t => t.TaskId.Equals(task.Id)))
                    {
                        return;
                    }

                    Tasks.Add(new TaskItemViewModel(task.Id, task.Class.Name.ToString(), isForegroundTask: false));
                    OnPropertyChanged(nameof(Tasks));
                }
            });
        }

        private void OnTaskEnded(ObjIdBase taskId)
        {
            Dispatcher.UIThread.Post(() =>
            {
                ForegroundTask.Remove(taskId);

                for (int i = 0; i < Tasks.Count; i++)
                {
                    if (Tasks[i].TaskId.Equals(taskId))
                    {
                        Tasks.RemoveAt(i);
                        OnPropertyChanged(nameof(Tasks));
                        return;
                    }
                }
            });
        }

        private void OnTaskProgressUpdated(ObjIdBase taskId, float progress)
        {
            Dispatcher.UIThread.Post(() =>
            {
                // Update foreground task
                ForegroundTask.UpdateProgress(taskId, progress);

                // Also check background tasks
                foreach (TaskItemViewModel task in Tasks)
                {
                    if (task.TaskId.Equals(taskId))
                    {
                        task.Progress = progress;
                        return;
                    }
                }
            });
        }

        private void HandleActiveSceneChanged(Scene? scene)
        {
            Dispatcher.UIThread.Post(() =>
            {
                if (!_isReady)
                {
                    return;
                }

                // only attach scenes that have the FOREGROUND flag
                if (scene == null || !scene.SceneFlags.HasFlag(SceneFlags.Foreground))
                {
                    ActiveScene = null;
                    return;
                }

                _activeScene = Scenes
                    .Where(s => s.Scene == scene)
                    .FirstOrDefault();
                
                if (_activeScene == null)
                {
                    _activeScene = new SceneViewModel(scene, isActive: true);
                    Scenes.Add(_activeScene);

                    OnPropertyChanged(nameof(Scenes));
                }

                SceneHierarchy.AttachToScene(scene);

                OnPropertyChanged(nameof(ActiveScene));
            });
        }

        private void UpdateUndoRedoHeaders(EditorProject? project)
        {
            if (project == null)
            {
                UndoHeader = "_Undo";
                RedoHeader = "_Redo";
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                EditorActionBase? undoAction = project.ActionStack.GetUndoAction();
                EditorActionBase? redoAction = project.ActionStack.GetRedoAction();
                string? undoName = undoAction?.GetText();
                string? redoName = redoAction?.GetText();

                Dispatcher.UIThread.Post(() =>
                {
                    UndoHeader = string.IsNullOrEmpty(undoName) ? "_Undo" : $"_Undo {undoName}";
                    RedoHeader = string.IsNullOrEmpty(redoName) ? "_Redo" : $"_Redo {redoName}";
                });
            });
        }

        private void HandleCurrentProjectChanged(EditorProject? project)
        {
            // Game mode:  when project changes we also want to update the play/pause/stop buttons
            _gameModeChangedHandler?.Remove();

            _actionStackStateChangedHandler?.Remove();
            _actionStackStateChangedHandler = null;

            if (project != null)
            {
                _actionStackStateChangedHandler = project.ActionStack.GetOnStateChangeDelegate()
                    .Bind((EditorActionStackState state, int undoDepth) => UpdateUndoRedoHeaders(project));
            }

            UpdateUndoRedoHeaders(project);

            if (project != null)
            {
                _gameModeChangedHandler = project.GameInstance.GetOnGameStateChangeDelegate()
                    .Bind((Game game, GameStateMode newMode, GameStateMode prevMode) =>
                    {
                        Dispatcher.UIThread.Post(() =>
                        {
                            (SetGameModePlaying as SetGameModeCommand)?.RaiseCanExecuteChanged();
                            (SetGameModeStopped as SetGameModeCommand)?.RaiseCanExecuteChanged();
                            (SetGameModePaused as SetGameModeCommand)?.RaiseCanExecuteChanged();
                            OnPropertyChanged(nameof(CanSetGameModePlaying));
                            OnPropertyChanged(nameof(CanSetGameModePaused));
                            OnPropertyChanged(nameof(CanSetGameModeStopped));

                            (SelectTransformModeTranslate as SetGizmoCommand)?.RaiseCanExecuteChanged();
                            (SelectTransformModeRotate as SetGizmoCommand)?.RaiseCanExecuteChanged();
                            (SelectTransformModeScale as SetGizmoCommand)?.RaiseCanExecuteChanged();

                            OnPropertyChanged(nameof(CanSelectGizmo));
                            OnPropertyChanged(nameof(CanSelectTransformModeTranslate));
                            OnPropertyChanged(nameof(CanSelectTransformModeRotate));
                            OnPropertyChanged(nameof(CanSelectTransformModeScale));
                        });
                    });
            }

            _selectedGizmoChangedHandler?.Remove();
            _selectedGizmoChangedHandler = _editorSubsystem.GetOnSelectedGizmoChangedDelegate()
                .Bind((EditorGizmoBase? newGizmo, EditorGizmoBase? prevGizmo) =>
                {
                    Dispatcher.UIThread.Post(() =>
                    {
                        (SelectTransformModeTranslate as SetGizmoCommand)?.RaiseCanExecuteChanged();
                        (SelectTransformModeRotate as SetGizmoCommand)?.RaiseCanExecuteChanged();
                        (SelectTransformModeScale as SetGizmoCommand)?.RaiseCanExecuteChanged();

                        OnPropertyChanged(nameof(CanSelectGizmo));
                        OnPropertyChanged(nameof(CanSelectTransformModeTranslate));
                        OnPropertyChanged(nameof(CanSelectTransformModeRotate));
                        OnPropertyChanged(nameof(CanSelectTransformModeScale));
                    });
                });

            Dispatcher.UIThread.Post(() =>
            {
                OnPropertyChanged(nameof(CanSetGameModePlaying));
                OnPropertyChanged(nameof(CanSetGameModePaused));
                OnPropertyChanged(nameof(CanSetGameModeStopped));

                OnPropertyChanged(nameof(CanSelectGizmo));
                OnPropertyChanged(nameof(CanSelectTransformModeTranslate));
                OnPropertyChanged(nameof(CanSelectTransformModeRotate));
                OnPropertyChanged(nameof(CanSelectTransformModeScale));

                // Update scenes list
                Scenes.Clear();

                if (project != null)
                {
                    foreach (Scene scene in project.World.GetScenes())
                    {
                        // ONLY add scenes that have FOREGROUND flag.
                        if (!scene.SceneFlags.HasFlag(SceneFlags.Foreground))
                        {
                            continue;
                        }

                        Scenes.Add(new SceneViewModel(scene, isActive: _activeScene?.Scene?.Id == scene.Id));
                    }
                }

                OnPropertyChanged(nameof(Scenes));
            });
        }

        private void OnSceneHierarchyNodeSelected(Node? node)
        {
            if (!_isReady || _isUpdatingSelectionFromEngine != 0)
            {
                return;
            }

            CanAddInstance = node is Entity;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    _editorSubsystem.SetFocusedNode(node!, false);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Failed to set focused node: {ex.Message}");
                }
            });

            bool isRootNode = SceneHierarchy.IsRootNode(node);
            Inspector.SetSelectedNode(node, SceneHierarchy.Scene, isRootNode);
        }

        private void OnSceneAdded(World world, Scene scene)
        {
            Action action = () =>
            {
                // we only want scenes that have the FOREGROUND flag.
                if (_activeScene != null && _activeScene.Scene.SceneFlags.HasFlag(SceneFlags.Foreground))
                {
                    foreach (SceneViewModel svm in Scenes)
                    {
                        if (svm.Scene.Id == scene.Id)
                        {
                            return; // already exists
                        }
                    }
                    
                    Scenes.Add(new SceneViewModel(scene, isActive: _activeScene.Scene?.Id == scene.Id));

                    OnPropertyChanged(nameof(Scenes));
                }
            };

            if (Dispatcher.UIThread.CheckAccess())
            {
                action();

                return;
            }

            Dispatcher.UIThread.Post(action);
        }

        private void OnSceneRemoved(World world, Scene scene)
        {
            Action action = () =>
            {
                for (int i = 0; i < Scenes.Count; i++)
                {
                    if (Scenes[i].Scene.Id == scene.Id)
                    {
                        Scenes.RemoveAt(i);

                        OnPropertyChanged(nameof(Scenes));

                        return;
                    }
                }
            };

            if (Dispatcher.UIThread.CheckAccess())
            {
                action();

                return;
            }

            Dispatcher.UIThread.Post(action);
        }

        private void BindFocusedNodeChanged()
        {
            WeakReference<MainWindowViewModel> weakThis = new WeakReference<MainWindowViewModel>(this);

            _focusedNodeChangedHandler?.Remove();

            _focusedNodeChangedHandler = _editorSubsystem.GetOnFocusedNodeChangedDelegate()
                .Bind((Node newNode, Node prevNode, bool shouldSelectInOutline) =>
                {
                    if (!weakThis.TryGetTarget(out MainWindowViewModel? target))
                    {
                        return;
                    }

                    target.HandleFocusedNodeUpdate(newNode);
                });
        }

        private void HandleFocusedNodeUpdate(Node? node)
        {
            if (Interlocked.CompareExchange(ref _isUpdatingSelectionFromEngine, 1, 0) != 0)
            {
                return;
            }

            Dispatcher.UIThread.Post(() =>
            {
                if (!_isReady)
                {
                    Interlocked.Exchange(ref _isUpdatingSelectionFromEngine, 0);
                    return;
                }

                try
                {
                    Node? validNode = node != null && node.IsValid ? node : null;

                    bool isRootNode = SceneHierarchy.IsRootNode(validNode);
                    Inspector.SetSelectedNode(validNode, SceneHierarchy.Scene, isRootNode);
                    SceneHierarchy.SelectNodeFromEngine(validNode);

                    // can ONLY add Instanced Mesh Proxy child objects instances to entities that have a MeshComponent.
                    // Note that for now the most derived class MUST be EQUAL to Entity (not just derived from it)
                    // as currently we only support adding instances to entities, not to other node types (e.g. we don't support adding instances to a Light)
                    CanAddInstance = validNode != null
                        && validNode.GetType() == typeof(Entity);
                        //&& ((Entity)validNode).HasComponent<MeshComponent>();
                }
                finally
                {
                    Interlocked.Exchange(ref _isUpdatingSelectionFromEngine, 0);
                }
            });
        }
    }
}