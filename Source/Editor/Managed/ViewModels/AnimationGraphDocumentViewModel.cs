using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Windows.Input;
using Avalonia;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public sealed class AnimationGraphNodeView
    {
        public const int NoInput = -1;

        public int Index { get; init; }
        public string Title { get; init; } = string.Empty;
        public string Kind { get; init; } = string.Empty;
        public Point Position { get; set; }
        public IReadOnlyList<(string Name, int Source)> Inputs { get; init; } = Array.Empty<(string, int)>();
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
    }

    public class AnimationGraphParameterViewModel : ViewModelBase
    {
        private readonly AnimationGraphDocumentViewModel _document;

        private decimal _x;
        private decimal _y;
        private bool _isApplyingValue;

        public AnimationGraphParameterViewModel(AnimationGraphDocumentViewModel document, string name, uint type)
        {
            _document = document;

            Name = name;
            Type = type;

            RemoveCommand = new RelayCommand(() => _document.RemoveParameter(Name));
            FireCommand = new RelayCommand(() => _document.TriggerParameter(Name));
        }

        public string Name { get; }

        public uint Type { get; }

        public bool IsFloat => Type == 0;
        public bool IsBool => Type == 1;
        public bool IsTrigger => Type == 2;
        public bool IsVector2 => Type == 3;

        public ICommand RemoveCommand { get; }
        public ICommand FireCommand { get; }

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

    public class AnimationGraphDocumentViewModel : ViewModelBase
    {
        private const int PollIntervalMs = 150;

        private static readonly Dictionary<string, string> NodeKindLabels = new()
        {
            ["AnimationClipNode"] = "Clip",
            ["AnimationBlend1DNode"] = "Blend 1D",
            ["AnimationBlend2DNode"] = "Blend 2D",
            ["AnimationLayerNode"] = "Layer",
            ["AnimationAdditiveNode"] = "Additive",
            ["AnimationChainTwistNode"] = "Chain Twist",
            ["AnimationStateMachineNode"] = "State Machine",
            ["AnimationSlotNode"] = "Slot",
            ["AnimationFootIKNode"] = "Foot IK",
            ["AnimationMotionMatchingNode"] = "Motion Matching"
        };

        public static readonly IReadOnlyList<string> ConditionOps = new[]
        {
            ">", "<", "=", "!=", "is true", "is false", "triggered", "time in state >=", "time left <", "normalized time >="
        };

        public static readonly IReadOnlyList<string> BlendCurves = new[] { "Linear", "Smooth Step", "Ease In", "Ease Out" };

        private readonly DispatcherTimer _pollTimer;

        private DelegateHandler? _graphChangedHandler;

        private Entity? _debugEntity;
        private AnimationGraph? _debugGraph;
        private AnimationGraphInstance? _capturedInstance;

        private bool _isOpen;
        private bool _isDebugging;
        private string _title = string.Empty;
        private int _selectedNode = AnimationGraphNodeView.NoInput;
        private ComponentSubObjectViewModel? _selectedNodeProperties;
        private string _blendParameter = string.Empty;
        private bool _isBlend2D;
        private Vector _blendCursor;
        private string _newParameterName = string.Empty;
        private int _newParameterType;

        private int _openStateMachine = AnimationGraphNodeView.NoInput;
        private string _stateMachineTitle = string.Empty;
        private int _selectedState = AnimationStateView.AnyState;
        private int _selectedTransition = AnimationGraphNodeView.NoInput;
        private bool _isApplyingSelection;
        private string _stateName = string.Empty;
        private int _stateInputIndex;
        private bool _isSelectedStateEntry;
        private string _transitionTitle = string.Empty;
        private decimal _transitionDuration;
        private int _transitionCurve;
        private decimal _transitionPriority;
        private bool _transitionInterruptible;
        private bool _transitionInertialize;
        private List<(int From, int To, float Duration, int Curve, int Priority, bool Interruptible, bool Inertialize, List<(string Parameter, int Op, float Value)> Conditions)> _transitionDetails = new();

        public AnimationGraphDocumentViewModel()
        {
            AddParameterCommand = new RelayCommand(AddParameter);
            CloseStateMachineCommand = new RelayCommand(CloseStateMachine);
            SetEntryStateCommand = new RelayCommand(() => SetEntryState(_selectedState));
            RemoveStateCommand = new RelayCommand(() => RemoveState(_selectedState));
            RemoveTransitionCommand = new RelayCommand(() => RemoveTransition(_selectedTransition));
            AddConditionCommand = new RelayCommand(AddCondition);

            _pollTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(PollIntervalMs) };
            _pollTimer.Tick += (_, _) => PollPreview();
        }

        public event Action? GraphChanged;
        public event Action? PreviewChanged;
        public event Action<int>? FocusNodeRequested;

        public List<AnimationGraphNodeView> Nodes { get; private set; } = new List<AnimationGraphNodeView>();

        public List<AnimationStateView> States { get; private set; } = new List<AnimationStateView>();

        public List<AnimationTransitionView> Transitions { get; private set; } = new List<AnimationTransitionView>();

        public ObservableCollection<AnimationGraphParameterViewModel> Parameters { get; } = new ObservableCollection<AnimationGraphParameterViewModel>();

        public ObservableCollection<AnimationTransitionConditionViewModel> Conditions { get; } = new ObservableCollection<AnimationTransitionConditionViewModel>();

        public ObservableCollection<string> StateInputChoices { get; } = new ObservableCollection<string>();

        public IReadOnlyList<(string ClassName, string Label)> NodeKinds { get; } = NodeKindLabels.Select(kv => (kv.Key, kv.Value)).ToList();

        public IReadOnlyList<string> ParameterTypes { get; } = new[] { "Float", "Bool", "Trigger", "Vector2" };

        public IReadOnlyList<string> Curves => BlendCurves;

        public List<(Vector Position, string Label)> BlendSamples { get; private set; } = new List<(Vector, string)>();

        public ICommand AddParameterCommand { get; }
        public ICommand CloseStateMachineCommand { get; }
        public ICommand SetEntryStateCommand { get; }
        public ICommand RemoveStateCommand { get; }
        public ICommand RemoveTransitionCommand { get; }
        public ICommand AddConditionCommand { get; }

        public bool IsOpen
        {
            get => _isOpen;
            private set => SetProperty(ref _isOpen, value);
        }

        public bool IsDebugging
        {
            get => _isDebugging;
            private set
            {
                if (SetProperty(ref _isDebugging, value))
                {
                    OnPropertyChanged(nameof(IsReadOnly));
                    OnPropertyChanged(nameof(IsEditable));
                }
            }
        }

        public bool IsReadOnly => _isDebugging;

        public bool IsEditable => !_isDebugging;

        public string Title
        {
            get => _title;
            private set => SetProperty(ref _title, value);
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

        public bool HasSelection => _selectedNode >= 0 && !IsInStateMachine;

        public ComponentSubObjectViewModel? SelectedNodeProperties
        {
            get => _selectedNodeProperties;
            private set => SetProperty(ref _selectedNodeProperties, value);
        }

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
                    string name = value.Trim();

                    PostEdit(state => state.RenameGraphState((uint)_openStateMachine, (uint)stateIndex, new Name(name)));
                }
            }
        }

        public int StateInputIndex
        {
            get => _stateInputIndex;
            set
            {
                if (SetProperty(ref _stateInputIndex, value) && !_isApplyingSelection && value >= 0)
                {
                    int stateIndex = _selectedState;
                    int stateMachine = _openStateMachine;
                    int inputNode = value - 1;

                    if (value == 0)
                    {
                        PostEdit(state => state.DisconnectGraphInput((uint)stateMachine, (uint)stateIndex));
                    }
                    else
                    {
                        PostEdit(state => state.ConnectGraphNodes((uint)inputNode, (uint)stateMachine, (uint)stateIndex));
                    }
                }
            }
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

        private static EditorAnimationEditState? GetState() => EngineManager.EditorGame?.EditorSubsystem?.EditorAnimationEditState;

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
            if (active && _isOpen && !_isDebugging)
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

            if (_isOpen)
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

        // an open debugger moves to a newly selected entity that plays the same graph
        public void FollowSelection(Entity entity)
        {
            if (!_isDebugging || entity == _debugEntity)
            {
                return;
            }

            AnimationGraph? debugGraph = _debugGraph;

            _ = EngineManager.PostToSimThread(() =>
            {
                if (!entity.IsValid || !entity.TryGetComponent(out AnimationComponent animationComponent))
                {
                    return;
                }

                AnimationGraph? graph = animationComponent.Graph;

                if (graph != null && debugGraph != null && graph.Name.ToString() == debugGraph.Name.ToString())
                {
                    Dispatcher.UIThread.Post(() => AttachDebug(entity));
                }
            });
        }

        private void Open()
        {
            IsOpen = true;

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
            SetStateMachine(AnimationGraphNodeView.NoInput, string.Empty);
            IsOpen = false;
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

        public void Refresh()
        {
            if (!_isOpen)
            {
                return;
            }

            int selectedNode = _selectedNode;
            int openStateMachine = _openStateMachine;
            int selectedTransition = _selectedTransition;
            string debugName = _debugEntity?.Name.ToString() ?? string.Empty;

            _ = EngineManager.PostToSimThread(() =>
            {
                (AnimationGraph? graph, AnimationGraphInstance? _) = ResolveSource();

                if (graph == null)
                {
                    return;
                }

                string graphName = graph.Name.ToString();
                uint rootNode = graph.GetRootNode();

                List<AnimationGraphNodeView> nodes = new List<AnimationGraphNodeView>();

                for (uint nodeIndex = 0; nodeIndex < graph.NumNodes(); ++nodeIndex)
                {
                    string className = graph.GetNodeClassName(nodeIndex).ToString();
                    string name = graph.GetNodeName(nodeIndex).ToString();
                    string kind = NodeKindLabels.TryGetValue(className, out string? label) ? label : className;

                    List<(string, int)> inputs = new List<(string, int)>();

                    for (uint slot = 0; slot < graph.GetNodeInputCount(nodeIndex); ++slot)
                    {
                        uint source = graph.GetNodeInput(nodeIndex, slot);
                        inputs.Add((graph.GetNodeInputName(nodeIndex, slot).ToString(), source == uint.MaxValue ? AnimationGraphNodeView.NoInput : (int)source));
                    }

                    Vec2f position = graph.GetNodeEditorPosition(nodeIndex);

                    nodes.Add(new AnimationGraphNodeView
                    {
                        Index = (int)nodeIndex,
                        Title = name.Length > 0 ? name : kind,
                        Kind = kind,
                        Position = new Point(position.x, position.y),
                        Inputs = inputs,
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

                Dispatcher.UIThread.Post(() =>
                {
                    _debugGraph = _isDebugging ? graph : null;

                    Title = _isDebugging ? $"Debug: {graphName} ({debugName})" : $"Graph: {graphName}";
                    Nodes = nodes;

                    if (!Parameters.Select(p => (p.Name, p.Type)).SequenceEqual(parameters))
                    {
                        Parameters.Clear();

                        foreach ((string name, uint type) in parameters)
                        {
                            Parameters.Add(new AnimationGraphParameterViewModel(this, name, type));
                        }
                    }

                    if (stateMachineSnapshot != null)
                    {
                        ApplyStateMachine(stateMachineSnapshot);
                    }
                    else if (_openStateMachine >= 0)
                    {
                        SetStateMachine(AnimationGraphNodeView.NoInput, string.Empty);
                    }

                    SelectNodeObject(selectedNode, selectedObject);

                    GraphChanged?.Invoke();

                    PollPreview();
                });
            });
        }

        private sealed class StateMachineSnapshot
        {
            public string Title = string.Empty;
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
                        InputTitle = input < nodes.Count ? nodes[(int)input].Title : "(no input)",
                        Position = new Point(position.x, position.y),
                        IsEntry = stateIndex == entryState
                    });
                }

                for (uint transitionIndex = 0; transitionIndex < stateMachine.NumTransitions(); ++transitionIndex)
                {
                    bool fromAnyState = stateMachine.IsTransitionFromAnyState(transitionIndex);
                    int from = fromAnyState ? AnimationStateView.AnyState : (int)stateMachine.GetTransitionFrom(transitionIndex);
                    int to = (int)stateMachine.GetTransitionTo(transitionIndex);

                    snapshot.Transitions.Add(new AnimationTransitionView { Index = (int)transitionIndex, From = from, To = to });

                    List<(string, int, float)> conditions = new List<(string, int, float)>();

                    for (uint conditionIndex = 0; conditionIndex < stateMachine.NumConditions(transitionIndex); ++conditionIndex)
                    {
                        conditions.Add((
                            stateMachine.GetConditionParameter(transitionIndex, conditionIndex).ToString(),
                            (int)stateMachine.GetConditionOp(transitionIndex, conditionIndex),
                            stateMachine.GetConditionValue(transitionIndex, conditionIndex)));
                    }

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
                int input = stateView.InputIndex;

                StateInputChoices.Clear();
                StateInputChoices.Add("(none)");

                foreach (AnimationGraphNodeView node in Nodes)
                {
                    StateInputChoices.Add($"{node.Index}: {node.Title}");
                }

                StateName = stateView.Name;
                StateInputIndex = input + 1;
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

            ApplySelectionDetails();
            GraphChanged?.Invoke();
        }

        public void SelectTransition(int transitionIndex)
        {
            _selectedTransition = transitionIndex;
            _selectedState = AnimationStateView.AnyState;

            ApplySelectionDetails();
            GraphChanged?.Invoke();
        }

        public void Select(int nodeIndex)
        {
            if (!_isOpen)
            {
                return;
            }

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

        private void SelectNodeObject(int nodeIndex, AnimationGraphNode? node)
        {
            SelectedNode = nodeIndex;

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
                bool is2D = view?.Kind == NodeKindLabels["AnimationBlend2DNode"];
                bool isBlend = view?.Kind == NodeKindLabels["AnimationBlend1DNode"] || is2D;

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
            if (!_isOpen)
            {
                return;
            }

            List<string> parameterNames = Parameters.Select(p => p.Name).ToList();
            string blendParameter = _blendParameter;
            int openStateMachine = _openStateMachine;
            int numNodes = Nodes.Count;

            _ = EngineManager.PostToSimThread(() =>
            {
                (AnimationGraph? graph, AnimationGraphInstance? instance) = ResolveSource();

                if (graph == null || instance == null || !instance.IsValid)
                {
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

        private void PostEdit(Action<EditorAnimationEditState> action)
        {
            EditorAnimationEditState? state = GetState();

            if (state != null && !_isDebugging)
            {
                _ = EngineManager.PostToSimThread(() => action(state));
            }
        }

        public void AddNode(string className, Point position) => PostEdit(state => state.AddGraphNode(new Name(className), new Vec2f((float)position.X, (float)position.Y)));

        public void AddClipNode(string clipName, Point position)
        {
            PostEdit(state =>
            {
                Animation? clip = AssetManager.Instance.AssetRegistry.GetAsset(AssetBucket.Animations.Value, new Name(clipName)) as Animation;

                if (clip != null)
                {
                    state.AddGraphClipNode(clip, new Vec2f((float)position.X, (float)position.Y));
                }
            });
        }

        public void RemoveNode(int nodeIndex)
        {
            if (_isDebugging)
            {
                return;
            }

            if (nodeIndex == _selectedNode)
            {
                SelectNodeObject(AnimationGraphNodeView.NoInput, null);
            }

            PostEdit(state => state.RemoveGraphNode((uint)nodeIndex));
        }

        public void Connect(int fromNode, int toNode, int slot) => PostEdit(state => state.ConnectGraphNodes((uint)fromNode, (uint)toNode, (uint)slot));

        public void Disconnect(int nodeIndex, int slot) => PostEdit(state => state.DisconnectGraphInput((uint)nodeIndex, (uint)slot));

        public void Rewire(int nodeIndex, int slot, int toNode, int toSlot) => PostEdit(state => state.RewireGraphInput((uint)nodeIndex, (uint)slot, (uint)toNode, (uint)toSlot));

        public void SetOutput(int nodeIndex) => PostEdit(state => state.SetGraphRootNode((uint)nodeIndex));

        public void MoveNode(int nodeIndex, Point position) => PostEdit(state => state.MoveGraphNode((uint)nodeIndex, new Vec2f((float)position.X, (float)position.Y)));

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

        public void SetParameterValue(string name, Vec2f value) => PostEdit(state => state.SetPreviewParameter(new Name(name), value));

        public void TriggerParameter(string name) => PostEdit(state => state.TriggerPreviewParameter(new Name(name)));

        public void RemoveParameter(string name) => PostEdit(state => state.RemoveGraphParameter(new Name(name)));

        private void AddParameter()
        {
            string name = _newParameterName.Trim();
            uint type = (uint)Math.Clamp(_newParameterType, 0, ParameterTypes.Count - 1);

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
