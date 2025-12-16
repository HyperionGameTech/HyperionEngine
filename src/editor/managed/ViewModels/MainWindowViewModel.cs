using System;
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
            public void Execute(object? parameter) => EngineManager.GameInstance?.EditorSubsystem?.ExecuteCommandByName(new Name("EditorCommand" + _name));
            public event EventHandler? CanExecuteChanged;
            public void RaiseCanExecuteChanged() => CanExecuteChanged?.Invoke(this, EventArgs.Empty);
        }

        private class SetGizmoCommand : ICommand
        {
            private readonly EditorSubsystem _editorSubsystem;
            private readonly EditorManipulationMode _mode;

            public SetGizmoCommand(EditorSubsystem editorSubsystem, EditorManipulationMode mode)
            {
                _editorSubsystem = editorSubsystem;
                _mode = mode;
            }

            public bool CanExecute(object? parameter) => _editorSubsystem != null;

            public void Execute(object? parameter)
            {
                _ = EngineManager.PostToGameThread(() => _editorSubsystem.SetSelectedManipulationMode(_mode));
            }

            public event EventHandler? CanExecuteChanged;

            public void RaiseCanExecuteChanged() => CanExecuteChanged?.Invoke(this, EventArgs.Empty);
        }

        private class SetGameModeCommand : ICommand
        {
            private readonly EditorSubsystem _editorSubsystem;
            private readonly GameStateMode _mode;

            public SetGameModeCommand(EditorSubsystem editorSubsystem, GameStateMode mode)
            {
                _editorSubsystem = editorSubsystem;
                _mode = mode;
            }

            public bool CanExecute(object? parameter) => EngineManager.CurrentProject?.World != null;

            public void Execute(object? parameter)
            {
                World? world = EngineManager.CurrentProject?.World;
                if (world == null)
                    throw new NullReferenceException("Expected World to not be null when executing SetGameModeCommand!");

                switch (_mode)
                {
                    case GameStateMode.Simulating:
                        _ = EngineManager.PostToGameThread(world.StartSimulating);
                        break;
                    case GameStateMode.Paused:
                        _ = EngineManager.PostToGameThread(world.PauseSimulation);
                        break;
                    case GameStateMode.Stopped:
                        _ = EngineManager.PostToGameThread(world.StopSimulating);
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
        public SceneHierarchyViewModel SceneHierarchy { get; }
        public InspectorViewModel Inspector { get; }
        public ContentBrowserViewModel ContentBrowser { get; }

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

        public ICommand SelectTranslateGizmo { get; }
        public ICommand SelectRotateGizmo { get; }
        public ICommand SelectScaleGizmo { get; }

        public ICommand SetGameModePlaying { get; }
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

        public ICommand SetGameModePaused { get; }
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

        public ICommand SetGameModeStopped { get; }
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

        private DelegateHandler? _gameModeChangedHandler;
        private DelegateHandler? _focusedNodeChangedHandler;
        private DelegateHandler? _currentProjectChangedHandler;
        private DelegateHandler? _selectedGizmoChangedHandler;
        private DelegateHandler? _activeSceneChangedHandler;

        private bool _isUpdatingSelectionFromEngine;

        private readonly EditorSubsystem _editorSubsystem;

        private const int GameLaunchWaitIntervalMs = 500;
        private const int MaxGameLaunchWaitTimeMs = 60000; // max before giving up

        public MainWindowViewModel()
        {
            SceneHierarchy = new SceneHierarchyViewModel();
            Inspector = new InspectorViewModel();

            Game? gameInstance = EngineManager.GameInstance;
            if (gameInstance == null)
                throw new InvalidOperationException("Game instance is not initialized.");

            int waitedTime = 0;

            while (!gameInstance.IsLaunched())
            {
                Logger.Log(LogType.Info, "Waiting for game to launch...");

                Thread.Sleep(GameLaunchWaitIntervalMs);

                waitedTime += GameLaunchWaitIntervalMs;

                if (waitedTime >= MaxGameLaunchWaitTimeMs)
                {
                    throw new TimeoutException("Timed out waiting for game to launch!");
                }
            }

            World? world = gameInstance.World;
            if (world == null)
                throw new InvalidOperationException("Game world is not initialized.");

            EditorSubsystem? editorSubsystem = world.GetSubsystem<EditorSubsystem>();
            if (editorSubsystem == null)
                throw new InvalidOperationException("EditorSubsystem is not available in the world.");

            _editorSubsystem = editorSubsystem;

            ContentBrowser = new ContentBrowserViewModel(_editorSubsystem);
            ContentBrowser.LoadPackages();

            SelectTranslateGizmo = new SetGizmoCommand(_editorSubsystem, EditorManipulationMode.Translate);
            SelectRotateGizmo = new SetGizmoCommand(_editorSubsystem, EditorManipulationMode.Rotate);
            SelectScaleGizmo = new SetGizmoCommand(_editorSubsystem, EditorManipulationMode.Scale);

            SetGameModePlaying = new SetGameModeCommand(_editorSubsystem, GameStateMode.Simulating);
            SetGameModePaused = new SetGameModeCommand(_editorSubsystem, GameStateMode.Paused);
            SetGameModeStopped = new SetGameModeCommand(_editorSubsystem, GameStateMode.Stopped);

            Action<EditorProject?> setGameModeChangedHandler = (EditorProject? project) =>
            {
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
                            });
                        });
                }
            };

            setGameModeChangedHandler(EngineManager.CurrentProject);

            EditorState editorState = EditorState.Instance;
            _currentProjectChangedHandler = editorState.GetOnCurrentProjectChangedDelegate()
                .Bind(setGameModeChangedHandler);

            _selectedGizmoChangedHandler = _editorSubsystem.GetOnSelectedGizmoChangedDelegate()
                .Bind((EditorGizmoBase? newGizmo, EditorGizmoBase? prevGizmo) =>
                {
                    Dispatcher.UIThread.Post(() =>
                    {
                        (SelectTranslateGizmo as SetGizmoCommand)?.RaiseCanExecuteChanged();
                        (SelectRotateGizmo as SetGizmoCommand)?.RaiseCanExecuteChanged();
                        (SelectScaleGizmo as SetGizmoCommand)?.RaiseCanExecuteChanged();
                    });
                });

            // handle active scene changes
            _activeSceneChangedHandler = _editorSubsystem.GetOnActiveSceneChangedDelegate()
                .Bind(HandleActiveSceneChanged);

            SceneHierarchy.SelectedNodeChanged += OnSceneHierarchyNodeSelected;

            BindFocusedNodeChanged();

            _ = EngineManager.PostToGameThread(() =>
            {
                Scene? activeScene = _editorSubsystem.GetActiveScene();
                Node? focusedNode = _editorSubsystem.GetFocusedNode();

                Dispatcher.UIThread.Post(() =>
                {
                    SceneHierarchy.AttachToScene(activeScene);
                });

                HandleFocusedNodeUpdate(focusedNode);
            });
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
                SceneHierarchy.AttachToScene(scene);
            });
        }

        private void OnSceneHierarchyNodeSelected(Node? node)
        {
            if (_isUpdatingSelectionFromEngine)
            {
                return;
            }

            _ = EngineManager.PostToGameThread(() =>
            {
                try
                {
                    _editorSubsystem.SetFocusedNode(node!, false);
                    Inspector.SetSelectedNode(node);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogType.Warn, $"Failed to set focused node: {ex.Message}");
                }
            });
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
            _isUpdatingSelectionFromEngine = true;

            try
            {
                Node? validNode = node != null && node.IsValid ? node : null;

                Inspector.SetSelectedNode(validNode);
                SceneHierarchy.SelectNodeFromEngine(validNode);
            }
            finally
            {
                _isUpdatingSelectionFromEngine = false;
            }
        }
    }
}