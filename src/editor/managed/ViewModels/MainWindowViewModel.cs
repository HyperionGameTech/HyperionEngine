using System;
using System.Diagnostics;
using System.Threading;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;

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
        public EditorCommand Exit => new EditorCommand("Exit");
        public EditorCommand Undo => new EditorCommand("Undo");
        public EditorCommand Redo => new EditorCommand("Redo");
        public EditorCommand Copy => new EditorCommand("Copy");
        public EditorCommand Paste => new EditorCommand("Paste");

        public EditorCommand AddEmptyNode => new EditorCommand("AddEmptyNode");
        public EditorCommand AddEntity => new EditorCommand("AddEntity");
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
        public bool CanSelectGizmo
        {
            get
            {
                return _editorSubsystem != null
                    && EngineManager.GameInstance?.World?.GetGameState().Mode == GameStateMode.EditMode;
            }
        }

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

        private int _isUpdatingSelectionFromEngine = 0; // atomic
        private bool _isReady = false;

        private EditorSubsystem _editorSubsystem;
    
        private List<SceneViewModel> _scenes = new List<SceneViewModel>();
        public List<SceneViewModel> Scenes => _scenes;
        
        private SceneViewModel? _activeScene;
        public SceneViewModel? ActiveScene
        {
            get => _activeScene;
            set
            {
                Logger.Log(LogType.Info, $"Setting ActiveScene to {(value != null ? value.Scene.Name.ToString() : "null")}");
                if (_activeScene == value)
                    return;

                _activeScene = value;

                _ = EngineManager.PostToSimThread(() =>
                {
                    try
                    {
                        _editorSubsystem.SetActiveScene(_activeScene?.Scene);
                    }
                    catch (Exception ex)
                    {
                        Logger.Log(LogType.Warn, $"Failed to set active scene: {ex.Message}");
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

            HyperionEditorGame? editorGame = EngineManager.EditorGame;
            if (editorGame == null)
                throw new InvalidOperationException("Editor game instance is not initialized.");

            if (editorGame.IsLaunched())
            {
                Init(editorGame);

                return;
            }

            Logger.Log(LogType.Info, "Game instance not yet launched, setting up callback to be notified when ready");

            _gameInstanceLaunchedHandler = editorGame.GetOnLaunchedDelegate().Bind(() =>
            {
                Dispatcher.UIThread.Post(() =>
                {
                    Init(editorGame);
                });
            });

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
        }

        private void Init(HyperionEditorGame editorGame)
        {
            Dispatcher.UIThread.CheckAccess();

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

            _currentProjectChangedHandler = editorState.GetOnCurrentProjectChangedDelegate()
                .Bind(HandleCurrentProjectChanged);

            // handle active scene changes
            _activeSceneChangedHandler = _editorSubsystem.GetOnActiveSceneChangedDelegate()
                .Bind(HandleActiveSceneChanged);

            SceneHierarchy.SelectedNodeChanged += OnSceneHierarchyNodeSelected;

            BindFocusedNodeChanged();

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
            _gameModeChangedHandler?.Remove();
            _focusedNodeChangedHandler?.Remove();
            _currentProjectChangedHandler?.Remove();
            _selectedGizmoChangedHandler?.Remove();
            _activeSceneChangedHandler?.Remove();
            SceneHierarchy.SelectedNodeChanged -= OnSceneHierarchyNodeSelected;
            ContentBrowser.Dispose();
        }

        private void HandleActiveSceneChanged(Scene? scene)
        {
            Dispatcher.UIThread.Invoke(() =>
            {
                if (!_isReady)
                {
                    return;
                }

                if (scene == null)
                {
                    ActiveScene = null;
                    return;
                }

                _activeScene = _scenes.Find(s => s.Scene == scene);
                
                if (_activeScene == null)
                {
                    _activeScene = new SceneViewModel(scene, isActive: true);
                    _scenes.Add(_activeScene);

                    OnPropertyChanged(nameof(Scenes));
                }

                SceneHierarchy.AttachToScene(scene);

                OnPropertyChanged(nameof(ActiveScene));
            });
        }

        private void HandleCurrentProjectChanged(EditorProject? project)
        {
            // Game mode:  when project changes we also want to update the play/pause/stop buttons
            _gameModeChangedHandler?.Remove();

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
                    });
                });

            OnPropertyChanged(nameof(CanSetGameModePlaying));
            OnPropertyChanged(nameof(CanSetGameModePaused));
            OnPropertyChanged(nameof(CanSetGameModeStopped));

            OnPropertyChanged(nameof(CanSelectGizmo));

            // Update scenes list
            _scenes.Clear();

            if (project != null)
            {
                foreach (Scene scene in project.World.GetScenes())
                {
                    _scenes.Add(new SceneViewModel(scene, isActive: _activeScene?.Scene?.Id == scene.Id));
                }
            }

            OnPropertyChanged(nameof(Scenes));
        }

        private void OnSceneHierarchyNodeSelected(Node? node)
        {
            if (!_isReady || _isUpdatingSelectionFromEngine != 0)
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    _editorSubsystem.SetFocusedNode(node!, false);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogType.Warn, $"Failed to set focused node: {ex.Message}");
                }
            });

            bool isRootNode = SceneHierarchy.IsRootNode(node);
            Inspector.SetSelectedNode(node, SceneHierarchy.Scene, isRootNode);
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
                }
                finally
                {
                    Interlocked.Exchange(ref _isUpdatingSelectionFromEngine, 0);
                }
            });
        }
    }
}