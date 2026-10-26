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
        public float Weight { get; set; }
        public string Status { get; set; } = string.Empty;
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
            ["AnimationStateMachineNode"] = "State Machine"
        };

        private readonly DispatcherTimer _pollTimer;

        private DelegateHandler? _graphChangedHandler;

        private bool _isOpen;
        private string _title = string.Empty;
        private int _selectedNode = AnimationGraphNodeView.NoInput;
        private ComponentSubObjectViewModel? _selectedNodeProperties;
        private string _blendParameter = string.Empty;
        private bool _isBlend2D;
        private Vector _blendCursor;
        private string _newParameterName = string.Empty;
        private int _newParameterType;

        public AnimationGraphDocumentViewModel()
        {
            AddParameterCommand = new RelayCommand(AddParameter);

            _pollTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(PollIntervalMs) };
            _pollTimer.Tick += (_, _) => PollPreview();
        }

        public event Action? GraphChanged;
        public event Action? PreviewChanged;

        public List<AnimationGraphNodeView> Nodes { get; private set; } = new List<AnimationGraphNodeView>();

        public ObservableCollection<AnimationGraphParameterViewModel> Parameters { get; } = new ObservableCollection<AnimationGraphParameterViewModel>();

        public IReadOnlyList<(string ClassName, string Label)> NodeKinds { get; } = NodeKindLabels.Select(kv => (kv.Key, kv.Value)).ToList();

        public IReadOnlyList<string> ParameterTypes { get; } = new[] { "Float", "Bool", "Trigger", "Vector2" };

        public List<(Vector Position, string Label)> BlendSamples { get; private set; } = new List<(Vector, string)>();

        public ICommand AddParameterCommand { get; }

        public bool IsOpen
        {
            get => _isOpen;
            private set => SetProperty(ref _isOpen, value);
        }

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

        public bool HasSelection => _selectedNode >= 0;

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

        private static EditorAnimationEditState? GetState() => EngineManager.EditorGame?.EditorSubsystem?.EditorAnimationEditState;

        public void SetActive(bool active)
        {
            if (active && _isOpen)
            {
                Refresh();
                return;
            }

            _graphChangedHandler?.Remove();
            _graphChangedHandler = null;

            if (!active)
            {
                _pollTimer.Stop();
                SelectedNodeProperties?.Dispose();
                SelectedNodeProperties = null;
                SelectedNode = AnimationGraphNodeView.NoInput;
                IsOpen = false;

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

            IsOpen = true;

            _pollTimer.Start();

            Refresh();
        }

        public void Refresh()
        {
            EditorAnimationEditState? state = GetState();

            if (state == null)
            {
                return;
            }

            int selectedNode = _selectedNode;

            _ = EngineManager.PostToSimThread(() =>
            {
                if (!state.IsEditingGraph())
                {
                    return;
                }

                string title = state.GetGraph()?.Name.ToString() ?? string.Empty;
                uint rootNode = state.GetGraphRootNode();

                List<AnimationGraphNodeView> nodes = new List<AnimationGraphNodeView>();

                for (uint nodeIndex = 0; nodeIndex < state.NumGraphNodes(); ++nodeIndex)
                {
                    string className = state.GetGraphNodeClass(nodeIndex).ToString();
                    string name = state.GetGraphNodeName(nodeIndex).ToString();
                    string kind = NodeKindLabels.TryGetValue(className, out string? label) ? label : className;

                    List<(string, int)> inputs = new List<(string, int)>();

                    for (uint slot = 0; slot < state.GetGraphNodeInputCount(nodeIndex); ++slot)
                    {
                        uint source = state.GetGraphNodeInput(nodeIndex, slot);
                        inputs.Add((state.GetGraphNodeInputName(nodeIndex, slot).ToString(), source == uint.MaxValue ? AnimationGraphNodeView.NoInput : (int)source));
                    }

                    Vec2f position = state.GetGraphNodePosition(nodeIndex);

                    nodes.Add(new AnimationGraphNodeView
                    {
                        Index = (int)nodeIndex,
                        Title = name.Length > 0 ? name : kind,
                        Kind = kind,
                        Position = new Point(position.x, position.y),
                        Inputs = inputs,
                        CanAddInputs = state.CanAddGraphNodeInputs(nodeIndex),
                        IsOutput = nodeIndex == rootNode,
                        Weight = state.GetPreviewNodeWeight(nodeIndex)
                    });
                }

                List<(string Name, uint Type)> parameters = new List<(string, uint)>();

                foreach (Name parameterName in state.GetGraphParameterNames())
                {
                    parameters.Add((parameterName.ToString(), state.GetGraphParameterType(parameterName)));
                }

                if (selectedNode >= nodes.Count)
                {
                    selectedNode = AnimationGraphNodeView.NoInput;
                }

                AnimationGraphNode? selectedObject = selectedNode >= 0 ? state.GetGraphNode((uint)selectedNode) : null;

                Dispatcher.UIThread.Post(() =>
                {
                    Title = $"Graph: {title}";
                    Nodes = nodes;

                    if (!Parameters.Select(p => (p.Name, p.Type)).SequenceEqual(parameters))
                    {
                        Parameters.Clear();

                        foreach ((string name, uint type) in parameters)
                        {
                            Parameters.Add(new AnimationGraphParameterViewModel(this, name, type));
                        }
                    }

                    SelectNodeObject(selectedNode, selectedObject);

                    GraphChanged?.Invoke();

                    PollPreview();
                });
            });
        }

        public void Select(int nodeIndex)
        {
            EditorAnimationEditState? state = GetState();

            if (state == null)
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                AnimationGraphNode? node = nodeIndex >= 0 ? state.GetGraphNode((uint)nodeIndex) : null;

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

            if (node != null && node.IsValid)
            {
                EditorAnimationEditState? state = GetState();

                SelectedNodeProperties = new ComponentSubObjectViewModel(
                    Nodes.FirstOrDefault(n => n.Index == nodeIndex)?.Kind ?? string.Empty,
                    node,
                    postWriteCallback: () => state?.CommitGraphEdit("Edit Graph Node"));
            }

            RefreshBlendSpace();
        }

        private void RefreshBlendSpace()
        {
            EditorAnimationEditState? state = GetState();
            int nodeIndex = _selectedNode;

            if (state == null || nodeIndex < 0)
            {
                ApplyBlendSpace(false, string.Empty, false, new List<(Vector, string)>());

                return;
            }

            AnimationGraphNodeView? view = Nodes.FirstOrDefault(n => n.Index == nodeIndex);

            _ = EngineManager.PostToSimThread(() =>
            {
                string parameter = state.GetBlendParameter((uint)nodeIndex).ToString();
                bool is2D = view?.Kind == NodeKindLabels["AnimationBlend2DNode"];

                List<(Vector, string)> samples = new List<(Vector, string)>();
                bool isBlend = view?.Kind == NodeKindLabels["AnimationBlend1DNode"] || is2D;

                if (isBlend)
                {
                    for (uint sample = 0; sample < state.GetGraphNodeInputCount((uint)nodeIndex); ++sample)
                    {
                        Vec2f position = state.GetBlendSamplePosition((uint)nodeIndex, sample);
                        int source = view != null && sample < view.Inputs.Count ? view.Inputs[(int)sample].Source : AnimationGraphNodeView.NoInput;
                        string label = Nodes.FirstOrDefault(n => n.Index == source)?.Title ?? "(none)";

                        samples.Add((new Vector(position.x, position.y), label));
                    }
                }

                Dispatcher.UIThread.Post(() => ApplyBlendSpace(isBlend, parameter, is2D, samples));
            });
        }

        private void ApplyBlendSpace(bool isBlend, string parameter, bool is2D, List<(Vector, string)> samples)
        {
            IsBlendSelected = isBlend;
            BlendParameter = parameter;
            IsBlend2D = is2D;
            BlendSamples = samples;

            OnPropertyChanged(nameof(BlendSamples));
            OnPropertyChanged(nameof(IsBlendSelected));
        }

        private void PollPreview()
        {
            EditorAnimationEditState? state = GetState();

            if (!_isOpen || state == null)
            {
                return;
            }

            List<string> parameterNames = Parameters.Select(p => p.Name).ToList();
            string blendParameter = _blendParameter;

            _ = EngineManager.PostToSimThread(() =>
            {
                if (!state.IsEditingGraph())
                {
                    return;
                }

                List<float> weights = new List<float>();
                List<string> statuses = new List<string>();

                for (uint nodeIndex = 0; nodeIndex < state.NumGraphNodes(); ++nodeIndex)
                {
                    weights.Add(state.GetPreviewNodeWeight(nodeIndex));
                    statuses.Add(state.GetPreviewNodeStatus(nodeIndex) ?? string.Empty);
                }

                List<Vec2f> values = parameterNames.Select(name => state.GetPreviewParameter(new Name(name))).ToList();
                Vec2f cursor = blendParameter.Length > 0 ? state.GetPreviewParameter(new Name(blendParameter)) : new Vec2f(0.0f, 0.0f);

                Dispatcher.UIThread.Post(() =>
                {
                    for (int nodeIndex = 0; nodeIndex < Nodes.Count && nodeIndex < weights.Count; ++nodeIndex)
                    {
                        Nodes[nodeIndex].Weight = weights[nodeIndex];
                        Nodes[nodeIndex].Status = statuses[nodeIndex];
                    }

                    for (int parameterIndex = 0; parameterIndex < Parameters.Count && parameterIndex < values.Count; ++parameterIndex)
                    {
                        Parameters[parameterIndex].ApplyValue(values[parameterIndex]);
                    }

                    BlendCursor = new Vector(cursor.x, cursor.y);

                    PreviewChanged?.Invoke();
                });
            });
        }

        private void PostToState(Action<EditorAnimationEditState> action)
        {
            EditorAnimationEditState? state = GetState();

            if (state != null)
            {
                _ = EngineManager.PostToSimThread(() => action(state));
            }
        }

        public void AddNode(string className, Point position) => PostToState(state => state.AddGraphNode(new Name(className), new Vec2f((float)position.X, (float)position.Y)));

        public void AddClipNode(string clipName, Point position)
        {
            PostToState(state =>
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
            if (nodeIndex == _selectedNode)
            {
                SelectNodeObject(AnimationGraphNodeView.NoInput, null);
            }

            PostToState(state => state.RemoveGraphNode((uint)nodeIndex));
        }

        public void Connect(int fromNode, int toNode, int slot) => PostToState(state => state.ConnectGraphNodes((uint)fromNode, (uint)toNode, (uint)slot));

        public void Disconnect(int nodeIndex, int slot) => PostToState(state => state.DisconnectGraphInput((uint)nodeIndex, (uint)slot));

        public void SetOutput(int nodeIndex) => PostToState(state => state.SetGraphRootNode((uint)nodeIndex));

        public void MoveNode(int nodeIndex, Point position) => PostToState(state => state.MoveGraphNode((uint)nodeIndex, new Vec2f((float)position.X, (float)position.Y)));

        public void MoveBlendSample(int sample, Vector position)
        {
            int nodeIndex = _selectedNode;

            PostToState(state => state.SetBlendSamplePosition((uint)nodeIndex, (uint)sample, new Vec2f((float)position.X, (float)position.Y)));
        }

        public void SetBlendCursor(Vector position)
        {
            BlendCursor = position;

            if (_blendParameter.Length > 0)
            {
                SetParameterValue(_blendParameter, new Vec2f((float)position.X, (float)position.Y));
            }
        }

        public void SetParameterValue(string name, Vec2f value) => PostToState(state => state.SetPreviewParameter(new Name(name), value));

        public void TriggerParameter(string name) => PostToState(state => state.TriggerPreviewParameter(new Name(name)));

        public void RemoveParameter(string name) => PostToState(state => state.RemoveGraphParameter(new Name(name)));

        private void AddParameter()
        {
            string name = _newParameterName.Trim();
            uint type = (uint)Math.Clamp(_newParameterType, 0, ParameterTypes.Count - 1);

            if (name.Length == 0)
            {
                return;
            }

            NewParameterName = string.Empty;

            PostToState(state => state.AddGraphParameter(new Name(name), type));
        }
    }
}
