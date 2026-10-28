using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Globalization;
using System.Linq;
using System.Windows.Input;
using Avalonia;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.ViewModels
{
    public enum AnimationGraphNodeCategory
    {
        Source,
        Blend,
        Modify,
        State
    }

    public sealed class AnimationGraphNodeView
    {
        public const int NoInput = -1;

        public int Index { get; init; }
        public string Title { get; init; } = string.Empty;
        public string Kind { get; init; } = string.Empty;
        public string Detail { get; init; } = string.Empty;
        public AnimationGraphNodeCategory Category { get; init; }
        public Point Position { get; set; }
        public IReadOnlyList<(string Name, int Source)> Inputs { get; init; } = Array.Empty<(string, int)>();
        public IReadOnlyList<string> Parameters { get; init; } = Array.Empty<string>();
        public bool CanAddInputs { get; init; }
        public bool IsOutput { get; init; }
        public bool IsStateMachine { get; init; }
        public float Weight { get; set; }
        public string Status { get; set; } = string.Empty;
    }

    public sealed class AnimationStateView
    {
        public const int AnyState = -1;

        public int Index { get; init; }
        public string Name { get; init; } = string.Empty;
        public int InputIndex { get; init; } = AnimationGraphNodeView.NoInput;
        public string InputTitle { get; init; } = string.Empty;
        public Point Position { get; set; }
        public bool IsEntry { get; init; }
        public bool IsCurrent { get; set; }
        public bool IsPrevious { get; set; }
        public float Progress { get; set; }
    }

    public sealed class AnimationTransitionView
    {
        public int Index { get; init; }
        public int From { get; init; }
        public int To { get; init; }
        public bool IsActive { get; set; }
        public IReadOnlyList<string> Parameters { get; init; } = Array.Empty<string>();
    }

    public class AnimationGraphParameterViewModel : ViewModelBase
    {
        private readonly AnimationGraphDocumentViewModel _document;

        private decimal _x;
        private decimal _y;
        private bool _isApplyingValue;
        private bool _isSelected;

        public AnimationGraphParameterViewModel(AnimationGraphDocumentViewModel document, string name, uint type)
        {
            _document = document;

            Name = name;
            Type = type;

            RemoveCommand = new RelayCommand(() => _document.RemoveParameter(Name));
            FireCommand = new RelayCommand(() => _document.TriggerParameter(Name));
            SelectCommand = new RelayCommand(() => _document.SelectParameter(Name));
        }

        public string Name { get; }

        public uint Type { get; }

        public string TypeLabel => AnimationGraphDocumentViewModel.ParameterTypeLabels[(int)Math.Min(Type, 3)];

        public bool IsFloat => Type == 0;
        public bool IsBool => Type == 1;
        public bool IsTrigger => Type == 2;
        public bool IsVector2 => Type == 3;

        public ICommand RemoveCommand { get; }
        public ICommand FireCommand { get; }
        public ICommand SelectCommand { get; }

        public bool IsSelected
        {
            get => _isSelected;
            set => SetProperty(ref _isSelected, value);
        }

        public decimal X
        {
            get => _x;
            set
            {
                if (SetProperty(ref _x, value))
                {
                    OnPropertyChanged(nameof(BoolValue));
                    PushValue();
                }
            }
        }

        public decimal Y
        {
            get => _y;
            set
            {
                if (SetProperty(ref _y, value))
                {
                    PushValue();
                }
            }
        }

        public bool BoolValue
        {
            get => _x != 0;
            set => X = value ? 1 : 0;
        }

        public void ApplyValue(Vec2f value)
        {
            _isApplyingValue = true;

            X = Math.Round((decimal)value.x, 3);
            Y = Math.Round((decimal)value.y, 3);

            _isApplyingValue = false;
        }

        private void PushValue()
        {
            if (!_isApplyingValue)
            {
                _document.SetParameterValue(Name, new Vec2f((float)_x, (float)_y));
            }
        }
    }

    public class AnimationTransitionConditionViewModel : ViewModelBase
    {
        private const int NumParameterOps = 7;

        private readonly AnimationGraphDocumentViewModel _document;
        private readonly int _transitionIndex;
        private readonly int _conditionIndex;

        private string _parameter;
        private int _opIndex;
        private decimal _value;

        public AnimationTransitionConditionViewModel(AnimationGraphDocumentViewModel document, int transitionIndex, int conditionIndex, string parameter, int opIndex, float value)
        {
            _document = document;
            _transitionIndex = transitionIndex;
            _conditionIndex = conditionIndex;

            _parameter = parameter;
            _opIndex = opIndex;
            _value = Math.Round((decimal)value, 3);

            RemoveCommand = new RelayCommand(() => _document.RemoveCondition(_transitionIndex, _conditionIndex));
        }

        public IReadOnlyList<string> ParameterNames => _document.Parameters.Select(p => p.Name).ToList();

        public IReadOnlyList<string> Ops => AnimationGraphDocumentViewModel.ConditionOps;

        public ICommand RemoveCommand { get; }

        public bool UsesParameter => _opIndex < NumParameterOps;

        // bool and trigger tests have nothing to compare against
        public bool UsesValue => _opIndex < 4 || _opIndex >= NumParameterOps;

        public string ValueUnit => _opIndex == 7 || _opIndex == 8 ? "s" : string.Empty;

        public string Parameter
        {
            get => _parameter;
            set
            {
                if (value != null && SetProperty(ref _parameter, value))
                {
                    Push();
                }
            }
        }

        public int OpIndex
        {
            get => _opIndex;
            set
            {
                if (SetProperty(ref _opIndex, value))
                {
                    OnPropertyChanged(nameof(UsesParameter));
                    OnPropertyChanged(nameof(UsesValue));
                    OnPropertyChanged(nameof(ValueUnit));
                    Push();
                }
            }
        }

        public decimal Value
        {
            get => _value;
            set
            {
                if (SetProperty(ref _value, value))
                {
                    Push();
                }
            }
        }

        private void Push() => _document.SetCondition(_transitionIndex, _conditionIndex, _parameter, _opIndex, (float)_value);
    }

    public class AnimationGraphDocumentViewModel : EditorDocumentViewModel
    {
        private const int PollIntervalMs = 150;

        private static readonly Dictionary<string, (string Label, AnimationGraphNodeCategory Category)> NodeKinds = new()
        {
            ["AnimationClipNode"] = ("Clip", AnimationGraphNodeCategory.Source),
            ["AnimationMotionMatchingNode"] = ("Motion Matching", AnimationGraphNodeCategory.Source),
            ["AnimationBlend1DNode"] = ("Blend 1D", AnimationGraphNodeCategory.Blend),
            ["AnimationBlend2DNode"] = ("Blend 2D", AnimationGraphNodeCategory.Blend),
            ["AnimationLayerNode"] = ("Layer", AnimationGraphNodeCategory.Blend),
            ["AnimationAdditiveNode"] = ("Additive", AnimationGraphNodeCategory.Blend),
            ["AnimationSlotNode"] = ("Slot", AnimationGraphNodeCategory.Blend),
            ["AnimationChainTwistNode"] = ("Chain Twist", AnimationGraphNodeCategory.Modify),
            ["AnimationFootIKNode"] = ("Foot IK", AnimationGraphNodeCategory.Modify),
            ["AnimationStateMachineNode"] = ("State Machine", AnimationGraphNodeCategory.State)
        };

        public static readonly IReadOnlyList<string> ConditionOps = new[]
        {
            ">", "<", "=", "!=", "is true", "is false", "is triggered", "time in state >=", "time left <", "progress >="
        };

        public static readonly IReadOnlyList<string> BlendCurves = new[] { "Linear", "Smooth Step", "Ease In", "Ease Out" };

        public static readonly IReadOnlyList<string> ParameterTypeLabels = new[] { "Float", "Bool", "Trigger", "Vector2" };

        private readonly DispatcherTimer _pollTimer;

        private DelegateHandler? _graphChangedHandler;

        private Entity? _debugEntity;
        private AnimationGraph? _debugGraph;
        private AnimationGraphInstance? _capturedInstance;

        private string _graphName = string.Empty;
        private bool _isDebugging;
        private bool _isGraphReadOnly;
        private bool _frameOnNextRefresh;
        private int _selectedNode = AnimationGraphNodeView.NoInput;
        private HashSet<int> _selectedNodes = new HashSet<int>();
        private List<int> _copiedNodes = new List<int>();
        private ComponentSubObjectViewModel? _selectedNodeProperties;
        private string _selectedParameter = string.Empty;
        private string _blendParameter = string.Empty;
        private bool _isBlend2D;
        private Vector _blendCursor;
        private string _newParameterName = string.Empty;
        private int _newParameterType;

        private List<Entity> _previewEntities = new List<Entity>();
        private int _selectedPreviewIndex = -1;
        private bool _isApplyingPreview;
        private bool _isPreviewPlaying;

        private int _openStateMachine = AnimationGraphNodeView.NoInput;
        private string _stateMachineTitle = string.Empty;
        private int _selectedState = AnimationStateView.AnyState;
        private int _selectedTransition = AnimationGraphNodeView.NoInput;
        private bool _isApplyingSelection;
        private string _stateName = string.Empty;
        private string _stateSourceTitle = string.Empty;
        private bool _isSelectedStateEntry;
        private string _transitionTitle = string.Empty;
        private decimal _transitionDuration;
        private int _transitionCurve;
        private decimal _transitionPriority;
        private bool _transitionInterruptible;
        private bool _transitionInertialize;
        private List<(int From, int To, float Duration, int Curve, int Priority, bool Interruptible, bool Inertialize, List<(string Parameter, int Op, float Value)> Conditions)> _transitionDetails = new();

        private string _parameterName = string.Empty;
        private int _parameterType;
        private decimal _parameterDefaultX;
        private decimal _parameterDefaultY;

        public AnimationGraphDocumentViewModel()
        {
            AddParameterCommand = new RelayCommand(AddParameter);
            CloseStateMachineCommand = new RelayCommand(CloseStateMachine);
            SetEntryStateCommand = new RelayCommand(() => SetEntryState(_selectedState));
            RemoveStateCommand = new RelayCommand(() => RemoveState(_selectedState));
            ShowStateSourceCommand = new RelayCommand(() => OpenStateInput(_selectedState));
            RemoveTransitionCommand = new RelayCommand(() => RemoveTransition(_selectedTransition));
            AddConditionCommand = new RelayCommand(AddCondition);
            RemoveParameterCommand = new RelayCommand(() => RemoveParameter(_selectedParameter));
            CopyToProjectCommand = new RelayCommand(CopyToProject);
            TogglePlayCommand = new RelayCommand(TogglePlay);
            RestartCommand = new RelayCommand(Restart);
            AddNodeCommand = new RelayCommand(() => AddNodeRequested?.Invoke());
            FrameCommand = new RelayCommand(() => FrameRequested?.Invoke());
            ArrangeCommand = new RelayCommand(() =>
            {
                _frameOnNextRefresh = true;
                PostEdit(state => state.ArrangeGraphNodes());
            });

            Inspector = new AnimationGraphInspectorViewModel(this);

            _pollTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(PollIntervalMs) };
            _pollTimer.Tick += (_, _) => PollPreview();

            if (MainWindowViewModel.Instance is { } mainWindow)
            {
                mainWindow.PropertyChanged += OnMainWindowPropertyChanged;
            }
        }

        public event Action? GraphChanged;
        public event Action? PreviewChanged;
        public event Action<int>? FocusNodeRequested;
        public event Action? FrameRequested;
        public event Action? AddNodeRequested;

        public override bool DocksAtBottom => true;

        public AnimationGraphInspectorViewModel Inspector { get; }

        public List<AnimationGraphNodeView> Nodes { get; private set; } = new List<AnimationGraphNodeView>();

        public List<AnimationStateView> States { get; private set; } = new List<AnimationStateView>();

        public List<AnimationTransitionView> Transitions { get; private set; } = new List<AnimationTransitionView>();

        public ObservableCollection<AnimationGraphParameterViewModel> Parameters { get; } = new ObservableCollection<AnimationGraphParameterViewModel>();

        public ObservableCollection<AnimationTransitionConditionViewModel> Conditions { get; } = new ObservableCollection<AnimationTransitionConditionViewModel>();

        public ObservableCollection<string> PreviewChoices { get; } = new ObservableCollection<string>();

        public IReadOnlyList<string> ClipChoices { get; private set; } = Array.Empty<string>();

        public IReadOnlyList<(string ClassName, string Label)> NodeKindChoices { get; } = NodeKinds.Select(kv => (kv.Key, kv.Value.Label)).ToList();

        public IReadOnlyList<string> ParameterTypes => ParameterTypeLabels;

        public IReadOnlyList<string> Curves => BlendCurves;

        public List<(Vector Position, string Label)> BlendSamples { get; private set; } = new List<(Vector, string)>();

        public ICommand AddParameterCommand { get; }
        public ICommand CloseStateMachineCommand { get; }
        public ICommand SetEntryStateCommand { get; }
        public ICommand RemoveStateCommand { get; }
        public ICommand ShowStateSourceCommand { get; }
        public ICommand RemoveTransitionCommand { get; }
        public ICommand AddConditionCommand { get; }
        public ICommand RemoveParameterCommand { get; }
        public ICommand CopyToProjectCommand { get; }
        public ICommand TogglePlayCommand { get; }
        public ICommand RestartCommand { get; }
        public ICommand AddNodeCommand { get; }
        public ICommand FrameCommand { get; }
        public ICommand ArrangeCommand { get; }

        public override string Id => "AnimationGraph";

        public bool IsDebugging
        {
            get => _isDebugging;
            private set
            {
                if (SetProperty(ref _isDebugging, value))
                {
                    OnReadOnlyChanged();
                }
            }
        }

        public bool IsGraphReadOnly
        {
            get => _isGraphReadOnly;
            private set
            {
                if (SetProperty(ref _isGraphReadOnly, value))
                {
                    OnReadOnlyChanged();
                }
            }
        }

        public bool IsReadOnly => _isDebugging || _isGraphReadOnly;

        public bool IsEditable => !IsReadOnly;

        public bool CanCopyToProject => _isGraphReadOnly && !_isDebugging;

        public bool HasPreviewControls => !_isDebugging;

        public bool HasApplyControls => !_isDebugging && !_isGraphReadOnly;

        public string ReadOnlyLabel => _isDebugging ? "Debugging" : "Read-only";

        public bool IsPreviewPlaying
        {
            get => _isPreviewPlaying;
            private set
            {
                if (SetProperty(ref _isPreviewPlaying, value))
                {
                    OnPropertyChanged(nameof(PlayIcon));
                }
            }
        }

        public string PlayIcon => _isPreviewPlaying ? "DebugPause" : "Play";

        public int SelectedPreviewIndex
        {
            get => _selectedPreviewIndex;
            set
            {
                if (SetProperty(ref _selectedPreviewIndex, value) && !_isApplyingPreview && value >= 0 && value < _previewEntities.Count)
                {
                    Entity entity = _previewEntities[value];

                    PostEdit(state => state.SetGraphPreviewSource(entity), allowReadOnly: true);
                }
            }
        }

        public int SelectedNode
        {
            get => _selectedNode;
            private set
            {
                if (SetProperty(ref _selectedNode, value))
                {
                    OnPropertyChanged(nameof(HasSelection));
                }
            }
        }

        public IReadOnlyCollection<int> SelectedNodes => _selectedNodes;

        public bool HasSelection => _selectedNode >= 0 && !IsInStateMachine;

        public ComponentSubObjectViewModel? SelectedNodeProperties
        {
            get => _selectedNodeProperties;
            private set => SetProperty(ref _selectedNodeProperties, value);
        }

        public string SelectedParameter => _selectedParameter;

        public bool IsParameterSelected => _selectedParameter.Length > 0;

        public bool IsBlendSelected { get; private set; }

        public bool IsBlend2D
        {
            get => _isBlend2D;
            private set => SetProperty(ref _isBlend2D, value);
        }

        public string BlendParameter
        {
            get => _blendParameter;
            private set => SetProperty(ref _blendParameter, value);
        }

        public Vector BlendCursor
        {
            get => _blendCursor;
            private set => SetProperty(ref _blendCursor, value);
        }

        public string NewParameterName
        {
            get => _newParameterName;
            set => SetProperty(ref _newParameterName, value);
        }

        public int NewParameterType
        {
            get => _newParameterType;
            set => SetProperty(ref _newParameterType, value);
        }

        public int OpenStateMachine => _openStateMachine;

        public bool IsInStateMachine => _openStateMachine >= 0;

        public string StateMachineTitle
        {
            get => _stateMachineTitle;
            private set => SetProperty(ref _stateMachineTitle, value);
        }

        public int SelectedState => _selectedState;

        public int SelectedTransition => _selectedTransition;

        public bool IsStateSelected => IsInStateMachine && _selectedState >= 0;

        public bool IsTransitionSelected => IsInStateMachine && _selectedTransition >= 0;

        public string StateName
        {
            get => _stateName;
            set
            {
                if (value != null && SetProperty(ref _stateName, value) && !_isApplyingSelection && value.Trim().Length > 0)
                {
                    int stateIndex = _selectedState;
                    int stateMachine = _openStateMachine;
                    string name = value.Trim();

                    PostEdit(state => state.RenameGraphState((uint)stateMachine, (uint)stateIndex, new Name(name)));
                }
            }
        }

        public string StateSourceTitle
        {
            get => _stateSourceTitle;
            private set => SetProperty(ref _stateSourceTitle, value);
        }

        public bool IsSelectedStateEntry
        {
            get => _isSelectedStateEntry;
            private set => SetProperty(ref _isSelectedStateEntry, value);
        }

        public string TransitionTitle
        {
            get => _transitionTitle;
            private set => SetProperty(ref _transitionTitle, value);
        }

        public decimal TransitionDuration
        {
            get => _transitionDuration;
            set
            {
                if (SetProperty(ref _transitionDuration, value))
                {
                    PushTransition();
                }
            }
        }

        public int TransitionCurve
        {
            get => _transitionCurve;
            set
            {
                if (SetProperty(ref _transitionCurve, value))
                {
                    PushTransition();
                }
            }
        }

        public decimal TransitionPriority
        {
            get => _transitionPriority;
            set
            {
                if (SetProperty(ref _transitionPriority, value))
                {
                    PushTransition();
                }
            }
        }

        public bool TransitionInterruptible
        {
            get => _transitionInterruptible;
            set
            {
                if (SetProperty(ref _transitionInterruptible, value))
                {
                    PushTransition();
                }
            }
        }

        public bool TransitionInertialize
        {
            get => _transitionInertialize;
            set
            {
                if (SetProperty(ref _transitionInertialize, value))
                {
                    PushTransition();
                }
            }
        }

        public string ParameterName
        {
            get => _parameterName;
            set
            {
                if (value == null || !SetProperty(ref _parameterName, value) || _isApplyingSelection)
                {
                    return;
                }

                string from = _selectedParameter;
                string to = value.Trim();

                if (to.Length == 0 || to == from)
                {
                    return;
                }

                _selectedParameter = to;

                PostEdit(state => state.RenameGraphParameter(new Name(from), new Name(to)));
            }
        }

        public int ParameterType
        {
            get => _parameterType;
            set
            {
                if (SetProperty(ref _parameterType, value) && !_isApplyingSelection && value >= 0)
                {
                    string name = _selectedParameter;

                    PostEdit(state => state.SetGraphParameterType(new Name(name), (uint)value));
                }
            }
        }

        public bool IsParameterVector2 => _parameterType == 3;

        public bool IsParameterBool => _parameterType == 1 || _parameterType == 2;

        public decimal ParameterDefaultX
        {
            get => _parameterDefaultX;
            set
            {
                if (SetProperty(ref _parameterDefaultX, value))
                {
                    PushParameterDefault();
                }
            }
        }

        public decimal ParameterDefaultY
        {
            get => _parameterDefaultY;
            set
            {
                if (SetProperty(ref _parameterDefaultY, value))
                {
                    PushParameterDefault();
                }
            }
        }

        public bool ParameterDefaultBool
        {
            get => _parameterDefaultX != 0;
            set => ParameterDefaultX = value ? 1 : 0;
        }

        private static EditorAnimationEditState? GetState() => EngineManager.EditorGame?.EditorSubsystem?.GetAnimationEditState();

        private void OnReadOnlyChanged()
        {
            OnPropertyChanged(nameof(IsReadOnly));
            OnPropertyChanged(nameof(IsEditable));
            OnPropertyChanged(nameof(CanCopyToProject));
            OnPropertyChanged(nameof(HasPreviewControls));
            OnPropertyChanged(nameof(HasApplyControls));
            OnPropertyChanged(nameof(ReadOnlyLabel));
        }

        private void OnMainWindowPropertyChanged(object? sender, PropertyChangedEventArgs e)
        {
            if (e.PropertyName == nameof(MainWindowViewModel.IsEditModeDirty))
            {
                UpdateTitle();
            }
        }

        private void UpdateTitle()
        {
            if (_isDebugging)
            {
                Title = $"Debug: {_graphName} ({_debugEntity?.Name.ToString() ?? string.Empty})";

                return;
            }

            bool isDirty = MainWindowViewModel.Instance?.IsEditModeDirty ?? false;

            Title = isDirty ? $"Graph: {_graphName} *" : $"Graph: {_graphName}";
        }

        // sim thread: the graph and running instance the document shows
        private (AnimationGraph? Graph, AnimationGraphInstance? Instance) ResolveSource()
        {
            if (_debugEntity != null)
            {
                if (!_debugEntity.IsValid || !_debugEntity.TryGetComponent(out AnimationComponent animationComponent))
                {
                    return (null, null);
                }

                return (animationComponent.Graph, animationComponent.GraphInstance);
            }

            EditorAnimationEditState? state = GetState();

            if (state == null || !state.IsEditingGraph())
            {
                return (null, null);
            }

            return (state.GetGraph(), state.GetPreviewGraphInstance());
        }

        public void SetActive(bool active)
        {
            if (active && IsOpen && !_isDebugging)
            {
                Refresh();
                return;
            }

            if (active && _isDebugging)
            {
                DetachDebug();
            }

            _graphChangedHandler?.Remove();
            _graphChangedHandler = null;

            if (!active)
            {
                if (!_isDebugging)
                {
                    Close();
                }

                return;
            }

            WeakReference<AnimationGraphDocumentViewModel> weakThis = new WeakReference<AnimationGraphDocumentViewModel>(this);

            _graphChangedHandler = GetState()?.GetOnGraphChangedDelegate().Bind(() =>
            {
                if (weakThis.TryGetTarget(out AnimationGraphDocumentViewModel? target))
                {
                    Dispatcher.UIThread.Post(target.Refresh);
                }
            });

            Open();
        }

        public void AttachDebug(Entity entity)
        {
            if (GetState()?.IsEditingGraph() ?? false)
            {
                return;
            }

            ReleaseCapture();

            _debugEntity = entity;
            _debugGraph = null;

            IsDebugging = true;

            if (IsOpen)
            {
                Refresh();
            }
            else
            {
                Open();
            }
        }

        public void DetachDebug()
        {
            if (!_isDebugging)
            {
                return;
            }

            _debugEntity = null;
            _debugGraph = null;

            IsDebugging = false;

            Close();
        }

        // an open debugger moves to a newly selected character that plays the same graph
        public void FollowSelection(Entity entity)
        {
            if (!_isDebugging || entity == _debugEntity)
            {
                return;
            }

            AnimationGraph? debugGraph = _debugGraph;

            _ = EngineManager.PostToSimThread(() =>
            {
                Entity? running = GetState()?.FindRunningGraphEntity(entity);

                if (running == null || running == _debugEntity || !running.TryGetComponent(out AnimationComponent animationComponent))
                {
                    return;
                }

                AnimationGraph? graph = animationComponent.Graph;

                if (graph != null && debugGraph != null && graph.Name.ToString() == debugGraph.Name.ToString())
                {
                    Dispatcher.UIThread.Post(() => AttachDebug(running));
                }
            });
        }

        private void Open()
        {
            IsOpen = true;

            _frameOnNextRefresh = true;
            _pollTimer.Start();

            Refresh();
        }

        private void Close()
        {
            ReleaseCapture();

            _pollTimer.Stop();
            SelectedNodeProperties?.Dispose();
            SelectedNodeProperties = null;
            SelectedNode = AnimationGraphNodeView.NoInput;
            _selectedNodes.Clear();
            _selectedParameter = string.Empty;
            SetStateMachine(AnimationGraphNodeView.NoInput, string.Empty);
            IsGraphReadOnly = false;
            IsOpen = false;

            PushInspector();
        }

        private void ReleaseCapture()
        {
            AnimationGraphInstance? instance = _capturedInstance;
            _capturedInstance = null;

            if (instance != null)
            {
                _ = EngineManager.PostToSimThread(() =>
                {
                    if (instance.IsValid)
                    {
                        instance.SetDebugCapture(false);
                    }
                });
            }
        }

        private static string FormatSample(Vec2f position, bool is2D)
        {
            if (is2D)
            {
                return string.Format(CultureInfo.InvariantCulture, "({0:0.##}, {1:0.##})", position.x, position.y);
            }

            return position.x.ToString("0.##", CultureInfo.InvariantCulture);
        }

        public void Refresh()
        {
            if (!IsOpen)
            {
                return;
            }

            int selectedNode = _selectedNode;
            int openStateMachine = _openStateMachine;
            int selectedTransition = _selectedTransition;
            bool isDebugging = _isDebugging;

            _ = EngineManager.PostToSimThread(() =>
            {
                (AnimationGraph? graph, AnimationGraphInstance? _) = ResolveSource();

                if (graph == null)
                {
                    return;
                }

                EditorAnimationEditState? state = isDebugging ? null : GetState();

                string graphName = graph.Name.ToString();
                uint rootNode = graph.GetRootNode();

                List<AnimationGraphNodeView> nodes = new List<AnimationGraphNodeView>();
                List<string> titles = new List<string>();

                for (uint nodeIndex = 0; nodeIndex < graph.NumNodes(); ++nodeIndex)
                {
                    string className = graph.GetNodeClassName(nodeIndex).ToString();
                    string name = graph.GetNodeName(nodeIndex).ToString();
                    string kind = NodeKinds.TryGetValue(className, out var kindInfo) ? kindInfo.Label : className;

                    titles.Add(name.Length > 0 ? name : kind);
                }

                for (uint nodeIndex = 0; nodeIndex < graph.NumNodes(); ++nodeIndex)
                {
                    string className = graph.GetNodeClassName(nodeIndex).ToString();
                    bool hasKind = NodeKinds.TryGetValue(className, out var kindInfo);
                    string kind = hasKind ? kindInfo.Label : className;
                    AnimationGraphNodeCategory category = hasKind ? kindInfo.Category : AnimationGraphNodeCategory.Modify;
                    bool isBlend = className == "AnimationBlend1DNode" || className == "AnimationBlend2DNode";
                    bool is2D = className == "AnimationBlend2DNode";

                    List<(string, int)> inputs = new List<(string, int)>();

                    for (uint slot = 0; slot < graph.GetNodeInputCount(nodeIndex); ++slot)
                    {
                        uint source = graph.GetNodeInput(nodeIndex, slot);
                        string label = graph.GetNodeInputName(nodeIndex, slot).ToString();

                        if (isBlend && state != null)
                        {
                            label = FormatSample(state.GetBlendSamplePosition(nodeIndex, slot), is2D);
                        }

                        inputs.Add((label, source == uint.MaxValue ? AnimationGraphNodeView.NoInput : (int)source));
                    }

                    string subject = graph.GetNodeSubject(nodeIndex).ToString();
                    string detail = subject;

                    if (className == "AnimationClipNode" && subject.Length > 0)
                    {
                        detail = string.Format(CultureInfo.InvariantCulture, "{0}  {1:0.00}s", subject, graph.GetNodeClipLength(nodeIndex));
                    }
                    else if (isBlend && subject.Length > 0)
                    {
                        detail = $"by {subject}";
                    }

                    Vec2f position = graph.GetNodeEditorPosition(nodeIndex);

                    nodes.Add(new AnimationGraphNodeView
                    {
                        Index = (int)nodeIndex,
                        Title = titles[(int)nodeIndex],
                        Kind = kind,
                        Detail = detail,
                        Category = category,
                        Position = new Point(position.x, position.y),
                        Inputs = inputs,
                        Parameters = graph.GetNodeParameters(nodeIndex).Cast<Name>().Select(p => p.ToString()).ToList(),
                        CanAddInputs = graph.CanNodeAddInputs(nodeIndex),
                        IsOutput = nodeIndex == rootNode,
                        IsStateMachine = className == "AnimationStateMachineNode"
                    });
                }

                List<(string Name, uint Type)> parameters = new List<(string, uint)>();

                foreach (Name parameterName in graph.GetParameterNames())
                {
                    parameters.Add((parameterName.ToString(), graph.GetParameterType(parameterName)));
                }

                if (selectedNode >= nodes.Count)
                {
                    selectedNode = AnimationGraphNodeView.NoInput;
                }

                AnimationGraphNode? selectedObject = selectedNode >= 0 ? graph.GetNodeObject((uint)selectedNode) : null;

                AnimationStateMachineNode? stateMachine = openStateMachine >= 0 ? graph.GetNodeObject((uint)openStateMachine) as AnimationStateMachineNode : null;
                StateMachineSnapshot? stateMachineSnapshot = stateMachine != null ? StateMachineSnapshot.Read(stateMachine, nodes, selectedTransition) : null;

                bool isGraphReadOnly = state?.IsGraphReadOnly() ?? false;

                List<Entity> previewEntities = new List<Entity>();
                List<string> previewNames = new List<string>();
                int previewIndex = -1;
                List<string> clipChoices = new List<string>();

                if (state != null)
                {
                    Entity? previewSource = state.GetPreviewSource();

                    foreach (Entity candidate in state.GetGraphPreviewCandidates())
                    {
                        if (candidate == previewSource)
                        {
                            previewIndex = previewEntities.Count;
                        }

                        string parentName = candidate.GetParent()?.Name.ToString() ?? string.Empty;

                        previewEntities.Add(candidate);
                        previewNames.Add(parentName.Length > 0 ? $"{candidate.Name} ({parentName})" : candidate.Name.ToString());
                    }

                    clipChoices = state.GetClipNames().Cast<Name>().Select(n => n.ToString()).ToList();
                }

                Dispatcher.UIThread.Post(() =>
                {
                    _debugGraph = _isDebugging ? graph : null;
                    _graphName = graphName;

                    UpdateTitle();

                    IsGraphReadOnly = isGraphReadOnly;
                    Nodes = nodes;
                    ClipChoices = clipChoices;

                    ApplyPreviewChoices(previewEntities, previewNames, previewIndex);

                    if (!Parameters.Select(p => (p.Name, p.Type)).SequenceEqual(parameters))
                    {
                        Parameters.Clear();

                        foreach ((string name, uint type) in parameters)
                        {
                            Parameters.Add(new AnimationGraphParameterViewModel(this, name, type) { IsSelected = name == _selectedParameter });
                        }
                    }

                    if (_selectedParameter.Length > 0 && !Parameters.Any(p => p.Name == _selectedParameter))
                    {
                        _selectedParameter = string.Empty;
                    }

                    _selectedNodes.RemoveWhere(index => index >= nodes.Count);

                    if (stateMachineSnapshot != null)
                    {
                        ApplyStateMachine(stateMachineSnapshot);
                    }
                    else if (_openStateMachine >= 0)
                    {
                        SetStateMachine(AnimationGraphNodeView.NoInput, string.Empty);
                    }

                    SelectNodeObject(selectedNode, selectedObject);

                    if (_selectedParameter.Length > 0)
                    {
                        RefreshParameterDetails();
                    }

                    GraphChanged?.Invoke();

                    if (_frameOnNextRefresh && nodes.Count > 0)
                    {
                        _frameOnNextRefresh = false;
                        FrameRequested?.Invoke();
                    }

                    PollPreview();
                });
            });
        }

        private void ApplyPreviewChoices(List<Entity> entities, List<string> names, int selectedIndex)
        {
            _isApplyingPreview = true;

            _previewEntities = entities;

            if (!PreviewChoices.SequenceEqual(names))
            {
                PreviewChoices.Clear();

                foreach (string name in names)
                {
                    PreviewChoices.Add(name);
                }
            }

            SelectedPreviewIndex = selectedIndex;

            _isApplyingPreview = false;
        }

        private sealed class StateMachineSnapshot
        {
            public List<AnimationStateView> States = new List<AnimationStateView>();
            public List<AnimationTransitionView> Transitions = new List<AnimationTransitionView>();
            public List<(int From, int To, float Duration, int Curve, int Priority, bool Interruptible, bool Inertialize, List<(string Parameter, int Op, float Value)> Conditions)> TransitionDetails = new();

            public static StateMachineSnapshot Read(AnimationStateMachineNode stateMachine, List<AnimationGraphNodeView> nodes, int selectedTransition)
            {
                StateMachineSnapshot snapshot = new StateMachineSnapshot();

                uint entryState = stateMachine.GetEntryState();

                for (uint stateIndex = 0; stateIndex < stateMachine.NumStates(); ++stateIndex)
                {
                    uint input = stateMachine.GetStateInput(stateIndex);
                    Vec2f position = stateMachine.GetStatePosition(stateIndex);

                    snapshot.States.Add(new AnimationStateView
                    {
                        Index = (int)stateIndex,
                        Name = stateMachine.GetStateName(stateIndex).ToString(),
                        InputIndex = input < nodes.Count ? (int)input : AnimationGraphNodeView.NoInput,
                        InputTitle = input < nodes.Count ? nodes[(int)input].Title : "(no pose)",
                        Position = new Point(position.x, position.y),
                        IsEntry = stateIndex == entryState
                    });
                }

                for (uint transitionIndex = 0; transitionIndex < stateMachine.NumTransitions(); ++transitionIndex)
                {
                    bool fromAnyState = stateMachine.IsTransitionFromAnyState(transitionIndex);
                    int from = fromAnyState ? AnimationStateView.AnyState : (int)stateMachine.GetTransitionFrom(transitionIndex);
                    int to = (int)stateMachine.GetTransitionTo(transitionIndex);

                    List<(string, int, float)> conditions = new List<(string, int, float)>();

                    for (uint conditionIndex = 0; conditionIndex < stateMachine.NumConditions(transitionIndex); ++conditionIndex)
                    {
                        conditions.Add((
                            stateMachine.GetConditionParameter(transitionIndex, conditionIndex).ToString(),
                            (int)stateMachine.GetConditionOp(transitionIndex, conditionIndex),
                            stateMachine.GetConditionValue(transitionIndex, conditionIndex)));
                    }

                    snapshot.Transitions.Add(new AnimationTransitionView
                    {
                        Index = (int)transitionIndex,
                        From = from,
                        To = to,
                        Parameters = conditions.Select(c => c.Item1).Where(p => p.Length > 0).Distinct().ToList()
                    });

                    snapshot.TransitionDetails.Add((
                        from,
                        to,
                        stateMachine.GetTransitionDuration(transitionIndex),
                        (int)stateMachine.GetTransitionCurve(transitionIndex),
                        stateMachine.GetTransitionPriority(transitionIndex),
                        stateMachine.IsTransitionInterruptible(transitionIndex),
                        stateMachine.GetTransitionMode(transitionIndex) == 1,
                        conditions));
                }

                return snapshot;
            }
        }

        private string StateLabel(int stateIndex)
        {
            if (stateIndex == AnimationStateView.AnyState)
            {
                return "Any State";
            }

            return States.FirstOrDefault(s => s.Index == stateIndex)?.Name ?? "?";
        }

        private void ApplyStateMachine(StateMachineSnapshot snapshot)
        {
            States = snapshot.States;
            Transitions = snapshot.Transitions;
            _transitionDetails = snapshot.TransitionDetails;

            if (_selectedState >= States.Count)
            {
                _selectedState = AnimationStateView.AnyState;
            }

            if (_selectedTransition >= Transitions.Count)
            {
                _selectedTransition = AnimationGraphNodeView.NoInput;
            }

            ApplySelectionDetails();
        }

        private void ApplySelectionDetails()
        {
            _isApplyingSelection = true;

            if (_selectedState >= 0 && _selectedState < States.Count)
            {
                AnimationStateView stateView = States[_selectedState];

                StateName = stateView.Name;
                StateSourceTitle = stateView.InputTitle;
                IsSelectedStateEntry = stateView.IsEntry;
            }

            Conditions.Clear();

            if (_selectedTransition >= 0 && _selectedTransition < _transitionDetails.Count)
            {
                var details = _transitionDetails[_selectedTransition];

                TransitionTitle = $"{StateLabel(details.From)} → {StateLabel(details.To)}";
                TransitionDuration = Math.Round((decimal)details.Duration, 3);
                TransitionCurve = details.Curve;
                TransitionPriority = details.Priority;
                TransitionInterruptible = details.Interruptible;
                TransitionInertialize = details.Inertialize;

                for (int conditionIndex = 0; conditionIndex < details.Conditions.Count; ++conditionIndex)
                {
                    (string parameter, int op, float value) = details.Conditions[conditionIndex];

                    Conditions.Add(new AnimationTransitionConditionViewModel(this, _selectedTransition, conditionIndex, parameter, op, value));
                }
            }

            _isApplyingSelection = false;

            OnPropertyChanged(nameof(SelectedState));
            OnPropertyChanged(nameof(SelectedTransition));
            OnPropertyChanged(nameof(IsStateSelected));
            OnPropertyChanged(nameof(IsTransitionSelected));

            PushInspector();
        }

        private void SetStateMachine(int nodeIndex, string title)
        {
            _openStateMachine = nodeIndex;
            _selectedState = AnimationStateView.AnyState;
            _selectedTransition = AnimationGraphNodeView.NoInput;

            if (nodeIndex < 0)
            {
                States = new List<AnimationStateView>();
                Transitions = new List<AnimationTransitionView>();
                _transitionDetails = new();
            }

            StateMachineTitle = title;

            OnPropertyChanged(nameof(OpenStateMachine));
            OnPropertyChanged(nameof(IsInStateMachine));
            OnPropertyChanged(nameof(HasSelection));
            OnPropertyChanged(nameof(IsStateSelected));
            OnPropertyChanged(nameof(IsTransitionSelected));

            PushInspector();
        }

        public void OpenStateMachineNode(int nodeIndex)
        {
            AnimationGraphNodeView? node = Nodes.FirstOrDefault(n => n.Index == nodeIndex);

            if (node == null || !node.IsStateMachine)
            {
                return;
            }

            SetStateMachine(nodeIndex, node.Title);
            Refresh();
        }

        public void CloseStateMachine()
        {
            if (_openStateMachine < 0)
            {
                return;
            }

            SetStateMachine(AnimationGraphNodeView.NoInput, string.Empty);

            GraphChanged?.Invoke();
        }

        // leaves the state machine and shows the node a state plays
        public void OpenStateInput(int stateIndex)
        {
            AnimationStateView? stateView = States.FirstOrDefault(s => s.Index == stateIndex);
            AnimationGraphNodeView? input = stateView != null ? Nodes.FirstOrDefault(n => n.Index == stateView.InputIndex) : null;

            CloseStateMachine();

            if (input != null)
            {
                Select(input.Index);
                FocusNodeRequested?.Invoke(input.Index);
            }
        }

        public void SelectState(int stateIndex)
        {
            _selectedState = stateIndex;
            _selectedTransition = AnimationGraphNodeView.NoInput;

            ClearParameterSelection();
            ApplySelectionDetails();
            GraphChanged?.Invoke();
        }

        public void SelectTransition(int transitionIndex)
        {
            _selectedTransition = transitionIndex;
            _selectedState = AnimationStateView.AnyState;

            ClearParameterSelection();
            ApplySelectionDetails();
            GraphChanged?.Invoke();
        }

        public void Select(int nodeIndex)
        {
            SetNodeSelection(nodeIndex >= 0 ? new[] { nodeIndex } : Array.Empty<int>(), nodeIndex);
        }

        public void SetNodeSelection(IEnumerable<int> nodeIndices, int primary)
        {
            if (!IsOpen)
            {
                return;
            }

            _selectedNodes = new HashSet<int>(nodeIndices);

            if (primary >= 0)
            {
                _selectedNodes.Add(primary);
            }
            else if (_selectedNodes.Count > 0)
            {
                primary = _selectedNodes.Min();
            }

            if (primary >= 0)
            {
                ClearParameterSelection();
                ClearSceneSelection();
            }

            int nodeIndex = primary;

            _ = EngineManager.PostToSimThread(() =>
            {
                (AnimationGraph? graph, AnimationGraphInstance? _) = ResolveSource();
                AnimationGraphNode? node = graph != null && nodeIndex >= 0 ? graph.GetNodeObject((uint)nodeIndex) : null;

                Dispatcher.UIThread.Post(() =>
                {
                    SelectNodeObject(node != null ? nodeIndex : AnimationGraphNodeView.NoInput, node);
                    GraphChanged?.Invoke();
                });
            });
        }

        private static void ClearSceneSelection()
        {
            _ = EngineManager.PostToSimThread(() =>
            {
                EngineManager.EditorGame?.EditorSubsystem?.ClearSelection();
            });
        }

        private void SelectNodeObject(int nodeIndex, AnimationGraphNode? node)
        {
            SelectedNode = nodeIndex;

            if (nodeIndex < 0)
            {
                _selectedNodes.Clear();
            }

            SelectedNodeProperties?.Dispose();
            SelectedNodeProperties = null;

            if (node != null && node.IsValid && !_isDebugging)
            {
                EditorAnimationEditState? state = GetState();

                SelectedNodeProperties = new ComponentSubObjectViewModel(
                    Nodes.FirstOrDefault(n => n.Index == nodeIndex)?.Kind ?? string.Empty,
                    node,
                    postWriteCallback: () => state?.CommitGraphEdit("Edit Graph Node"));
            }

            OnPropertyChanged(nameof(HasSelection));

            RefreshBlendSpace();
            PushInspector();
        }

        public void SelectParameter(string name)
        {
            _selectedParameter = name;

            foreach (AnimationGraphParameterViewModel parameter in Parameters)
            {
                parameter.IsSelected = parameter.Name == name;
            }

            if (name.Length > 0)
            {
                _selectedState = AnimationStateView.AnyState;
                _selectedTransition = AnimationGraphNodeView.NoInput;

                SelectNodeObject(AnimationGraphNodeView.NoInput, null);
                ClearSceneSelection();
                RefreshParameterDetails();
            }

            OnPropertyChanged(nameof(IsParameterSelected));

            PushInspector();
            GraphChanged?.Invoke();
        }

        private void ClearParameterSelection()
        {
            if (_selectedParameter.Length == 0)
            {
                return;
            }

            _selectedParameter = string.Empty;

            foreach (AnimationGraphParameterViewModel parameter in Parameters)
            {
                parameter.IsSelected = false;
            }

            OnPropertyChanged(nameof(IsParameterSelected));
        }

        private void RefreshParameterDetails()
        {
            string name = _selectedParameter;

            _ = EngineManager.PostToSimThread(() =>
            {
                (AnimationGraph? graph, AnimationGraphInstance? _) = ResolveSource();

                if (graph == null)
                {
                    return;
                }

                uint type = graph.GetParameterType(new Name(name));
                Vec2f defaultValue = graph.GetParameterDefault(new Name(name));

                Dispatcher.UIThread.Post(() =>
                {
                    if (name != _selectedParameter || type == uint.MaxValue)
                    {
                        return;
                    }

                    _isApplyingSelection = true;

                    ParameterName = name;
                    ParameterType = (int)type;
                    _parameterDefaultX = Math.Round((decimal)defaultValue.x, 3);
                    _parameterDefaultY = Math.Round((decimal)defaultValue.y, 3);

                    _isApplyingSelection = false;

                    OnPropertyChanged(nameof(ParameterDefaultX));
                    OnPropertyChanged(nameof(ParameterDefaultY));
                    OnPropertyChanged(nameof(ParameterDefaultBool));
                    OnPropertyChanged(nameof(IsParameterVector2));
                    OnPropertyChanged(nameof(IsParameterBool));
                });
            });
        }

        private void PushParameterDefault()
        {
            if (_isApplyingSelection || _selectedParameter.Length == 0)
            {
                return;
            }

            string name = _selectedParameter;
            Vec2f value = new Vec2f((float)_parameterDefaultX, (float)_parameterDefaultY);

            OnPropertyChanged(nameof(ParameterDefaultBool));

            PostEdit(state => state.SetGraphParameterDefault(new Name(name), value));
        }

        public bool UsesSelectedParameter(IReadOnlyList<string> parameters)
        {
            return _selectedParameter.Length > 0 && parameters.Contains(_selectedParameter);
        }

        private void PushInspector()
        {
            InspectorViewModel? inspector = MainWindowViewModel.Instance?.Inspector;

            if (inspector == null)
            {
                return;
            }

            string heading = string.Empty;
            bool hasContent = IsOpen;

            if (hasContent && IsTransitionSelected)
            {
                heading = TransitionTitle;
            }
            else if (hasContent && IsStateSelected)
            {
                heading = $"State: {StateName}";
            }
            else if (hasContent && _selectedParameter.Length > 0)
            {
                heading = $"Parameter: {_selectedParameter}";
            }
            else if (hasContent && HasSelection && Nodes.FirstOrDefault(n => n.Index == _selectedNode) is { } node)
            {
                heading = node.Title == node.Kind ? node.Kind : $"{node.Title}  ({node.Kind})";
            }
            else
            {
                hasContent = false;
            }

            Inspector.NotifyChanged();

            inspector.SetEditModeContent(heading, hasContent ? Inspector : null);
        }

        private void RefreshBlendSpace()
        {
            EditorAnimationEditState? state = GetState();
            int nodeIndex = _selectedNode;

            if (state == null || nodeIndex < 0 || _isDebugging)
            {
                ApplyBlendSpace(false, string.Empty, false, new List<(Vector, string)>());

                return;
            }

            AnimationGraphNodeView? view = Nodes.FirstOrDefault(n => n.Index == nodeIndex);

            _ = EngineManager.PostToSimThread(() =>
            {
                string parameter = state.GetBlendParameter((uint)nodeIndex).ToString();
                bool is2D = view?.Kind == NodeKinds["AnimationBlend2DNode"].Label;
                bool isBlend = view?.Kind == NodeKinds["AnimationBlend1DNode"].Label || is2D;

                List<(Vector, string)> samples = new List<(Vector, string)>();

                if (isBlend && view != null)
                {
                    for (int sample = 0; sample < view.Inputs.Count; ++sample)
                    {
                        Vec2f position = state.GetBlendSamplePosition((uint)nodeIndex, (uint)sample);
                        int source = view.Inputs[sample].Source;
                        string label = Nodes.FirstOrDefault(n => n.Index == source)?.Title ?? "(none)";

                        samples.Add((new Vector(position.x, position.y), label));
                    }
                }

                Dispatcher.UIThread.Post(() => ApplyBlendSpace(isBlend, parameter, is2D, samples));
            });
        }

        private void ApplyBlendSpace(bool isBlend, string parameter, bool is2D, List<(Vector, string)> samples)
        {
            IsBlendSelected = isBlend && !IsInStateMachine;
            BlendParameter = parameter;
            IsBlend2D = is2D;
            BlendSamples = samples;

            OnPropertyChanged(nameof(BlendSamples));
            OnPropertyChanged(nameof(IsBlendSelected));
        }

        private void PollPreview()
        {
            if (!IsOpen)
            {
                return;
            }

            List<string> parameterNames = Parameters.Select(p => p.Name).ToList();
            string blendParameter = _blendParameter;
            int openStateMachine = _openStateMachine;
            int numNodes = Nodes.Count;
            bool isDebugging = _isDebugging;

            _ = EngineManager.PostToSimThread(() =>
            {
                (AnimationGraph? graph, AnimationGraphInstance? instance) = ResolveSource();

                bool isPlaying = !isDebugging && (GetState()?.IsPlaying() ?? false);

                if (graph == null || instance == null || !instance.IsValid)
                {
                    Dispatcher.UIThread.Post(() => IsPreviewPlaying = isPlaying);

                    return;
                }

                if (!instance.IsDebugCaptured())
                {
                    instance.SetDebugCapture(true);
                }

                List<float> weights = new List<float>();
                List<string> statuses = new List<string>();

                for (uint nodeIndex = 0; nodeIndex < numNodes; ++nodeIndex)
                {
                    weights.Add(instance.GetDebugNodeWeight(nodeIndex));
                    statuses.Add(instance.GetDebugNodeStatus(nodeIndex) ?? string.Empty);
                }

                List<Vec2f> values = parameterNames.Select(name => instance.GetDebugValue(new Name(name))).ToList();
                Vec2f cursor = blendParameter.Length > 0 ? instance.GetDebugValue(new Name(blendParameter)) : new Vec2f(0.0f, 0.0f);

                int currentState = AnimationStateView.AnyState;
                int previousState = AnimationStateView.AnyState;
                int activeTransition = AnimationGraphNodeView.NoInput;
                float progress = 0.0f;

                if (openStateMachine >= 0)
                {
                    uint current = instance.GetDebugCurrentState((uint)openStateMachine);
                    uint transition = instance.GetDebugTransition((uint)openStateMachine);

                    currentState = current == uint.MaxValue ? AnimationStateView.AnyState : (int)current;
                    activeTransition = transition == uint.MaxValue ? AnimationGraphNodeView.NoInput : (int)transition;
                    progress = instance.GetDebugStateProgress((uint)openStateMachine);

                    if (activeTransition >= 0)
                    {
                        previousState = (int)instance.GetDebugPreviousState((uint)openStateMachine);
                    }
                }

                Dispatcher.UIThread.Post(() =>
                {
                    if (!ReferenceEquals(_capturedInstance, instance))
                    {
                        _capturedInstance = instance;
                    }

                    IsPreviewPlaying = isPlaying;

                    for (int nodeIndex = 0; nodeIndex < Nodes.Count && nodeIndex < weights.Count; ++nodeIndex)
                    {
                        Nodes[nodeIndex].Weight = weights[nodeIndex];
                        Nodes[nodeIndex].Status = statuses[nodeIndex];
                    }

                    for (int parameterIndex = 0; parameterIndex < Parameters.Count && parameterIndex < values.Count; ++parameterIndex)
                    {
                        Parameters[parameterIndex].ApplyValue(values[parameterIndex]);
                    }

                    foreach (AnimationStateView stateView in States)
                    {
                        stateView.IsCurrent = stateView.Index == currentState;
                        stateView.IsPrevious = stateView.Index == previousState;
                        stateView.Progress = stateView.IsCurrent ? progress : 0.0f;
                    }

                    foreach (AnimationTransitionView transitionView in Transitions)
                    {
                        transitionView.IsActive = transitionView.Index == activeTransition;
                    }

                    BlendCursor = new Vector(cursor.x, cursor.y);

                    PreviewChanged?.Invoke();
                });
            });
        }

        private void PostEdit(Action<EditorAnimationEditState> action, bool allowReadOnly = false)
        {
            EditorAnimationEditState? state = GetState();

            if (state != null && !_isDebugging && (allowReadOnly || !_isGraphReadOnly))
            {
                _ = EngineManager.PostToSimThread(() => action(state));
            }
        }

        private void CopyToProject()
        {
            PostEdit(state => state.CopyGraphToProject(), allowReadOnly: true);
        }

        private void TogglePlay()
        {
            bool play = !_isPreviewPlaying;

            IsPreviewPlaying = play;

            PostEdit(state => state.SetPlaying(play), allowReadOnly: true);
        }

        private void Restart()
        {
            IsPreviewPlaying = true;

            PostEdit(state =>
            {
                state.SetTime(0.0f);
                state.SetPlaying(true);
            }, allowReadOnly: true);
        }

        public void AddNode(string className, Point position, int connectFrom = AnimationGraphNodeView.NoInput)
        {
            PostEdit(state =>
            {
                uint nodeIndex = state.AddGraphNode(new Name(className), new Vec2f((float)position.X, (float)position.Y));

                ConnectNew(state, nodeIndex, connectFrom);
            });
        }

        public void AddClipNode(string clipName, Point position, int connectFrom = AnimationGraphNodeView.NoInput)
        {
            PostEdit(state =>
            {
                uint nodeIndex = state.AddGraphClipNodeByName(new Name(clipName), new Vec2f((float)position.X, (float)position.Y));

                ConnectNew(state, nodeIndex, connectFrom);
            });
        }

        private void ConnectNew(EditorAnimationEditState state, uint nodeIndex, int connectFrom)
        {
            if (nodeIndex == uint.MaxValue)
            {
                return;
            }

            if (connectFrom >= 0)
            {
                state.ConnectGraphNodes((uint)connectFrom, nodeIndex, 0);
            }

            int selected = (int)nodeIndex;

            Dispatcher.UIThread.Post(() => Select(selected));
        }

        public void RemoveNodes(IEnumerable<int> nodeIndices)
        {
            if (!IsEditable)
            {
                return;
            }

            List<int> indices = nodeIndices.Distinct().OrderByDescending(i => i).ToList();

            if (indices.Count == 0)
            {
                return;
            }

            SelectNodeObject(AnimationGraphNodeView.NoInput, null);

            PostEdit(state =>
            {
                foreach (int nodeIndex in indices)
                {
                    state.RemoveGraphNode((uint)nodeIndex);
                }
            });
        }

        public void RemoveNode(int nodeIndex) => RemoveNodes(new[] { nodeIndex });

        public void RenameNode(int nodeIndex, string name) => PostEdit(state => state.RenameGraphNode((uint)nodeIndex, new Name(name.Trim())));

        public void DuplicateNodes(IEnumerable<int> nodeIndices, Vector offset)
        {
            List<int> indices = nodeIndices.Distinct().OrderBy(i => i).ToList();

            if (indices.Count == 0)
            {
                return;
            }

            PostEdit(state =>
            {
                List<int> created = new List<int>();

                foreach (int nodeIndex in indices)
                {
                    uint duplicate = state.DuplicateGraphNode((uint)nodeIndex, new Vec2f((float)offset.X, (float)offset.Y));

                    if (duplicate != uint.MaxValue)
                    {
                        created.Add((int)duplicate);
                    }
                }

                Dispatcher.UIThread.Post(() => SetNodeSelection(created, created.Count > 0 ? created[0] : AnimationGraphNodeView.NoInput));
            });
        }

        public void CopyNodes(IEnumerable<int> nodeIndices)
        {
            _copiedNodes = nodeIndices.Distinct().ToList();
        }

        public void PasteNodes(Point at)
        {
            List<AnimationGraphNodeView> copied = Nodes.Where(n => _copiedNodes.Contains(n.Index)).ToList();

            if (copied.Count == 0)
            {
                return;
            }

            Point origin = new Point(copied.Min(n => n.Position.X), copied.Min(n => n.Position.Y));

            DuplicateNodes(copied.Select(n => n.Index), at - origin);
        }

        public void Connect(int fromNode, int toNode, int slot) => PostEdit(state => state.ConnectGraphNodes((uint)fromNode, (uint)toNode, (uint)slot));

        public void Disconnect(int nodeIndex, int slot) => PostEdit(state => state.DisconnectGraphInput((uint)nodeIndex, (uint)slot));

        public void Rewire(int nodeIndex, int slot, int toNode, int toSlot) => PostEdit(state => state.RewireGraphInput((uint)nodeIndex, (uint)slot, (uint)toNode, (uint)toSlot));

        public void SetOutput(int nodeIndex) => PostEdit(state => state.SetGraphRootNode((uint)nodeIndex));

        public void MoveNode(int nodeIndex, Point position) => PostEdit(state => state.MoveGraphNode((uint)nodeIndex, new Vec2f((float)position.X, (float)position.Y)));

        public void MoveNodes(IReadOnlyList<(int Index, Point Position)> moves)
        {
            PostEdit(state =>
            {
                foreach ((int index, Point position) in moves)
                {
                    state.MoveGraphNode((uint)index, new Vec2f((float)position.X, (float)position.Y));
                }
            });
        }

        public bool WouldCycle(int fromNode, int toNode)
        {
            Dictionary<int, AnimationGraphNodeView> byIndex = Nodes.ToDictionary(n => n.Index);
            Stack<int> pending = new Stack<int>();
            HashSet<int> visited = new HashSet<int>();

            pending.Push(fromNode);

            while (pending.Count > 0)
            {
                int current = pending.Pop();

                if (current == toNode)
                {
                    return true;
                }

                if (!visited.Add(current) || !byIndex.TryGetValue(current, out AnimationGraphNodeView? view))
                {
                    continue;
                }

                foreach ((string _, int source) in view.Inputs)
                {
                    if (source >= 0)
                    {
                        pending.Push(source);
                    }
                }
            }

            return false;
        }

        public void MoveBlendSample(int sample, Vector position)
        {
            int nodeIndex = _selectedNode;

            PostEdit(state => state.SetBlendSamplePosition((uint)nodeIndex, (uint)sample, new Vec2f((float)position.X, (float)position.Y)));
        }

        public void SetBlendCursor(Vector position)
        {
            if (_isDebugging)
            {
                return;
            }

            BlendCursor = position;

            if (_blendParameter.Length > 0)
            {
                SetParameterValue(_blendParameter, new Vec2f((float)position.X, (float)position.Y));
            }
        }

        public void SetParameterValue(string name, Vec2f value) => PostEdit(state => state.SetPreviewParameter(new Name(name), value), allowReadOnly: true);

        public void TriggerParameter(string name) => PostEdit(state => state.TriggerPreviewParameter(new Name(name)), allowReadOnly: true);

        public void RemoveParameter(string name)
        {
            if (name.Length == 0)
            {
                return;
            }

            if (name == _selectedParameter)
            {
                SelectParameter(string.Empty);
            }

            PostEdit(state => state.RemoveGraphParameter(new Name(name)));
        }

        private void AddParameter()
        {
            string name = _newParameterName.Trim();
            uint type = (uint)Math.Clamp(_newParameterType, 0, ParameterTypeLabels.Count - 1);

            if (name.Length == 0)
            {
                return;
            }

            NewParameterName = string.Empty;

            PostEdit(state => state.AddGraphParameter(new Name(name), type));
        }

        public void AddState(Point position)
        {
            int stateMachine = _openStateMachine;

            PostEdit(state => state.AddGraphState((uint)stateMachine, new Vec2f((float)position.X, (float)position.Y)));
        }

        public void RemoveState(int stateIndex)
        {
            int stateMachine = _openStateMachine;

            if (stateIndex < 0)
            {
                return;
            }

            _selectedState = AnimationStateView.AnyState;

            PostEdit(state => state.RemoveGraphState((uint)stateMachine, (uint)stateIndex));
        }

        public void MoveState(int stateIndex, Point position)
        {
            int stateMachine = _openStateMachine;

            PostEdit(state => state.MoveGraphState((uint)stateMachine, (uint)stateIndex, new Vec2f((float)position.X, (float)position.Y)));
        }

        public void SetEntryState(int stateIndex)
        {
            int stateMachine = _openStateMachine;

            if (stateIndex >= 0)
            {
                PostEdit(state => state.SetGraphEntryState((uint)stateMachine, (uint)stateIndex));
            }
        }

        public void AddTransition(int fromState, int toState)
        {
            int stateMachine = _openStateMachine;
            bool fromAnyState = fromState == AnimationStateView.AnyState;

            if (toState < 0 || fromState == toState)
            {
                return;
            }

            PostEdit(state =>
            {
                uint transitionIndex = state.AddGraphTransition((uint)stateMachine, fromAnyState ? 0u : (uint)fromState, fromAnyState, (uint)toState, 0.2f);

                if (transitionIndex != uint.MaxValue)
                {
                    Dispatcher.UIThread.Post(() => _selectedTransition = (int)transitionIndex);
                }
            });
        }

        public void RemoveTransition(int transitionIndex)
        {
            int stateMachine = _openStateMachine;

            if (transitionIndex < 0)
            {
                return;
            }

            _selectedTransition = AnimationGraphNodeView.NoInput;

            PostEdit(state => state.RemoveGraphTransition((uint)stateMachine, (uint)transitionIndex));
        }

        private void PushTransition()
        {
            if (_isApplyingSelection || _selectedTransition < 0)
            {
                return;
            }

            int stateMachine = _openStateMachine;
            int transitionIndex = _selectedTransition;
            float duration = (float)_transitionDuration;
            uint curve = (uint)Math.Clamp(_transitionCurve, 0, BlendCurves.Count - 1);
            int priority = (int)_transitionPriority;
            bool interruptible = _transitionInterruptible;
            uint mode = _transitionInertialize ? 1u : 0u;

            PostEdit(state => state.SetGraphTransition((uint)stateMachine, (uint)transitionIndex, duration, curve, priority, interruptible, mode));
        }

        private void AddCondition()
        {
            int stateMachine = _openStateMachine;
            int transitionIndex = _selectedTransition;
            string parameter = Parameters.FirstOrDefault()?.Name ?? string.Empty;

            if (transitionIndex >= 0)
            {
                PostEdit(state => state.AddGraphTransitionCondition((uint)stateMachine, (uint)transitionIndex, new Name(parameter), 0, 0.0f));
            }
        }

        public void SetCondition(int transitionIndex, int conditionIndex, string parameter, int op, float value)
        {
            int stateMachine = _openStateMachine;

            if (!_isApplyingSelection)
            {
                PostEdit(state => state.SetGraphTransitionCondition((uint)stateMachine, (uint)transitionIndex, (uint)conditionIndex, new Name(parameter), (uint)op, value));
            }
        }

        public void RemoveCondition(int transitionIndex, int conditionIndex)
        {
            int stateMachine = _openStateMachine;

            PostEdit(state => state.RemoveGraphTransitionCondition((uint)stateMachine, (uint)transitionIndex, (uint)conditionIndex));
        }
    }
}
