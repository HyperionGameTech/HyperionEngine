using Avalonia.Threading;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Hyperion;
using Hyperion.Editor.Commands;
using Hyperion.Editor.Services;
using Hyperion.Editor.Views;
using System;
using System.Collections.Generic;
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

        public EditorPanelViewModel? ActivePanel => PanelService.Instance.ActivePanel;

        public ICommand ClosePanelCommand => PanelService.Instance.CloseCommand;

        public EditorCommand NewProject => new EditorCommand("NewProject");
        public EditorCommand OpenProject => new EditorCommand("OpenProject");
        public EditorCommand SaveProject => new EditorCommand("SaveProject");
        public EditorCommand SaveProjectAs => new EditorCommand("SaveProjectAs");
        public ICommand Exit { get; } = new RelayCommand(() =>
        {
            if (Avalonia.Application.Current?.ApplicationLifetime is Avalonia.Controls.ApplicationLifetimes.IClassicDesktopStyleApplicationLifetime lifetime)
            {
                lifetime.MainWindow?.Close();
            }
        });
        public EditorCommand Undo => new EditorCommand("Undo");
        public EditorCommand Redo => new EditorCommand("Redo");
        public EditorCommand SelectAll => new EditorCommand("SelectAll");

        public EditorCommand BuildLightmaps => new EditorCommand("BuildLightmaps");
        public EditorCommand BuildReflectionProbes => new EditorCommand("BuildReflectionProbes");
        public EditorCommand BuildIrradianceProbes => new EditorCommand("BuildIrradianceProbes");
        public EditorCommand BuildBentNormals => new EditorCommand("BuildBentNormals");

        public EditorCommand CookGameContent => new EditorCommand("CookGameContent");

        private string _undoHeader = "_Undo";
        public string UndoHeader
        {
            get => _undoHeader;
            set => SetProperty(ref _undoHeader, value);
        }

        private bool _canUndo;
        public bool CanUndo
        {
            get => _canUndo;
            set => SetProperty(ref _canUndo, value);
        }

        private string _redoHeader = "_Redo";
        public string RedoHeader
        {
            get => _redoHeader;
            set => SetProperty(ref _redoHeader, value);
        }

        private bool _canRedo;
        public bool CanRedo
        {
            get => _canRedo;
            set => SetProperty(ref _canRedo, value);
        }

        private string _pasteHeader = "_Paste";
        public string PasteHeader
        {
            get => _pasteHeader;
            set => SetProperty(ref _pasteHeader, value);
        }

        private bool _canPaste;
        public bool CanPaste
        {
            get => _canPaste;
            set => SetProperty(ref _canPaste, value);
        }

        private string _copyHeader = "_Copy";
        public string CopyHeader
        {
            get => _copyHeader;
            set => SetProperty(ref _copyHeader, value);
        }

        private bool _canCopy;
        public bool CanCopy
        {
            get => _canCopy;
            set => SetProperty(ref _canCopy, value);
        }

        private string _deleteHeader = "_Delete";
        public string DeleteHeader
        {
            get => _deleteHeader;
            set => SetProperty(ref _deleteHeader, value);
        }

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

        public EditorCommand AddSprite => new EditorCommand("AddSprite");
        public EditorCommand AddTextSprite => new EditorCommand("AddTextSprite");

        public EditorCommand AddPointLight => new EditorCommand("AddPointLight");
        public EditorCommand AddDirectionalLight => new EditorCommand("AddDirectionalLight");
        public EditorCommand AddSpotLight => new EditorCommand("AddSpotLight");
        public EditorCommand AddAreaRectLight => new EditorCommand("AddAreaRectLight");

        public EditorCommand AddReflectionProbe => new EditorCommand("AddReflectionProbe");
        public EditorCommand AddIrradianceProbe => new EditorCommand("AddIrradianceProbe");
        public EditorCommand AddLightmapVolume => new EditorCommand("AddLightmapVolume");
        public EditorCommand AddProbeVolume => new EditorCommand("AddProbeVolume");
        public EditorCommand AddParticleVolume => new EditorCommand("AddParticleVolume");
        public EditorCommand AddFogVolume => new EditorCommand("AddFogVolume");

        public EditorCommand NewScript => new EditorCommand("NewScript");

        // Shapes
        public EditorCommand AddPlane => new EditorCommand("AddPlane");
        public EditorCommand AddCube => new EditorCommand("AddCube");
        public ICommand AddNormalizedCubeSphereCommand { get; private set; }
        public EditorCommand AddCylinder => new EditorCommand("AddCylinder");

        private string GetSelectedNodeName() => SceneHierarchy.SelectedNode?.Node?.Name.ToString() ?? string.Empty;

        public EditorCommand DeleteNode => new EditorCommand("DeleteNode");
        public EditorCommand Delete => new EditorCommand("DeleteNode");
        public EditorCommand TeleportToNode => new EditorCommand("TeleportTo", GetSelectedNodeName);
        public EditorCommand Copy => new EditorCommand("Copy");
        public EditorCommand Paste => new EditorCommand("Paste");

        public ICommand SelectTransformModeTranslate { get; private set; }
        public ICommand SelectTransformModeRotate { get; private set; }
        public ICommand SelectTransformModeScale { get; private set; }

        public bool CanSelectTransformModeTranslate => CanSelectGizmo;// && _editorSubsystem.GetSelectedGizmo()?.ManipulationMode != EditorManipulationMode.Translate;
        public bool CanSelectTransformModeRotate => CanSelectGizmo;// && _editorSubsystem.GetSelectedGizmo()?.ManipulationMode != EditorManipulationMode.Rotate;
        public bool CanSelectTransformModeScale => CanSelectGizmo;// && _editorSubsystem.GetSelectedGizmo()?.ManipulationMode != EditorManipulationMode.Scale;

        public bool IsTransformModeTranslateActive => _editorSubsystem?.GetSelectedManipulationMode() == EditorManipulationMode.Translate;
        public bool IsTransformModeRotateActive => _editorSubsystem?.GetSelectedManipulationMode() == EditorManipulationMode.Rotate;
        public bool IsTransformModeScaleActive => _editorSubsystem?.GetSelectedManipulationMode() == EditorManipulationMode.Scale;

        public bool CanSelectGizmo = true; // temp hax

        public ICommand ToggleSnapToGrid { get; private set; }
        public bool IsSnapToGridEnabled => _editorSubsystem?.IsSnapToGridEnabled() ?? false;

        public ICommand TogglePhysicsDebugDraw { get; private set; }
        public bool IsPhysicsDebugDrawEnabled => _editorSubsystem?.IsPhysicsDebugDrawEnabled() ?? false;

        public ICommand FitPhysicsShapeToMesh { get; private set; }
        public bool CanFitPhysicsShapeToMesh => _canFitPhysicsShapeToMesh;

        /// <summary>
        /// Cached mirror of the engine's mesh edit state.
        /// </summary>
        private struct MeshEditStateSnapshot
        {
            public bool Enabled;
            public bool CanEnable;
            public bool FaceModeQuad;
            public bool AlignToNormal = true;
            public bool FaceSelected;
            public bool DragActive;
            public bool HasPendingEdits;
            public bool Simulating;
            public int LockedAxis = -1;
            public string TargetName = string.Empty;

            public MeshEditStateSnapshot()
            {
            }
        }

        private MeshEditStateSnapshot _meshEditState = new MeshEditStateSnapshot();

        private bool _canFitPhysicsShapeToMesh;

        public ICommand ToggleMeshEditMode { get; private set; }
        public bool IsMeshEditModeEnabled => _meshEditState.Enabled;

        public bool CanEnableMeshEditMode => _meshEditState.CanEnable;

        public ICommand DiscardMeshEdits { get; private set; }
        public ICommand SaveMeshEdits { get; private set; }

        public bool HasPendingMeshEdits => _meshEditState.HasPendingEdits;

        public ICommand SelectMeshEditFaceTriangle { get; private set; }
        public ICommand SelectMeshEditFaceQuad { get; private set; }
        public bool IsMeshEditFaceModeQuad => _meshEditState.FaceModeQuad;

        public ICommand ToggleMeshEditAlignToNormal { get; private set; }
        public bool IsMeshEditAlignToNormal => _meshEditState.AlignToNormal;

        public string MeshEditTargetName => _meshEditState.TargetName;

        public string MeshEditModeTooltip
        {
            get
            {
                if (IsMeshEditModeEnabled)
                {
                    return "Exit mesh edit (Esc)";
                }

                if (_meshEditState.Simulating)
                {
                    return "Cannot edit mesh while in simulation";
                }

                if (!CanEnableMeshEditMode)
                {
                    return "No mesh selected";
                }

                return "Mesh Edit Mode";
            }
        }

        public string StatusText
        {
            get => "Ready";
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
        private DelegateHandler? _selectionChangedHandler;
        private DelegateHandler? _currentProjectChangedHandler;
        private DelegateHandler? _clipboardChangedHandler;
        private DelegateHandler? _selectedGizmoChangedHandler;
        private DelegateHandler? _activeSceneChangedHandler;
        private DelegateHandler? _actionStackStateChangedHandler;
        private DelegateHandler? _meshEditStateChangedHandler;

        private int _isUpdatingSelectionFromEngine = 0; // atomic
        private int _isUpdatingFocusedNodeFromEngine = 0; // atomic
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

                OnPropertyChanged(nameof(ActiveScene));
            }
        }

        public ICommand SetActiveSceneCommand { get; private set; }
        public ICommand AddNewSceneCommand { get; private set; }

        public MainWindowViewModel()
        {
            PanelService.Instance.ActivePanelChanged += OnActivePanelChanged;

            SceneHierarchy = new SceneHierarchyViewModel();
            Inspector = new InspectorViewModel();
            ForegroundTask = new ForegroundTaskViewModel();

            SelectTransformModeTranslate = new SetGizmoCommand(EditorManipulationMode.Translate);
            SelectTransformModeRotate = new SetGizmoCommand(EditorManipulationMode.Rotate);
            SelectTransformModeScale = new SetGizmoCommand(EditorManipulationMode.Scale);

            ToggleSnapToGrid = new RelayCommand(() =>
            {
                _ = EngineManager.PostToSimThread(() =>
                {
                    _editorSubsystem.SetSnapToGridEnabled(!_editorSubsystem.IsSnapToGridEnabled());

                    Dispatcher.UIThread.Post(() => OnPropertyChanged(nameof(IsSnapToGridEnabled)));
                });
            });

            TogglePhysicsDebugDraw = new RelayCommand(() =>
            {
                _ = EngineManager.PostToSimThread(() =>
                {
                    _editorSubsystem.SetPhysicsDebugDrawEnabled(!_editorSubsystem.IsPhysicsDebugDrawEnabled());

                    Dispatcher.UIThread.Post(() => OnPropertyChanged(nameof(IsPhysicsDebugDrawEnabled)));
                });
            });

            FitPhysicsShapeToMesh = new RelayCommand(
                () =>
                {
                    _ = EngineManager.PostToSimThread(() =>
                    {
                        _editorSubsystem.FitPhysicsShapeToMesh();

                        RefreshMeshEditState();
                    });
                },
                () => CanFitPhysicsShapeToMesh);

            SetGameModePlaying = new SetGameModeCommand(GameStateMode.Simulating);
            SetGameModePaused = new SetGameModeCommand(GameStateMode.Paused);
            SetGameModeStopped = new SetGameModeCommand(GameStateMode.Stopped);

            ToggleMeshEditMode = new RelayCommand(() =>
            {
                _ = EngineManager.PostToSimThread(() =>
                {
                    if (_editorSubsystem.IsMeshEditModeEnabled())
                    {
                        _editorSubsystem.ExitMeshEditMode(/* saveEdits */ true);
                    }
                    else
                    {
                        _editorSubsystem.EnterMeshEditMode();
                    }

                    RefreshMeshEditState();
                });
            });

            SaveMeshEdits = new RelayCommand(() =>
            {
                _ = EngineManager.PostToSimThread(() =>
                {
                    _editorSubsystem.ExitMeshEditMode(/* saveEdits */ true);

                    RefreshMeshEditState();
                });
            });

            DiscardMeshEdits = new RelayCommand(
                () =>
                {
                    _ = EngineManager.PostToSimThread(() =>
                    {
                        _editorSubsystem.ExitMeshEditMode(/* saveEdits */ false);

                        RefreshMeshEditState();
                    });
                },
                () => HasPendingMeshEdits);

            SelectMeshEditFaceTriangle = new RelayCommand(() =>
            {
                _ = EngineManager.PostToSimThread(() =>
                {
                    _editorSubsystem.SetMeshEditFaceMode(MeshEditFaceMode.Triangle);

                    RefreshMeshEditState();
                });
            });

            SelectMeshEditFaceQuad = new RelayCommand(() =>
            {
                _ = EngineManager.PostToSimThread(() =>
                {
                    _editorSubsystem.SetMeshEditFaceMode(MeshEditFaceMode.Quad);

                    RefreshMeshEditState();
                });
            });

            ToggleMeshEditAlignToNormal = new RelayCommand(() =>
            {
                _ = EngineManager.PostToSimThread(() =>
                {
                    _editorSubsystem.SetMeshEditAlignToNormal(!_editorSubsystem.IsMeshEditAlignToNormal());

                    RefreshMeshEditState();
                });
            });

            SetActiveSceneCommand = new RelayCommand<SceneViewModel>(sceneViewModel =>
            {
                // Feed back changes to EditorSubsystem.
                // this will eventually trigger `set ActiveScene` via our bound delegate
                _ = EngineManager.PostToSimThread(() =>
                {
                    try
                    {
                        _editorSubsystem.SetActiveScene(sceneViewModel?.Scene);
                    }
                    catch (Exception ex)
                    {
                        Logger.Log(LogLevel.Warning, $"Failed to set active scene: {ex.Message}");
                    }
                });
            });

            AddNewSceneCommand = new RelayCommand(() =>
            {
                var panel = new AddNewScenePanelViewModel(result =>
                {
                    if (result == null)
                        return;

                    string sceneName = result.Name;
                    SceneFlags sceneFlags = result.Flags;

                    _ = EngineManager.PostToSimThread(() =>
                    {
                        try
                        {
                            Scene newScene = new Scene();
                            newScene.SetName(new Name(sceneName));
                            newScene.SetSceneFlags(sceneFlags);

                            EditorProject? project = EngineManager.CurrentProject;
                            if (project == null)
                            {
                                throw new Exception("Current project is null");
                            }
                            project.AddScene(newScene);

                            _editorSubsystem.SetActiveScene(newScene);
                        }
                        catch (Exception ex)
                        {
                            Logger.Log(LogLevel.Warning, $"Failed to add new scene: {ex.Message}");
                        }
                    });
                });

                PanelService.Instance.OpenPanel(panel);
            });

            AddNormalizedCubeSphereCommand = new RelayCommand(() =>
            {
                var panel = new AddNormalizedCubeSpherePanelViewModel(_editorSubsystem, confirmed => { });

                PanelService.Instance.OpenPanel(panel);
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
            ContentBrowser.LoadBuckets();
            OnPropertyChanged(nameof(ContentBrowser));

            HandleCurrentProjectChanged(_editorSubsystem.CurrentProject, isSimulationStateChange: false);

            EditorState editorState = EditorState.Instance;

            _currentProjectChangedHandler?.Remove();
            _currentProjectChangedHandler = editorState.GetOnCurrentProjectChangedDelegate()
                .Bind(HandleCurrentProjectChanged);

            _clipboardChangedHandler?.Remove();
            _clipboardChangedHandler = editorState.GetOnClipboardChangedDelegate()
                .Bind(() => OnClipboardChanged());

            // handle active scene changes
            _activeSceneChangedHandler?.Remove();
            _activeSceneChangedHandler = _editorSubsystem.GetOnActiveSceneChangedDelegate()
                .Bind(HandleActiveSceneChanged);

            SceneHierarchy.SelectedNodeChanged += OnSceneHierarchyNodeSelected;
            SceneHierarchy.SelectionChanged += OnSceneHierarchySelectionChanged;

            EngineManager.SceneAdded += OnSceneAdded;
            EngineManager.SceneRemoved += OnSceneRemoved;

            BindFocusedNodeChanged();
            BindSelectionChanged();
            BindMeshEditStateChanged();

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

        ~MainWindowViewModel()
        {
            Dispose(false);
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        public void Dispose(bool isDisposing)
        {
            PanelService.Instance.ActivePanelChanged -= OnActivePanelChanged;

            _gameModeChangedHandler?.Remove();
            _focusedNodeChangedHandler?.Remove();
            _selectionChangedHandler?.Remove();
            _currentProjectChangedHandler?.Remove();
            _clipboardChangedHandler?.Remove();
            _selectedGizmoChangedHandler?.Remove();
            _activeSceneChangedHandler?.Remove();
            _actionStackStateChangedHandler?.Remove();

            if (isDisposing)
            {
                EngineManager.SceneAdded -= OnSceneAdded;
                EngineManager.SceneRemoved -= OnSceneRemoved;
                EngineManager.TaskStarted -= OnTaskStarted;
                EngineManager.TaskEnded -= OnTaskEnded;
                EngineManager.TaskProgressUpdated -= OnTaskProgressUpdated;

                SceneHierarchy.SelectedNodeChanged -= OnSceneHierarchyNodeSelected;
                SceneHierarchy.SelectionChanged -= OnSceneHierarchySelectionChanged;
                
                ContentBrowser.Dispose();
            }
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
                Logger.Log(LogLevel.Info, "Changing active scene in c#");

                if (!_isReady)
                {
                    return;
                }

                // only attach scenes that have the FOREGROUND flag
                if (scene == null || !scene.SceneFlags.HasFlag(SceneFlags.Foreground))
                {
                    _activeScene = null;

                    SceneHierarchy.AttachToScene(null);

                    OnPropertyChanged(nameof(ActiveScene));

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
                CanUndo = false;
                CanRedo = false;
                return;
            }

            WeakReference<EditorProject> weakProject = new WeakReference<EditorProject>(project);

            _ = EngineManager.PostToSimThread(() =>
            {
                if (!weakProject.TryGetTarget(out EditorProject? p))
                    return;

                EditorActionBase? undoAction = p.ActionStack.GetUndoAction();
                EditorActionBase? redoAction = p.ActionStack.GetRedoAction();
                string? undoName = undoAction?.GetText();
                string? redoName = redoAction?.GetText();
                bool hasUndo = undoAction != null;
                bool hasRedo = redoAction != null;

                Dispatcher.UIThread.Post(() =>
                {
                    UndoHeader = string.IsNullOrEmpty(undoName) ? "_Undo" : $"_Undo {undoName}";
                    RedoHeader = string.IsNullOrEmpty(redoName) ? "_Redo" : $"_Redo {redoName}";
                    CanUndo = hasUndo;
                    CanRedo = hasRedo;
                });
            });
        }

        private void UpdatePasteHeader()
        {
            _ = EngineManager.PostToSimThread(() =>
            {
                IEnumerable<Node> clipboardNodes = EditorState.Instance.ClipboardNodes;

                int count = clipboardNodes.Count();
                string header;

                if (count == 0)
                {
                    header = "_Paste";
                }
                else if (count == 1)
                {
                    string? nodeName = clipboardNodes.FirstOrDefault()?.Name.ToString();
                    header = string.IsNullOrEmpty(nodeName) ? "_Paste" : $"_Paste {nodeName}";
                }
                else
                {
                    header = $"_Paste {count} Nodes";
                }

                Dispatcher.UIThread.Post(() =>
                {
                    PasteHeader = header;
                    CanPaste = count > 0;
                });
            });
        }

        private void UpdateCopyDeleteHeaders()
        {
            Dispatcher.UIThread.VerifyAccess();

            int count = SceneHierarchy.SelectedNodes.Count;
            CopyHeader = count > 1 ? $"_Copy {count} Nodes" : "_Copy";
            DeleteHeader = count > 1 ? $"_Delete {count} Nodes" : "_Delete";
            CanCopy = count > 0;
        }

        private void OnClipboardChanged()
        {
            UpdatePasteHeader();
        }

        private void HandleCurrentProjectChanged(EditorProject? project, bool isSimulationStateChange)
        {
            // Game mode:  when project changes we also want to update the play/pause/stop buttons
            _gameModeChangedHandler?.Remove();

            _actionStackStateChangedHandler?.Remove();
            _actionStackStateChangedHandler = null;

            if (project != null)
            {
                _actionStackStateChangedHandler = project.ActionStack.GetOnStateChangeDelegate()
                    .Bind((EditorActionStackState state, int undoDepth) =>
                    {
                        UpdateUndoRedoHeaders(project);
                        UpdatePasteHeader();

                        Dispatcher.UIThread.Post(() => SceneHierarchy.RefreshAllNames());
                    });
            }

            UpdateUndoRedoHeaders(project);
            UpdatePasteHeader();

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

                            OnPropertyChanged(nameof(IsTransformModeTranslateActive));
                            OnPropertyChanged(nameof(IsTransformModeRotateActive));
                            OnPropertyChanged(nameof(IsTransformModeScaleActive));

                            _ = EngineManager.PostToSimThread(RefreshMeshEditState);
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

                        OnPropertyChanged(nameof(IsTransformModeTranslateActive));
                        OnPropertyChanged(nameof(IsTransformModeRotateActive));
                        OnPropertyChanged(nameof(IsTransformModeScaleActive));
                    });
                });

            WeakReference<EditorProject?> weakProjectForUI = new WeakReference<EditorProject?>(project);

            Dispatcher.UIThread.Post(() =>
            {
                OnPropertyChanged(nameof(CanSetGameModePlaying));
                OnPropertyChanged(nameof(CanSetGameModePaused));
                OnPropertyChanged(nameof(CanSetGameModeStopped));

                OnPropertyChanged(nameof(CanSelectGizmo));
                OnPropertyChanged(nameof(CanSelectTransformModeTranslate));
                OnPropertyChanged(nameof(CanSelectTransformModeRotate));
                OnPropertyChanged(nameof(CanSelectTransformModeScale));

                OnPropertyChanged(nameof(IsTransformModeTranslateActive));
                OnPropertyChanged(nameof(IsTransformModeRotateActive));
                OnPropertyChanged(nameof(IsTransformModeScaleActive));

                OnPropertyChanged(nameof(IsSnapToGridEnabled));

                _ = EngineManager.PostToSimThread(RefreshMeshEditState);

                // Update scenes list
                Scenes.Clear();

                weakProjectForUI.TryGetTarget(out EditorProject? p);

                if (p != null)
                {
                    foreach (Scene scene in p.World.GetScenes())
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
            if (!_isReady || _isUpdatingSelectionFromEngine != 0 || _isUpdatingFocusedNodeFromEngine != 0)
            {
                return;
            }

            CanAddInstance = node is Entity;

            _ = EngineManager.PostToSimThread(() =>
            {
                _editorSubsystem.SetSelectedNodes(new Node[] { node! });
                _editorSubsystem.SetFocusedNode(node!, false);
            });

            bool isRootNode = SceneHierarchy.IsRootNode(node);
            Inspector.SetSelectedNode(node, SceneHierarchy.Scene, isRootNode);
        }

        public void HandleShiftClick(NodeViewModel clickedNode)
        {
            Dispatcher.UIThread.VerifyAccess();

            if (!_isReady)
            {
                return;
            }

            // Save the anchor (currently selected/focused node) before updating it
            NodeViewModel? anchorNode = SceneHierarchy.SelectedNode;

            // Get the clicked node's native reference
            Node clickedNodeRef = clickedNode.Node;

            // Set the clicked node as the new focused node (with notification suppressed)
            SceneHierarchy.SelectNodeFromEngine(clickedNodeRef);

            // Get flattened list of all visible nodes in tree order
            List<NodeViewModel> flatNodes = SceneHierarchy.GetFlattenedNodes();

            int anchorIndex = -1;
            int clickedIndex = -1;

            for (int i = 0; i < flatNodes.Count; i++)
            {
                if (anchorNode != null && flatNodes[i] == anchorNode)
                {
                    anchorIndex = i;
                }

                if (flatNodes[i] == clickedNode)
                {
                    clickedIndex = i;
                }
            }

            if (clickedIndex == -1)
            {
                return;
            }

            // If no previous anchor, treat as normal single-click selection
            if (anchorIndex == -1)
            {
                anchorIndex = clickedIndex;
            }

            // Determine range bounds
            int rangeStart = Math.Min(anchorIndex, clickedIndex);
            int rangeEnd = Math.Max(anchorIndex, clickedIndex);

            // Capture nodes in the range
            List<Node> nodesInRange = new List<Node>();
            for (int i = rangeStart; i <= rangeEnd; i++)
            {
                NodeViewModel vm = flatNodes[i];
                if (vm.Node != null && vm.Node.IsValid)
                {
                    nodesInRange.Add(vm.Node);
                }
            }

            // Push the range selection to the engine on the sim thread
            _ = EngineManager.PostToSimThread(() =>
            {
                _editorSubsystem.SetSelectedNodes(new Node[] { clickedNodeRef });
                _editorSubsystem.SetFocusedNode(clickedNodeRef, false);
            });

            // Update the inspector for the new focused node
            bool isRootNode = SceneHierarchy.IsRootNode(clickedNodeRef);
            Inspector.SetSelectedNode(clickedNodeRef, SceneHierarchy.Scene, isRootNode);
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
            if (Interlocked.CompareExchange(ref _isUpdatingFocusedNodeFromEngine, 1, 0) != 0)
            {
                return;
            }

            Dispatcher.UIThread.Post(() =>
            {
                if (!_isReady)
                {
                    Interlocked.Exchange(ref _isUpdatingFocusedNodeFromEngine, 0);
                    return;
                }

                try
                {
                    Node? validNode = node != null && node.IsValid ? node : null;

                    bool isRootNode = SceneHierarchy.IsRootNode(validNode);
                    Inspector.SetSelectedNode(validNode, SceneHierarchy.Scene, isRootNode);
                    SceneHierarchy.SelectNodeFromEngine(validNode);
                    UpdateCopyDeleteHeaders();

                    // can ONLY add Instanced Mesh Proxy child objects instances to entities that have a MeshComponent.
                    // Note that for now the most derived class MUST be EQUAL to Entity (not just derived from it)
                    // as currently we only support adding instances to entities, not to other node types (e.g. we don't support adding instances to a Light)
                    CanAddInstance = validNode != null
                        && validNode.GetType() == typeof(Entity);
                    //&& ((Entity)validNode).HasComponent<MeshComponent>();

                    _ = EngineManager.PostToSimThread(RefreshMeshEditState);
                }
                finally
                {
                    Interlocked.Exchange(ref _isUpdatingFocusedNodeFromEngine, 0);
                }
            });
        }

        private void OnSceneHierarchySelectionChanged()
        {
            if (!_isReady)
            {
                return;
            }

            if (Interlocked.CompareExchange(ref _isUpdatingSelectionFromEngine, 1, 0) != 0)
            {
                return;
            }

            // Capture the selected nodes on the UI thread before dispatching to sim thread
            List<Node> nodes = SceneHierarchy.SelectedNodes
                .Select(vm => vm.Node)
                .Where(n => n != null && n.IsValid)
                .ToList();

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    _editorSubsystem.ClearSelection();

                    foreach (Node node in nodes)
                    {
                        _editorSubsystem.AddToSelection(node);
                    }
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Failed to update selection on engine: {ex.Message}");
                }

                // Refresh the UI from engine state to ensure consistency
                try
                {
                    var selectedNodes = _editorSubsystem.GetSelectedNodes();

                    Dispatcher.UIThread.Post(() =>
                    {
                        SceneHierarchy.UpdateSelectionFromEngine(selectedNodes.Cast<Node>());
                        UpdateCopyDeleteHeaders();
                        Interlocked.Exchange(ref _isUpdatingSelectionFromEngine, 0);
                    });
                }
                catch
                {
                    Interlocked.Exchange(ref _isUpdatingSelectionFromEngine, 0);
                }
            });
        }

        private void BindSelectionChanged()
        {
            WeakReference<MainWindowViewModel> weakThis = new WeakReference<MainWindowViewModel>(this);

            _selectionChangedHandler?.Remove();

            _selectionChangedHandler = _editorSubsystem.GetOnSelectionChangedDelegate()
                .Bind(() =>
                {
                    if (!weakThis.TryGetTarget(out MainWindowViewModel? target))
                    {
                        return;
                    }

                    target.HandleSelectionUpdate();
                });
        }

        private void BindMeshEditStateChanged()
        {
            WeakReference<MainWindowViewModel> weakThis = new WeakReference<MainWindowViewModel>(this);

            _meshEditStateChangedHandler?.Remove();
            _meshEditStateChangedHandler = _editorSubsystem.GetOnMeshEditStateChangedDelegate()
                .Bind(() =>
                {
                    if (!weakThis.TryGetTarget(out MainWindowViewModel? target))
                    {
                        return;
                    }

                    target.RefreshMeshEditState();
                });
        }

        /// <summary>
        /// Reads the engine's mesh edit state and publishes it to the UI thread.
        /// </summary>
        private void RefreshMeshEditState()
        {
            if (_editorSubsystem == null)
            {
                return;
            }

            MeshEditStateSnapshot snapshot = new MeshEditStateSnapshot();

            bool canFitPhysicsShapeToMesh = false;

            try
            {
                snapshot.Enabled = _editorSubsystem.IsMeshEditModeEnabled();
                snapshot.CanEnable = _editorSubsystem.CanEnableMeshEditMode();
                snapshot.FaceModeQuad = _editorSubsystem.GetMeshEditFaceMode() == MeshEditFaceMode.Quad;
                snapshot.AlignToNormal = _editorSubsystem.IsMeshEditAlignToNormal();
                snapshot.FaceSelected = _editorSubsystem.HasMeshEditFaceSelected();
                snapshot.DragActive = _editorSubsystem.IsMeshEditDragActive();
                snapshot.HasPendingEdits = _editorSubsystem.HasPendingMeshEdits();
                snapshot.Simulating = _editorSubsystem.IsSimulating();
                snapshot.LockedAxis = _editorSubsystem.GetMeshEditLockedAxis();
                snapshot.TargetName = _editorSubsystem.GetMeshEditTargetNode()?.Name.ToString() ?? string.Empty;

                canFitPhysicsShapeToMesh = _editorSubsystem.CanFitPhysicsShapeToMesh();
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Warning, $"Failed to read mesh edit state from engine: {ex.Message}");

                return;
            }

            Dispatcher.UIThread.Post(() =>
            {
                _meshEditState = snapshot;

                _canFitPhysicsShapeToMesh = canFitPhysicsShapeToMesh;

                NotifyMeshEditStateChanged();
            });
        }

        private void NotifyMeshEditStateChanged()
        {
            OnPropertyChanged(nameof(IsMeshEditModeEnabled));
            OnPropertyChanged(nameof(CanEnableMeshEditMode));
            OnPropertyChanged(nameof(IsMeshEditFaceModeQuad));
            OnPropertyChanged(nameof(IsMeshEditAlignToNormal));
            OnPropertyChanged(nameof(HasPendingMeshEdits));
            OnPropertyChanged(nameof(MeshEditTargetName));
            OnPropertyChanged(nameof(MeshEditModeTooltip));
            OnPropertyChanged(nameof(StatusText));

            OnPropertyChanged(nameof(IsTransformModeTranslateActive));
            OnPropertyChanged(nameof(IsTransformModeRotateActive));
            OnPropertyChanged(nameof(IsTransformModeScaleActive));

            (ToggleMeshEditMode as RelayCommand)?.RaiseCanExecuteChanged();
            (SaveMeshEdits as RelayCommand)?.RaiseCanExecuteChanged();
            (DiscardMeshEdits as RelayCommand)?.RaiseCanExecuteChanged();

            OnPropertyChanged(nameof(CanFitPhysicsShapeToMesh));
            (FitPhysicsShapeToMesh as RelayCommand)?.RaiseCanExecuteChanged();
        }

        private void HandleSelectionUpdate()
        {
            if (Interlocked.CompareExchange(ref _isUpdatingSelectionFromEngine, 1, 0) != 0)
            {
                return;
            }

            try
            {
                var selectedNodes = _editorSubsystem.GetSelectedNodes();

                Dispatcher.UIThread.Post(() =>
                {
                    try
                    {
                        SceneHierarchy.UpdateSelectionFromEngine(selectedNodes.Cast<Node>());
                        UpdateCopyDeleteHeaders();
                    }
                    finally
                    {
                        Interlocked.Exchange(ref _isUpdatingSelectionFromEngine, 0);
                    }
                });
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Warning, $"Failed to get selected nodes from engine: {ex.Message}");
                Interlocked.Exchange(ref _isUpdatingSelectionFromEngine, 0);
            }
        }

        public void SelectSingleNodeExclusive(NodeViewModel clicked)
        {
            Dispatcher.UIThread.VerifyAccess();

            Node? clickedNode = clicked.Node;

            SceneHierarchy.SelectedNodes.Clear();
            SceneHierarchy.SelectedNodes.Add(clicked);
            SceneHierarchy.NotifySelectedNodesChanged();
            UpdateCopyDeleteHeaders();

            if (clickedNode != null)
            {
                _ = EngineManager.PostToSimThread(() =>
                {
                    _editorSubsystem.SetSelectedNodes(new Node[] { clickedNode });
                    _editorSubsystem.SetFocusedNode(clickedNode, false);
                });

                bool isRootNode = SceneHierarchy.IsRootNode(clickedNode);
                Inspector.SetSelectedNode(clickedNode, SceneHierarchy.Scene, isRootNode);
                CanAddInstance = clickedNode is Entity;
            }
        }

        public void HandleTreeSelectionChanged(List<NodeViewModel> added, List<NodeViewModel> removed)
        {
            Dispatcher.UIThread.VerifyAccess();

            if (!_isReady || _isUpdatingSelectionFromEngine != 0 || _isUpdatingFocusedNodeFromEngine != 0)
                return;

            // Nothing changed
            if (added.Count == 0 && removed.Count == 0)
                return;

            bool isReplace = removed.Count > 0 && added.Count == 1;
            bool isToggleAdd = removed.Count == 0 && added.Count == 1;
            bool isToggleRemove = removed.Count >= 1 && added.Count == 0;

            if (isReplace)
            {
                // Normal click: replace selection with the single clicked node
                SelectSingleNodeExclusive(added[0]);
            }
            else if (isToggleAdd)
            {
                // Ctrl+Click: add to selection
                NodeViewModel toggled = added[0];
                Node? toggledNode = toggled.Node;

                if (!SceneHierarchy.SelectedNodes.Contains(toggled))
                {
                    SceneHierarchy.SelectedNodes.Add(toggled);
                    SceneHierarchy.NotifySelectedNodesChanged();
                    UpdateCopyDeleteHeaders();

                    if (toggledNode != null)
                    {
                        _ = EngineManager.PostToSimThread(() =>
                        {
                            try
                            {
                                _editorSubsystem.AddToSelection(toggledNode);
                            }
                            catch (Exception ex)
                            {
                                Logger.Log(LogLevel.Warning, $"Failed to add to selection: {ex.Message}");
                            }
                        });
                    }
                }
            }
            else if (isToggleRemove)
            {
                // Ctrl+Click or deselect: remove from selection
                foreach (NodeViewModel vm in removed)
                {
                    SceneHierarchy.SelectedNodes.Remove(vm);

                    Node? node = vm.Node;
                    if (node != null)
                    {
                        _ = EngineManager.PostToSimThread(() =>
                        {
                            try
                            {
                                _editorSubsystem.RemoveFromSelection(node);
                            }
                            catch (Exception ex)
                            {
                                Logger.Log(LogLevel.Warning, $"Failed to remove from selection: {ex.Message}");
                            }
                        });
                    }
                }

                SceneHierarchy.NotifySelectedNodesChanged();
                UpdateCopyDeleteHeaders();
            }
            else
            {
                SceneHierarchy.SelectedNodes.Clear();
                
                foreach (NodeViewModel vm in added)
                {
                    SceneHierarchy.SelectedNodes.Add(vm);
                }
                
                SceneHierarchy.NotifySelectedNodesChanged();
                
                UpdateCopyDeleteHeaders();

                // Sync to engine
                List<Node> nodes = added
                    .Select(vm => vm.Node)
                    .Where(n => n != null && n.IsValid)
                    .ToList();

                if (nodes.Count > 0 && added.Count > 0)
                {
                    Node? first = nodes[0];

                    _ = EngineManager.PostToSimThread(() =>
                    {
                        _editorSubsystem.ClearSelection();
                        if (first != null)
                        {
                            _editorSubsystem.SetSelectedNodes(new Node[] { first });
                            _editorSubsystem.SetFocusedNode(first, false);
                        }
                        else
                        {
                            _editorSubsystem.ClearSelection();
                        }
                    });
                }
            }
        }

        public void AddAssetToScene(uint bucketIndex, Name assetName)
        {
            if (!_isReady)
                return;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    _editorSubsystem.ExecuteCommandByName(
                        new Name("EditorCommandAddAsset"),
                        $"{bucketIndex} {assetName}");
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Failed to add asset to scene: {ex.Message}");
                }
            });
        }

        public void AddAssetToSceneAtViewport(uint bucketIndex, Name assetName, float nx, float ny)
        {
            if (!_isReady)
                return;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    _editorSubsystem.ExecuteCommandByName(
                        new Name("EditorCommandAddAsset"),
                        $"{bucketIndex} {assetName} {nx} {ny}");
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Failed to add asset to scene at viewport: {ex.Message}");
                }
            });
        }

        private void OnSceneMenuItemClick(object? sender, EventArgs e)
        {
            // @TODO Hide dropdown
        }

        private void OnActivePanelChanged(object? sender, EventArgs e)
        {
            OnPropertyChanged(nameof(ActivePanel));
        }
    }
}
