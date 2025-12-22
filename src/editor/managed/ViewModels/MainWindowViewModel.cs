using System;
using System.Diagnostics;
using System.Threading;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class MainWindowViewModel : ViewModelBase, IDisposable
    {
        public class EditorCommand : ICommand
        {
            private string _name;
            
            public EditorCommand(string name)
            {
                _name = name;
            }

            public bool CanExecute(object? parameter) => !string.IsNullOrEmpty(_name);
            public void Execute(object? parameter) => EngineManager.EditorGame?.EditorSubsystem?.ExecuteCommandByName(new Name("EditorCommand" + _name));
            public event EventHandler? CanExecuteChanged;
            public void RaiseCanExecuteChanged() => CanExecuteChanged?.Invoke(this, EventArgs.Empty);
        }

        private class SetGizmoCommand : ICommand
        {
            private EditorManipulationMode _mode;

            public SetGizmoCommand(EditorManipulationMode mode)
            {
                _mode = mode;
            }

            public bool CanExecute(object? parameter) => true; // dont check as it needs to be called on the sim thread

            public void Execute(object? parameter)
            {
                EditorSubsystem? editorSubsystem = EngineManager.EditorGame?.EditorSubsystem;
                Debug.Assert(editorSubsystem != null);

                _ = EngineManager.PostToSimThread(() => editorSubsystem.SetSelectedManipulationMode(_mode));
            }

            public event EventHandler? CanExecuteChanged;

            public void RaiseCanExecuteChanged() => CanExecuteChanged?.Invoke(this, EventArgs.Empty);
        }

        private class SetGameModeCommand : ICommand
        {
            private static int _isChangingGameMode = 0;
            private GameStateMode _mode;

            public SetGameModeCommand(GameStateMode mode)
            {
                _mode = mode;
            }

            public bool CanExecute(object? parameter) => true; // TEMP : debug
                // EngineManager.GameInstance?.World?.GetGameState().Mode != _mode
                //     && Interlocked.CompareExchange(ref _isChangingGameMode, 0, 0) == 0;

            public void Execute(object? parameter)
            {
                if (Interlocked.CompareExchange(ref _isChangingGameMode, 1, 0) != 0)
                {
                    Logger.Log(LogType.Warn, "Cannot set game mode; already setting");
                    return;
                }

                Game? currentGameInstance = EngineManager.GameInstance;
                Debug.Assert(currentGameInstance != null);

                Game? gameInstance = currentGameInstance;

                switch (_mode)
                {
                    case GameStateMode.Simulating:
                    {
                        if (currentGameInstance is HyperionEditorGame hyperionEditorGame)
                        {
                            gameInstance = hyperionEditorGame.EditorSubsystem?.CurrentProject?.GameInstance;
                            Debug.Assert(gameInstance != null, "Failed to get game instance from current project");
                        }
                        else
                        {
                            throw new InvalidOperationException("Cannot enter Simulating mode when game instance is not HyperionEditorGame");
                        }

                        EngineManager.InitializeGame(gameInstance);

                        _ = EngineManager.PostToSimThread(() =>
                        {
                            try
                            {
                                gameInstance.World.StartSimulating();
                            }
                            finally
                            {
                                Interlocked.Exchange(ref _isChangingGameMode, 0);
                            }
                        });

                        break;
                    }
                    case GameStateMode.Paused:
                        _ = EngineManager.PostToSimThread(() =>
                        {
                            try
                            {
                                gameInstance.World.PauseSimulation();
                            }
                            finally
                            {
                                Interlocked.Exchange(ref _isChangingGameMode, 0);
                            }
                        });

                        break;
                    case GameStateMode.Stopped:
                        _ = EngineManager.PostToSimThread(() =>
                        {
                            gameInstance.World.StopSimulating();

                            Dispatcher.UIThread.Post(() =>
                            {
                                try
                                {
                                    EngineManager.InitializeEditor();
                                }
                                finally
                                {
                                    Interlocked.Exchange(ref _isChangingGameMode, 0);
                                }
                            });
                        });
                        break;
                    default:
                        throw new NotImplementedException();
                }
            }

            public event EventHandler? CanExecuteChanged;

            public void RaiseCanExecuteChanged() => CanExecuteChanged?.Invoke(this, EventArgs.Empty);
        }

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

        public ICommand SelectTranslateGizmo { get; private set; }
        public ICommand SelectRotateGizmo { get; private set; }
        public ICommand SelectScaleGizmo { get; private set; }
        public bool CanSelectGizmo
        {
            get
            {
                return _editorSubsystem != null
                    && EngineManager.GameInstance?.World?.GetGameState().Mode == GameStateMode.Editor;
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

                return project.World.GetGameState().Mode != GameStateMode.Editor;
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

        private const int GameLaunchWaitIntervalMs = 500;
        private const int MaxGameLaunchWaitTimeMs = 60000; // max before giving up

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

            SelectTranslateGizmo = new SetGizmoCommand(EditorManipulationMode.Translate);
            SelectRotateGizmo = new SetGizmoCommand(EditorManipulationMode.Rotate);
            SelectScaleGizmo = new SetGizmoCommand(EditorManipulationMode.Scale);

            SetGameModePlaying = new SetGameModeCommand(GameStateMode.Simulating);
            SetGameModePaused = new SetGameModeCommand(GameStateMode.Paused);
            SetGameModeStopped = new SetGameModeCommand(GameStateMode.Stopped);
        }

        private void Init(HyperionEditorGame editorGame)
        {
            World? world = editorGame.World;
            if (world == null)
                throw new InvalidOperationException("Editor world is not initialized.");

            EditorSubsystem? editorSubsystem = world.GetSubsystem<EditorSubsystem>();
            if (editorSubsystem == null)
                throw new InvalidOperationException("EditorSubsystem is not available in the world.");

            _editorSubsystem = editorSubsystem;

            ContentBrowser = new ContentBrowserViewModel(_editorSubsystem);
            ContentBrowser.LoadPackages();

            Action<EditorProject?> setGameModeChangedHandler = (EditorProject? project) =>
            {
                _selectedGizmoChangedHandler?.Remove();
                _gameModeChangedHandler?.Remove();

                if (project != null)
                {
                    _gameModeChangedHandler = project.World.GetOnGameStateChangeDelegate()
                        .Bind((World world, GameStateMode newMode, GameStateMode prevMode) =>
                        {
                            Dispatcher.UIThread.Post(() =>
                            {
                                (SetGameModePlaying as SetGameModeCommand)?.RaiseCanExecuteChanged();
                                (SetGameModeStopped as SetGameModeCommand)?.RaiseCanExecuteChanged();
                                (SetGameModePaused as SetGameModeCommand)?.RaiseCanExecuteChanged();
                                OnPropertyChanged(nameof(CanSetGameModePlaying));
                                OnPropertyChanged(nameof(CanSetGameModePaused));
                                OnPropertyChanged(nameof(CanSetGameModeStopped));

                                (SelectTranslateGizmo as SetGizmoCommand)?.RaiseCanExecuteChanged();
                                (SelectRotateGizmo as SetGizmoCommand)?.RaiseCanExecuteChanged();
                                (SelectScaleGizmo as SetGizmoCommand)?.RaiseCanExecuteChanged();
                                OnPropertyChanged(nameof(CanSelectGizmo));
                            });
                        });
                }

                _selectedGizmoChangedHandler = _editorSubsystem.GetOnSelectedGizmoChangedDelegate()
                    .Bind((EditorGizmoBase? newGizmo, EditorGizmoBase? prevGizmo) =>
                    {
                        Dispatcher.UIThread.Post(() =>
                        {
                            (SelectTranslateGizmo as SetGizmoCommand)?.RaiseCanExecuteChanged();
                            (SelectRotateGizmo as SetGizmoCommand)?.RaiseCanExecuteChanged();
                            (SelectScaleGizmo as SetGizmoCommand)?.RaiseCanExecuteChanged();

                            OnPropertyChanged(nameof(CanSelectGizmo));
                        });
                    });

                OnPropertyChanged(nameof(CanSetGameModePlaying));
                OnPropertyChanged(nameof(CanSetGameModePaused));
                OnPropertyChanged(nameof(CanSetGameModeStopped));

                OnPropertyChanged(nameof(CanSelectGizmo));
            };

            setGameModeChangedHandler(EngineManager.CurrentProject);

            EditorState editorState = EditorState.Instance;

            // when project changes we also want to update the play/pause/stop buttons
            _currentProjectChangedHandler = editorState.GetOnCurrentProjectChangedDelegate()
                .Bind(setGameModeChangedHandler);

            // handle active scene changes
            _activeSceneChangedHandler = _editorSubsystem.GetOnActiveSceneChangedDelegate()
                .Bind(HandleActiveSceneChanged);

            SceneHierarchy.SelectedNodeChanged += OnSceneHierarchyNodeSelected;

            BindFocusedNodeChanged();

            _ = EngineManager.PostToSimThread(() =>
            {
                Scene? activeScene = _editorSubsystem.GetActiveScene();
                Node? focusedNode = _editorSubsystem.GetFocusedNode();

                Dispatcher.UIThread.Post(() =>
                {
                    SceneHierarchy.AttachToScene(activeScene);
                });

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

                SceneHierarchy.AttachToScene(scene);
            });
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

            Inspector.SetSelectedNode(node);
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

                    Inspector.SetSelectedNode(validNode);
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