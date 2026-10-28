using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Media;
using Hyperion.Editor.ViewModels;

namespace Hyperion.Editor.Controls
{
    using Color = Avalonia.Media.Color;

    public sealed class AnimationGraphCanvas : Control
    {
        private const double NodeWidth = 170.0;
        private const double HeaderHeight = 24.0;
        private const double DetailHeight = 16.0;
        private const double RowHeight = 18.0;
        private const double PinRadius = 5.0;
        private const double OutputWidth = 90.0;
        private const double OutputGap = 70.0;
        private const double StateWidth = 150.0;
        private const double StateHeight = 46.0;
        private const double StateEdge = 8.0;
        private const double TransitionOffset = 5.0;
        private const double GridSpacing = 32.0;
        private const double MinZoom = 0.25;
        private const double MaxZoom = 2.0;
        private const double FrameMargin = 40.0;
        private const double PasteOffset = 30.0;

        private static readonly DataFormat<string> AssetDragFormat = DataFormat.CreateStringApplicationFormat("hyperion-asset");
        private static readonly Typeface LabelTypeface = new Typeface(FontFamily.Default);
        private static readonly Color OutputColor = Color.Parse("#FFFFB347");
        private static readonly Color InvalidColor = Color.Parse("#FFE5534B");

        private static readonly Dictionary<AnimationGraphNodeCategory, Color> CategoryColors = new()
        {
            [AnimationGraphNodeCategory.Source] = Color.Parse("#FF2E7D5B"),
            [AnimationGraphNodeCategory.Blend] = Color.Parse("#FF2F5E9E"),
            [AnimationGraphNodeCategory.Modify] = Color.Parse("#FF9E6A2F"),
            [AnimationGraphNodeCategory.State] = Color.Parse("#FF6E4A9E")
        };

        private AnimationGraphDocumentViewModel? _viewModel;

        private Vector _graphPan = new Vector(40.0, 40.0);
        private double _graphZoom = 1.0;
        private Vector _statePan = new Vector(260.0, 60.0);
        private double _stateZoom = 1.0;

        private enum DragMode
        {
            None,
            Pan,
            MoveNodes,
            Wire,
            MoveState,
            BoxSelect,
            Transition
        }

        private DragMode _dragMode;
        private Point _dragStart;
        private Point _pointer;
        private int _dragNode = AnimationGraphNodeView.NoInput;
        private Point _dragNodeStart;
        private Dictionary<int, Point> _dragStarts = new Dictionary<int, Point>();
        private bool _dragMoved;
        private bool _isRightPan;
        private bool _isFramePending;
        private (int Node, int Slot)? _rewiredInput;

        // set while a transition is being drawn from this state
        private int? _transitionSource;

        public AnimationGraphCanvas()
        {
            ClipToBounds = true;
            Focusable = true;

            DragDrop.SetAllowDrop(this, true);
            AddHandler(DragDrop.DragOverEvent, OnDragOver);
            AddHandler(DragDrop.DropEvent, OnDrop);
        }

        protected override void OnDataContextChanged(EventArgs e)
        {
            base.OnDataContextChanged(e);

            if (_viewModel != null)
            {
                _viewModel.GraphChanged -= InvalidateVisual;
                _viewModel.PreviewChanged -= InvalidateVisual;
                _viewModel.FocusNodeRequested -= FocusNode;
                _viewModel.FrameRequested -= Frame;
                _viewModel.AddNodeRequested -= OnAddNodeRequested;
            }

            _viewModel = DataContext as AnimationGraphDocumentViewModel;

            if (_viewModel != null)
            {
                _viewModel.GraphChanged += InvalidateVisual;
                _viewModel.PreviewChanged += InvalidateVisual;
                _viewModel.FocusNodeRequested += FocusNode;
                _viewModel.FrameRequested += Frame;
                _viewModel.AddNodeRequested += OnAddNodeRequested;
            }

            InvalidateVisual();
        }

        private bool InStateMachine => _viewModel?.IsInStateMachine ?? false;

        private bool IsEditable => _viewModel?.IsEditable ?? false;

        private Vector Pan
        {
            get => InStateMachine ? _statePan : _graphPan;
            set
            {
                if (InStateMachine)
                {
                    _statePan = value;
                }
                else
                {
                    _graphPan = value;
                }
            }
        }

        private double Zoom
        {
            get => InStateMachine ? _stateZoom : _graphZoom;
            set
            {
                if (InStateMachine)
                {
                    _stateZoom = value;
                }
                else
                {
                    _graphZoom = value;
                }
            }
        }

        private IBrush Resource(string key, Color fallback)
        {
            return this.TryFindResource(key, ActualThemeVariant, out object? resource) && resource is IBrush brush
                ? brush
                : new SolidColorBrush(fallback);
        }

        private Point ToWorld(Point screen) => new Point((screen.X - Pan.X) / Zoom, (screen.Y - Pan.Y) / Zoom);

        private void FocusNode(int nodeIndex)
        {
            if (FindNode(nodeIndex) is { } node)
            {
                Rect rect = NodeRect(node);

                _graphPan = new Vector(Bounds.Width * 0.5 - rect.Center.X * _graphZoom, Bounds.Height * 0.5 - rect.Center.Y * _graphZoom);
            }

            InvalidateVisual();
        }

        protected override void OnSizeChanged(SizeChangedEventArgs e)
        {
            base.OnSizeChanged(e);

            if (_isFramePending)
            {
                Frame();
            }
        }

        private void Frame()
        {
            if (_viewModel == null)
            {
                return;
            }

            if (Bounds.Width < 1.0 || Bounds.Height < 1.0)
            {
                _isFramePending = true;

                return;
            }

            _isFramePending = false;

            List<Rect> rects = new List<Rect>();

            if (InStateMachine)
            {
                rects.AddRange(_viewModel.States.Select(s => StateRect(s.Index)));
                rects.Add(StateRect(AnimationStateView.AnyState));
            }
            else
            {
                List<AnimationGraphNodeView> nodes = _viewModel.Nodes.Where(n => _viewModel.SelectedNodes.Contains(n.Index)).ToList();

                if (nodes.Count == 0)
                {
                    nodes = _viewModel.Nodes;

                    if (OutputRect() is { } outputRect)
                    {
                        rects.Add(outputRect);
                    }
                }

                rects.AddRange(nodes.Select(NodeRect));
            }

            if (rects.Count == 0)
            {
                return;
            }

            Rect bounds = rects.Aggregate((a, b) => a.Union(b)).Inflate(FrameMargin);

            double zoom = Math.Clamp(Math.Min(Bounds.Width / bounds.Width, Bounds.Height / bounds.Height), MinZoom, 1.0);

            Zoom = zoom;
            Pan = new Vector(Bounds.Width * 0.5 - bounds.Center.X * zoom, Bounds.Height * 0.5 - bounds.Center.Y * zoom);

            InvalidateVisual();
        }

        #region Graph

        private static double DetailRows(AnimationGraphNodeView node) => node.Detail.Length > 0 ? DetailHeight : 0.0;

        private static int NumRows(AnimationGraphNodeView node) => Math.Max(1, node.Inputs.Count + (node.CanAddInputs ? 1 : 0));

        private static double StatusHeight(AnimationGraphNodeView node) => node.IsStateMachine ? RowHeight : 0.0;

        private static Rect NodeRect(AnimationGraphNodeView node) => new Rect(node.Position, new Size(NodeWidth, HeaderHeight + DetailRows(node) + NumRows(node) * RowHeight + StatusHeight(node) + 6.0));

        private static Point OutputPin(AnimationGraphNodeView node) => new Point(node.Position.X + NodeWidth, node.Position.Y + HeaderHeight * 0.5);

        private static Point InputPin(AnimationGraphNodeView node, int slot) => new Point(node.Position.X, node.Position.Y + HeaderHeight + DetailRows(node) + (slot + 0.5) * RowHeight);

        private AnimationGraphNodeView? FindNode(int index) => _viewModel?.Nodes.FirstOrDefault(node => node.Index == index);

        private AnimationGraphNodeView? RootNode() => _viewModel?.Nodes.FirstOrDefault(node => node.IsOutput);

        private Rect? OutputRect()
        {
            if (_viewModel == null || _viewModel.Nodes.Count == 0)
            {
                return null;
            }

            if (RootNode() is { } root)
            {
                Rect rootRect = NodeRect(root);

                return new Rect(rootRect.Right + OutputGap, root.Position.Y, OutputWidth, HeaderHeight + RowHeight + 6.0);
            }

            double right = _viewModel.Nodes.Max(n => NodeRect(n).Right);
            double top = _viewModel.Nodes.Min(n => n.Position.Y);

            return new Rect(right + OutputGap, top, OutputWidth, HeaderHeight + RowHeight + 6.0);
        }

        private Point? OutputTerminalPin()
        {
            if (OutputRect() is not { } rect)
            {
                return null;
            }

            return new Point(rect.X, rect.Y + HeaderHeight + RowHeight * 0.5);
        }

        private bool HitOutputTerminal(Point world)
        {
            if (OutputRect() is not { } rect)
            {
                return false;
            }

            return rect.Inflate(PinRadius + 3.0).Contains(world);
        }

        private AnimationGraphNodeView? HitNode(Point world)
        {
            return _viewModel?.Nodes.LastOrDefault(node => NodeRect(node).Contains(world));
        }

        private AnimationGraphNodeView? HitNodeHeader(Point world)
        {
            return _viewModel?.Nodes.LastOrDefault(node => new Rect(node.Position, new Size(NodeWidth, HeaderHeight)).Contains(world));
        }

        private AnimationGraphNodeView? HitOutputPin(Point world)
        {
            double radius = (PinRadius + 3.0) / Math.Min(Zoom, 1.0);

            return _viewModel?.Nodes.LastOrDefault(node => Distance(OutputPin(node), world) <= radius);
        }

        private (AnimationGraphNodeView Node, int Slot)? HitInputPin(Point world)
        {
            if (_viewModel == null)
            {
                return null;
            }

            double radius = (PinRadius + 3.0) / Math.Min(Zoom, 1.0);

            foreach (AnimationGraphNodeView node in Enumerable.Reverse(_viewModel.Nodes))
            {
                for (int slot = 0; slot < NumRows(node); ++slot)
                {
                    if (slot >= node.Inputs.Count && !node.CanAddInputs)
                    {
                        continue;
                    }

                    if (Distance(InputPin(node, slot), world) <= radius)
                    {
                        return (node, slot);
                    }
                }
            }

            return null;
        }

        private bool IsInvalidWireTarget(int sourceNode, Point world)
        {
            if (_viewModel == null || HitInputPin(world) is not { } input)
            {
                return false;
            }

            return input.Node.Index == sourceNode || _viewModel.WouldCycle(sourceNode, input.Node.Index);
        }

        private void RenderGraph(DrawingContext context, Palette palette)
        {
            AnimationGraphDocumentViewModel viewModel = _viewModel!;

            foreach (AnimationGraphNodeView node in viewModel.Nodes)
            {
                for (int slot = 0; slot < node.Inputs.Count; ++slot)
                {
                    if (_rewiredInput == (node.Index, slot))
                    {
                        continue;
                    }

                    if (FindNode(node.Inputs[slot].Source) is { } source)
                    {
                        DrawWire(context, source.Weight > 0.0f && node.Weight > 0.0f ? palette.ActiveWire : palette.Wire, OutputPin(source), InputPin(node, slot));
                    }
                }
            }

            if (RootNode() is { } root && OutputTerminalPin() is { } terminalPin)
            {
                DrawWire(context, root.Weight > 0.0f ? palette.ActiveWire : palette.Wire, OutputPin(root), terminalPin);
            }

            foreach (AnimationGraphNodeView node in viewModel.Nodes)
            {
                Rect rect = NodeRect(node);
                Pen nodePen = palette.Border;
                bool isSelected = viewModel.SelectedNodes.Contains(node.Index) || node.Index == viewModel.SelectedNode;

                if (isSelected)
                {
                    nodePen = palette.Selected;
                }
                else if (viewModel.UsesSelectedParameter(node.Parameters))
                {
                    nodePen = palette.Highlight;
                }

                context.DrawRectangle(palette.Node, nodePen, rect, 4.0, 4.0);
                context.DrawRectangle(palette.CategoryHeaders[node.Category], null, new Rect(rect.X, rect.Y, rect.Width, HeaderHeight), 4.0, 4.0);

                if (node.Weight > 0.0f)
                {
                    context.FillRectangle(palette.Accent, new Rect(rect.X, rect.Y + HeaderHeight - 2.0, rect.Width * Math.Min(node.Weight, 1.0f), 2.0));
                }

                DrawText(context, node.Title, 11.0, palette.Text, new Point(rect.X + 8.0, rect.Y + 4.0), NodeWidth - 30.0);

                if (node.Title != node.Kind)
                {
                    DrawText(context, node.Kind, 9.0, palette.Muted, new Point(rect.Right - 8.0 - MeasureText(node.Kind, 9.0), rect.Y + 6.0), 60.0);
                }

                if (node.Detail.Length > 0)
                {
                    DrawText(context, node.Detail, 10.0, palette.Muted, new Point(rect.X + 8.0, rect.Y + HeaderHeight + 1.0), NodeWidth - 16.0);
                }

                if (node.Status.Length > 0)
                {
                    DrawText(context, node.Status, 10.0, palette.Accent, new Point(rect.X + 8.0, rect.Bottom - RowHeight - 3.0), NodeWidth - 16.0);
                }

                context.DrawEllipse(node.IsOutput ? palette.Output.Brush : palette.Muted, null, OutputPin(node), PinRadius, PinRadius);

                for (int slot = 0; slot < NumRows(node); ++slot)
                {
                    Point pin = InputPin(node, slot);

                    if (slot < node.Inputs.Count)
                    {
                        bool isConnected = node.Inputs[slot].Source != AnimationGraphNodeView.NoInput;

                        context.DrawEllipse(isConnected ? palette.Muted : null, new Pen(palette.Muted, 1.0), pin, PinRadius, PinRadius);
                        DrawText(context, node.Inputs[slot].Name, 10.0, palette.Muted, new Point(pin.X + 10.0, pin.Y - 7.0), NodeWidth - 20.0);
                    }
                    else if (node.CanAddInputs && IsEditable)
                    {
                        context.DrawEllipse(null, new Pen(palette.Muted, 1.0) { DashStyle = DashStyle.Dash }, pin, PinRadius, PinRadius);
                        DrawText(context, "+", 10.0, palette.Muted, new Point(pin.X + 10.0, pin.Y - 7.0), 20.0);
                    }
                }
            }

            if (OutputRect() is { } outputRect && OutputTerminalPin() is { } outputPin)
            {
                context.DrawRectangle(palette.Node, palette.Output, outputRect, 4.0, 4.0);
                DrawText(context, "Output", 11.0, palette.Text, new Point(outputRect.X + 8.0, outputRect.Y + 4.0), OutputWidth - 16.0);
                context.DrawEllipse(palette.Output.Brush, null, outputPin, PinRadius, PinRadius);
                DrawText(context, "Pose", 10.0, palette.Muted, new Point(outputPin.X + 10.0, outputPin.Y - 7.0), OutputWidth - 20.0);
            }

            if (_dragMode == DragMode.Wire && FindNode(_dragNode) is { } wireSource)
            {
                Point end = ToWorld(_pointer);
                Pen pen = palette.ActiveWire;

                if (IsInvalidWireTarget(wireSource.Index, end))
                {
                    pen = palette.Invalid;
                }

                DrawWire(context, pen, OutputPin(wireSource), end);
            }

            if (_dragMode == DragMode.BoxSelect)
            {
                Rect box = new Rect(ToWorld(_dragStart), ToWorld(_pointer)).Normalize();

                context.DrawRectangle(palette.BoxFill, new Pen(palette.Accent, 1.0 / Zoom), box);
            }
        }

        private void PressGraph(Point world, int clickCount, KeyModifiers modifiers)
        {
            AnimationGraphDocumentViewModel viewModel = _viewModel!;
            bool isAdditive = (modifiers & (KeyModifiers.Shift | KeyModifiers.Control)) != 0;

            if (clickCount == 2 && HitNode(world) is { } doubleClicked)
            {
                if (doubleClicked.IsStateMachine)
                {
                    viewModel.OpenStateMachineNode(doubleClicked.Index);
                }
                else if (IsEditable && HitNodeHeader(world) == doubleClicked)
                {
                    ShowRename(doubleClicked);
                }

                _dragMode = DragMode.None;

                return;
            }

            if (IsEditable && HitOutputPin(world) is { } outputNode)
            {
                _dragMode = DragMode.Wire;
                _dragNode = outputNode.Index;
            }
            else if (IsEditable && HitInputPin(world) is { } input && input.Slot < input.Node.Inputs.Count && input.Node.Inputs[input.Slot].Source != AnimationGraphNodeView.NoInput)
            {
                _dragMode = DragMode.Wire;
                _dragNode = input.Node.Inputs[input.Slot].Source;
                _rewiredInput = (input.Node.Index, input.Slot);
            }
            else if (HitNode(world) is { } node)
            {
                HashSet<int> selection = new HashSet<int>(viewModel.SelectedNodes);

                if (isAdditive)
                {
                    if (!selection.Remove(node.Index))
                    {
                        selection.Add(node.Index);
                    }

                    viewModel.SetNodeSelection(selection, selection.Contains(node.Index) ? node.Index : AnimationGraphNodeView.NoInput);

                    return;
                }

                if (!selection.Contains(node.Index))
                {
                    selection = new HashSet<int> { node.Index };
                    viewModel.Select(node.Index);
                }

                _dragMode = DragMode.MoveNodes;
                _dragNode = node.Index;
                _dragStarts = viewModel.Nodes.Where(n => selection.Contains(n.Index)).ToDictionary(n => n.Index, n => n.Position);
            }
            else
            {
                _dragMode = DragMode.BoxSelect;

                if (!isAdditive)
                {
                    viewModel.Select(AnimationGraphNodeView.NoInput);
                }
            }
        }

        private void FinishBoxSelect(KeyModifiers modifiers)
        {
            AnimationGraphDocumentViewModel viewModel = _viewModel!;

            if (!_dragMoved)
            {
                return;
            }

            Rect box = new Rect(ToWorld(_dragStart), ToWorld(_pointer)).Normalize();
            HashSet<int> selection = (modifiers & (KeyModifiers.Shift | KeyModifiers.Control)) != 0 ? new HashSet<int>(viewModel.SelectedNodes) : new HashSet<int>();

            foreach (AnimationGraphNodeView node in viewModel.Nodes)
            {
                if (box.Intersects(NodeRect(node)))
                {
                    selection.Add(node.Index);
                }
            }

            viewModel.SetNodeSelection(selection, AnimationGraphNodeView.NoInput);
        }

        private void ShowGraphContextMenu(Point world)
        {
            AnimationGraphDocumentViewModel viewModel = _viewModel!;

            if (HitNode(world) is not { } node)
            {
                if (IsEditable)
                {
                    ShowAddNodePopup(world, AnimationGraphNodeView.NoInput);
                }

                return;
            }

            ContextMenu menu = new ContextMenu();

            if (node.IsStateMachine)
            {
                MenuItem open = new MenuItem { Header = "Open" };
                open.Click += (_, _) => viewModel.OpenStateMachineNode(node.Index);

                menu.Items.Add(open);
            }

            if (IsEditable)
            {
                List<int> targets = viewModel.SelectedNodes.Contains(node.Index) ? viewModel.SelectedNodes.ToList() : new List<int> { node.Index };

                MenuItem rename = new MenuItem { Header = "Rename", InputGesture = new KeyGesture(Key.F2) };
                rename.Click += (_, _) => ShowRename(node);

                MenuItem output = new MenuItem { Header = "Use as Output", IsEnabled = !node.IsOutput };
                output.Click += (_, _) => viewModel.SetOutput(node.Index);

                MenuItem duplicate = new MenuItem { Header = "Duplicate", InputGesture = new KeyGesture(Key.D, KeyModifiers.Control) };
                duplicate.Click += (_, _) => viewModel.DuplicateNodes(targets, new Vector(PasteOffset, PasteOffset));

                MenuItem delete = new MenuItem { Header = "Delete", InputGesture = new KeyGesture(Key.Delete) };
                delete.Click += (_, _) => viewModel.RemoveNodes(targets);

                menu.Items.Add(rename);
                menu.Items.Add(output);
                menu.Items.Add(duplicate);
                menu.Items.Add(delete);
            }

            if (menu.Items.Count > 0)
            {
                menu.Open(this);
            }
        }

        private void OnAddNodeRequested()
        {
            if (!IsEditable)
            {
                return;
            }

            Point at = Bounds.Width > 0.0 ? ToWorld(new Point(Bounds.Width * 0.5, Bounds.Height * 0.5)) : new Point(0.0, 0.0);

            if (InStateMachine)
            {
                _viewModel!.AddState(at);

                return;
            }

            ShowAddNodePopup(at, AnimationGraphNodeView.NoInput);
        }

        private void ShowAddNodePopup(Point world, int connectFrom)
        {
            AnimationGraphDocumentViewModel viewModel = _viewModel!;

            List<(string Label, Action Add)> entries = new List<(string, Action)>();

            foreach ((string className, string label) in viewModel.NodeKindChoices)
            {
                entries.Add((label, () => viewModel.AddNode(className, world, connectFrom)));
            }

            foreach (string clip in viewModel.ClipChoices)
            {
                entries.Add(($"Clip: {clip}", () => viewModel.AddClipNode(clip, world, connectFrom)));
            }

            TextBox search = new TextBox { Watermark = "Search", MinWidth = 220 };
            ListBox results = new ListBox { MaxHeight = 260, ItemsSource = entries.Select(e => e.Label).ToList(), SelectedIndex = 0 };
            Flyout flyout = new Flyout { Content = new StackPanel { Spacing = 4, Children = { search, results } }, Placement = PlacementMode.Pointer };

            void Pick(string? label)
            {
                (string Label, Action Add) entry = entries.FirstOrDefault(e => e.Label == label);

                flyout.Hide();
                entry.Add?.Invoke();
            }

            search.TextChanged += (_, _) =>
            {
                string filter = search.Text ?? string.Empty;
                List<string> matches = entries.Select(e => e.Label).Where(l => l.Contains(filter, StringComparison.OrdinalIgnoreCase)).ToList();

                results.ItemsSource = matches;
                results.SelectedIndex = matches.Count > 0 ? 0 : -1;
            };

            search.KeyDown += (_, e) =>
            {
                if (e.Key == Key.Enter)
                {
                    Pick(results.SelectedItem as string);
                    e.Handled = true;
                }
                else if (e.Key == Key.Down && results.ItemCount > 0)
                {
                    results.SelectedIndex = Math.Min(results.SelectedIndex + 1, results.ItemCount - 1);
                    e.Handled = true;
                }
                else if (e.Key == Key.Up && results.ItemCount > 0)
                {
                    results.SelectedIndex = Math.Max(results.SelectedIndex - 1, 0);
                    e.Handled = true;
                }
            };

            results.Tapped += (_, _) => Pick(results.SelectedItem as string);

            flyout.Opened += (_, _) => search.Focus();
            flyout.ShowAt(this, true);
        }

        private void ShowRename(AnimationGraphNodeView node)
        {
            AnimationGraphDocumentViewModel viewModel = _viewModel!;

            TextBox name = new TextBox { Text = node.Title == node.Kind ? string.Empty : node.Title, Watermark = node.Kind, MinWidth = 160 };
            Flyout flyout = new Flyout { Content = name, Placement = PlacementMode.Pointer };

            name.KeyDown += (_, e) =>
            {
                if (e.Key == Key.Enter)
                {
                    viewModel.RenameNode(node.Index, name.Text ?? string.Empty);
                    flyout.Hide();
                    e.Handled = true;
                }
            };

            flyout.Opened += (_, _) =>
            {
                name.Focus();
                name.SelectAll();
            };

            flyout.ShowAt(this, true);
        }

        #endregion Graph

        #region State machine

        private Point AnyStatePosition()
        {
            List<AnimationStateView> states = _viewModel!.States;

            if (states.Count == 0)
            {
                return new Point(-220.0, 0.0);
            }

            return new Point(states.Min(s => s.Position.X) - 220.0, states.Min(s => s.Position.Y));
        }

        private Rect StateRect(int stateIndex)
        {
            if (stateIndex == AnimationStateView.AnyState)
            {
                return new Rect(AnyStatePosition(), new Size(StateWidth, StateHeight));
            }

            AnimationStateView? stateView = _viewModel!.States.FirstOrDefault(s => s.Index == stateIndex);

            return stateView != null ? new Rect(stateView.Position, new Size(StateWidth, StateHeight)) : default;
        }

        private int? HitState(Point world)
        {
            if (_viewModel!.States.LastOrDefault(s => StateRect(s.Index).Contains(world)) is { } stateView)
            {
                return stateView.Index;
            }

            if (StateRect(AnimationStateView.AnyState).Contains(world))
            {
                return AnimationStateView.AnyState;
            }

            return null;
        }

        private bool IsOnStateEdge(int stateIndex, Point world)
        {
            Rect rect = StateRect(stateIndex);
            double edge = StateEdge / Math.Min(Zoom, 1.0);

            return rect.Contains(world) && !rect.Deflate(edge).Contains(world);
        }

        // where a transition's arrow runs, between the edges of its two boxes and offset to one side so a pair going both ways stays apart
        private (Point From, Point To)? TransitionLine(AnimationTransitionView transition)
        {
            Rect from = StateRect(transition.From);
            Rect to = StateRect(transition.To);

            if (from == default || to == default)
            {
                return null;
            }

            Vector direction = to.Center - from.Center;
            double length = Math.Sqrt(direction.X * direction.X + direction.Y * direction.Y);

            if (length < 1.0)
            {
                return null;
            }

            direction /= length;

            Vector side = new Vector(-direction.Y, direction.X) * TransitionOffset;

            return (ClipToEdge(from, direction) + side, ClipToEdge(to, -direction) + side);
        }

        private static Point ClipToEdge(Rect rect, Vector direction)
        {
            double scaleX = Math.Abs(direction.X) > 1e-6 ? rect.Width * 0.5 / Math.Abs(direction.X) : double.MaxValue;
            double scaleY = Math.Abs(direction.Y) > 1e-6 ? rect.Height * 0.5 / Math.Abs(direction.Y) : double.MaxValue;

            return rect.Center + direction * Math.Min(scaleX, scaleY);
        }

        private int? HitTransition(Point world)
        {
            double tolerance = 6.0 / Zoom;

            foreach (AnimationTransitionView transition in Enumerable.Reverse(_viewModel!.Transitions))
            {
                if (TransitionLine(transition) is { } line && DistanceToSegment(world, line.From, line.To) <= tolerance)
                {
                    return transition.Index;
                }
            }

            return null;
        }

        private void RenderStateMachine(DrawingContext context, Palette palette)
        {
            AnimationGraphDocumentViewModel viewModel = _viewModel!;

            foreach (AnimationTransitionView transition in viewModel.Transitions)
            {
                if (TransitionLine(transition) is not { } line)
                {
                    continue;
                }

                Pen pen = palette.Wire;

                if (transition.Index == viewModel.SelectedTransition)
                {
                    pen = palette.Selected;
                }
                else if (transition.IsActive)
                {
                    pen = palette.ActiveWire;
                }
                else if (viewModel.UsesSelectedParameter(transition.Parameters))
                {
                    pen = palette.Highlight;
                }

                DrawArrow(context, pen, line.From, line.To);
            }

            DrawState(context, palette, StateRect(AnimationStateView.AnyState), "Any State", string.Empty, palette.Output, false, 0.0f);

            foreach (AnimationStateView stateView in viewModel.States)
            {
                Pen pen = palette.Border;

                if (stateView.Index == viewModel.SelectedState)
                {
                    pen = palette.Selected;
                }
                else if (stateView.IsCurrent)
                {
                    pen = palette.ActiveWire;
                }

                Rect rect = StateRect(stateView.Index);

                DrawState(context, palette, rect, stateView.Name, stateView.InputTitle, pen, stateView.IsCurrent || stateView.IsPrevious, stateView.Progress);

                if (stateView.IsEntry)
                {
                    context.FillRectangle(palette.Output.Brush!, new Rect(rect.X, rect.Y, 4.0, rect.Height));
                }
            }

            if (_transitionSource is { } source && StateRect(source) is var sourceRect && sourceRect != default)
            {
                DrawArrow(context, palette.ActiveWire, sourceRect.Center, ToWorld(_pointer));
            }
        }

        private void DrawState(DrawingContext context, Palette palette, Rect rect, string title, string subtitle, Pen pen, bool isActive, float progress)
        {
            context.DrawRectangle(isActive ? palette.Header : palette.Node, pen, rect, 6.0, 6.0);

            DrawText(context, title, 12.0, palette.Text, new Point(rect.X + 10.0, rect.Y + 6.0), rect.Width - 16.0);

            if (subtitle.Length > 0)
            {
                DrawText(context, subtitle, 10.0, palette.Muted, new Point(rect.X + 10.0, rect.Y + 24.0), rect.Width - 16.0);
            }

            if (progress > 0.0f)
            {
                context.FillRectangle(palette.Accent, new Rect(rect.X + 4.0, rect.Bottom - 4.0, (rect.Width - 8.0) * Math.Min(progress, 1.0f), 2.0));
            }
        }

        private static void DrawArrow(DrawingContext context, Pen pen, Point from, Point to)
        {
            context.DrawLine(pen, from, to);

            Vector direction = to - from;
            double length = Math.Sqrt(direction.X * direction.X + direction.Y * direction.Y);

            if (length < 1.0)
            {
                return;
            }

            direction /= length;

            Vector side = new Vector(-direction.Y, direction.X);
            Point tip = from + (to - from) * 0.5 + direction * 6.0;

            StreamGeometry geometry = new StreamGeometry();

            using (StreamGeometryContext geometryContext = geometry.Open())
            {
                geometryContext.BeginFigure(tip, true);
                geometryContext.LineTo(tip - direction * 10.0 + side * 5.0);
                geometryContext.LineTo(tip - direction * 10.0 - side * 5.0);
                geometryContext.EndFigure(true);
            }

            context.DrawGeometry(pen.Brush, null, geometry);
        }

        private void PressStateMachine(Point world, int clickCount)
        {
            AnimationGraphDocumentViewModel viewModel = _viewModel!;
            int? hitState = HitState(world);

            if (_transitionSource is { } source)
            {
                if (hitState is { } target && target >= 0)
                {
                    viewModel.AddTransition(source, target);
                }

                _transitionSource = null;
                _dragMode = DragMode.None;

                return;
            }

            if (hitState is { } stateIndex)
            {
                if (clickCount == 2 && stateIndex >= 0)
                {
                    viewModel.OpenStateInput(stateIndex);
                    _dragMode = DragMode.None;

                    return;
                }

                if (IsEditable && (stateIndex == AnimationStateView.AnyState || IsOnStateEdge(stateIndex, world)))
                {
                    _dragMode = DragMode.Transition;
                    _transitionSource = stateIndex;

                    return;
                }

                viewModel.SelectState(stateIndex);

                if (stateIndex >= 0 && IsEditable)
                {
                    _dragMode = DragMode.MoveState;
                    _dragNode = stateIndex;
                    _dragNodeStart = StateRect(stateIndex).Position;
                }

                return;
            }

            if (HitTransition(world) is { } transitionIndex)
            {
                viewModel.SelectTransition(transitionIndex);

                return;
            }

            _dragMode = DragMode.Pan;

            viewModel.SelectState(AnimationStateView.AnyState);
        }

        private void ShowStateMachineContextMenu(Point world)
        {
            AnimationGraphDocumentViewModel viewModel = _viewModel!;
            ContextMenu menu = new ContextMenu();

            if (HitState(world) is { } stateIndex)
            {
                if (IsEditable)
                {
                    MenuItem makeTransition = new MenuItem { Header = "Make Transition" };
                    makeTransition.Click += (_, _) => _transitionSource = stateIndex;

                    menu.Items.Add(makeTransition);
                }

                if (stateIndex >= 0)
                {
                    MenuItem open = new MenuItem { Header = "Show Pose" };
                    open.Click += (_, _) => viewModel.OpenStateInput(stateIndex);

                    menu.Items.Add(open);

                    if (IsEditable)
                    {
                        MenuItem entry = new MenuItem { Header = "Set as Entry" };
                        entry.Click += (_, _) => viewModel.SetEntryState(stateIndex);

                        MenuItem delete = new MenuItem { Header = "Delete" };
                        delete.Click += (_, _) => viewModel.RemoveState(stateIndex);

                        menu.Items.Add(entry);
                        menu.Items.Add(delete);
                    }
                }
            }
            else if (HitTransition(world) is { } transitionIndex)
            {
                if (IsEditable)
                {
                    MenuItem delete = new MenuItem { Header = "Delete" };
                    delete.Click += (_, _) => viewModel.RemoveTransition(transitionIndex);

                    menu.Items.Add(delete);
                }
            }
            else
            {
                if (IsEditable)
                {
                    MenuItem add = new MenuItem { Header = "Add State" };
                    add.Click += (_, _) => viewModel.AddState(world);

                    menu.Items.Add(add);
                }

                MenuItem back = new MenuItem { Header = "Back to Graph" };
                back.Click += (_, _) => viewModel.CloseStateMachine();

                menu.Items.Add(back);
            }

            if (menu.Items.Count > 0)
            {
                menu.Open(this);
            }
        }

        #endregion State machine

        private sealed class Palette
        {
            public IBrush Node = Brushes.Black;
            public IBrush Header = Brushes.Black;
            public IBrush Text = Brushes.White;
            public IBrush Muted = Brushes.Gray;
            public IBrush Accent = Brushes.Blue;
            public IBrush BoxFill = Brushes.Transparent;
            public Dictionary<AnimationGraphNodeCategory, IBrush> CategoryHeaders = new();
            public Pen Wire = new Pen();
            public Pen ActiveWire = new Pen();
            public Pen Border = new Pen();
            public Pen Selected = new Pen();
            public Pen Output = new Pen();
            public Pen Highlight = new Pen();
            public Pen Invalid = new Pen();
        }

        public override void Render(DrawingContext context)
        {
            Palette palette = new Palette
            {
                Node = Resource("Brush.PanelRaised", Color.Parse("#FF0E0F12")),
                Header = Resource("Brush.PanelAlt", Color.Parse("#FF0A0B0D")),
                Text = Resource("Brush.Text", Color.Parse("#FFE0E0E0")),
                Muted = Resource("Brush.TextMuted", Color.Parse("#FF9A9AA0")),
                Accent = Resource("Brush.Accent", Color.Parse("#FF3D7EFF"))
            };

            foreach ((AnimationGraphNodeCategory category, Color color) in CategoryColors)
            {
                palette.CategoryHeaders[category] = new SolidColorBrush(color, 0.55);
            }

            palette.BoxFill = new SolidColorBrush(Color.Parse("#FF3D7EFF"), 0.12);
            palette.Wire = new Pen(palette.Muted, 2.0);
            palette.ActiveWire = new Pen(palette.Accent, 2.5);
            palette.Border = new Pen(Resource("Brush.BorderLight", Color.Parse("#FF1A1C1F")), 1.0);
            palette.Selected = new Pen(palette.Text, 2.0);
            palette.Output = new Pen(new SolidColorBrush(OutputColor), 2.0);
            palette.Highlight = new Pen(palette.Accent, 2.0) { DashStyle = DashStyle.Dash };
            palette.Invalid = new Pen(new SolidColorBrush(InvalidColor), 2.5);

            Pen gridPen = new Pen(Resource("Brush.Border", Color.Parse("#FF121416")), 1.0);

            context.FillRectangle(Resource("Brush.Background", Color.Parse("#FF020304")), new Rect(Bounds.Size));

            double spacing = GridSpacing * Zoom;

            for (double x = Pan.X % spacing; x < Bounds.Width; x += spacing)
            {
                context.DrawLine(gridPen, new Point(x, 0.0), new Point(x, Bounds.Height));
            }

            for (double y = Pan.Y % spacing; y < Bounds.Height; y += spacing)
            {
                context.DrawLine(gridPen, new Point(0.0, y), new Point(Bounds.Width, y));
            }

            if (_viewModel == null)
            {
                return;
            }

            using (context.PushTransform(Matrix.CreateScale(Zoom, Zoom) * Matrix.CreateTranslation(Pan)))
            {
                if (InStateMachine)
                {
                    RenderStateMachine(context, palette);
                }
                else
                {
                    RenderGraph(context, palette);
                }
            }

        }

        private static void DrawWire(DrawingContext context, Pen pen, Point from, Point to)
        {
            double tangent = Math.Max(40.0, Math.Abs(to.X - from.X) * 0.5);

            StreamGeometry geometry = new StreamGeometry();

            using (StreamGeometryContext geometryContext = geometry.Open())
            {
                geometryContext.BeginFigure(from, false);
                geometryContext.CubicBezierTo(new Point(from.X + tangent, from.Y), new Point(to.X - tangent, to.Y), to);
                geometryContext.EndFigure(false);
            }

            context.DrawGeometry(null, pen, geometry);
        }

        private static double MeasureText(string text, double size)
        {
            return new FormattedText(text, CultureInfo.InvariantCulture, FlowDirection.LeftToRight, LabelTypeface, size, Brushes.White).Width;
        }

        private static void DrawText(DrawingContext context, string text, double size, IBrush brush, Point origin, double maxWidth)
        {
            FormattedText formatted = new FormattedText(text, CultureInfo.InvariantCulture, FlowDirection.LeftToRight, LabelTypeface, size, brush)
            {
                MaxTextWidth = Math.Max(maxWidth, 1.0),
                MaxLineCount = 1,
                Trimming = TextTrimming.CharacterEllipsis
            };

            context.DrawText(formatted, origin);
        }

        private static double Distance(Point a, Point b) => Math.Sqrt((a.X - b.X) * (a.X - b.X) + (a.Y - b.Y) * (a.Y - b.Y));

        private static double DistanceToSegment(Point point, Point a, Point b)
        {
            Vector segment = b - a;
            double lengthSquared = segment.X * segment.X + segment.Y * segment.Y;

            if (lengthSquared < 1e-9)
            {
                return Distance(point, a);
            }

            double t = Math.Clamp(((point.X - a.X) * segment.X + (point.Y - a.Y) * segment.Y) / lengthSquared, 0.0, 1.0);

            return Distance(point, a + segment * t);
        }

        protected override void OnPointerPressed(PointerPressedEventArgs e)
        {
            base.OnPointerPressed(e);

            if (_viewModel == null)
            {
                return;
            }

            Focus();

            PointerPoint pointer = e.GetCurrentPoint(this);
            Point world = ToWorld(pointer.Position);

            _dragStart = pointer.Position;
            _pointer = pointer.Position;
            _dragMoved = false;
            _isRightPan = false;

            if (pointer.Properties.IsMiddleButtonPressed)
            {
                _dragMode = DragMode.Pan;
            }
            else if (pointer.Properties.IsRightButtonPressed)
            {
                _transitionSource = null;
                _dragMode = DragMode.Pan;
                _isRightPan = true;
            }
            else if (pointer.Properties.IsLeftButtonPressed)
            {
                if (InStateMachine)
                {
                    PressStateMachine(world, e.ClickCount);
                }
                else
                {
                    PressGraph(world, e.ClickCount, e.KeyModifiers);
                }
            }

            e.Pointer.Capture(this);
            e.Handled = true;

            InvalidateVisual();
        }

        protected override void OnPointerMoved(PointerEventArgs e)
        {
            base.OnPointerMoved(e);

            Point position = e.GetPosition(this);
            Vector moved = position - _pointer;

            _pointer = position;
            _dragMoved |= Distance(position, _dragStart) > 3.0;

            switch (_dragMode)
            {
            case DragMode.Pan:
                Pan += moved;
                InvalidateVisual();
                break;
            case DragMode.MoveNodes:
                if (IsEditable)
                {
                    foreach ((int index, Point start) in _dragStarts)
                    {
                        if (FindNode(index) is { } node)
                        {
                            node.Position = start + (position - _dragStart) / Zoom;
                        }
                    }

                    InvalidateVisual();
                }

                break;
            case DragMode.MoveState:
                if (_viewModel?.States.FirstOrDefault(s => s.Index == _dragNode) is { } stateView)
                {
                    stateView.Position = _dragNodeStart + (position - _dragStart) / Zoom;
                    InvalidateVisual();
                }

                break;
            case DragMode.Wire:
            case DragMode.BoxSelect:
            case DragMode.Transition:
                InvalidateVisual();
                break;
            default:
                if (_transitionSource != null)
                {
                    InvalidateVisual();
                }

                UpdateHoverCursor(ToWorld(position));

                break;
            }
        }

        private void UpdateHoverCursor(Point world)
        {
            if (_viewModel == null || !IsEditable || !InStateMachine)
            {
                Cursor = Cursor.Default;

                return;
            }

            bool onEdge = HitState(world) is { } stateIndex && (stateIndex == AnimationStateView.AnyState || IsOnStateEdge(stateIndex, world));

            if (onEdge)
            {
                Cursor = new Cursor(StandardCursorType.Cross);
            }
            else
            {
                Cursor = Cursor.Default;
            }
        }

        protected override void OnPointerReleased(PointerReleasedEventArgs e)
        {
            base.OnPointerReleased(e);

            if (_viewModel != null)
            {
                Point world = ToWorld(e.GetPosition(this));

                if (_isRightPan && !_dragMoved)
                {
                    if (InStateMachine)
                    {
                        ShowStateMachineContextMenu(world);
                    }
                    else
                    {
                        ShowGraphContextMenu(world);
                    }
                }
                else if (_dragMode == DragMode.MoveNodes && _dragMoved && IsEditable)
                {
                    List<(int, Point)> moves = _dragStarts.Keys.Select(index => FindNode(index)).Where(n => n != null).Select(n => (n!.Index, n.Position)).ToList();

                    _viewModel.MoveNodes(moves);
                }
                else if (_dragMode == DragMode.MoveState && _dragMoved && _viewModel.States.FirstOrDefault(s => s.Index == _dragNode) is { } stateView)
                {
                    _viewModel.MoveState(stateView.Index, stateView.Position);
                }
                else if (_dragMode == DragMode.BoxSelect)
                {
                    FinishBoxSelect(e.KeyModifiers);
                }
                else if (_dragMode == DragMode.Transition && _transitionSource is { } source)
                {
                    if (HitState(world) is { } target && target >= 0 && target != source)
                    {
                        _viewModel.AddTransition(source, target);
                        _transitionSource = null;
                    }
                    else if (_dragMoved)
                    {
                        _transitionSource = null;
                    }
                    else if (source >= 0)
                    {
                        _transitionSource = null;
                        _viewModel.SelectState(source);
                    }
                }
                else if (_dragMode == DragMode.Wire)
                {
                    ReleaseWire(world);
                }
            }

            _dragMode = DragMode.None;
            _dragNode = AnimationGraphNodeView.NoInput;
            _rewiredInput = null;
            _dragStarts.Clear();

            e.Pointer.Capture(null);

            InvalidateVisual();
        }

        private void ReleaseWire(Point world)
        {
            AnimationGraphDocumentViewModel viewModel = _viewModel!;
            int source = _dragNode;

            if (HitOutputTerminal(world) && _rewiredInput == null)
            {
                viewModel.SetOutput(source);

                return;
            }

            if (_rewiredInput is { } rewired)
            {
                if (HitInputPin(world) is { } input)
                {
                    if (input.Node.Index != source && !viewModel.WouldCycle(source, input.Node.Index))
                    {
                        viewModel.Rewire(rewired.Node, rewired.Slot, input.Node.Index, input.Slot);
                    }
                }
                else if (_dragMoved)
                {
                    viewModel.Disconnect(rewired.Node, rewired.Slot);
                }

                return;
            }

            if (HitInputPin(world) is { } target)
            {
                if (target.Node.Index != source && !viewModel.WouldCycle(source, target.Node.Index))
                {
                    viewModel.Connect(source, target.Node.Index, target.Slot);
                }

                return;
            }

            if (_dragMoved && HitNode(world) == null)
            {
                ShowAddNodePopup(world, source);
            }
        }

        protected override void OnPointerWheelChanged(PointerWheelEventArgs e)
        {
            base.OnPointerWheelChanged(e);

            Point position = e.GetPosition(this);
            Point world = ToWorld(position);

            Zoom = Math.Clamp(Zoom * (e.Delta.Y > 0 ? 1.1 : 1.0 / 1.1), MinZoom, MaxZoom);

            Pan = new Vector(position.X - world.X * Zoom, position.Y - world.Y * Zoom);

            InvalidateVisual();
            e.Handled = true;
        }

        protected override void OnKeyDown(KeyEventArgs e)
        {
            base.OnKeyDown(e);

            if (_viewModel == null)
            {
                return;
            }

            bool isControl = (e.KeyModifiers & KeyModifiers.Control) != 0;

            switch (e.Key)
            {
            case Key.Escape:
                if (_transitionSource != null)
                {
                    _transitionSource = null;
                    InvalidateVisual();
                }
                else
                {
                    _viewModel.CloseStateMachine();
                }

                e.Handled = true;

                return;
            case Key.F:
                Frame();
                e.Handled = true;

                return;
            case Key.Home:
                _viewModel.Select(AnimationGraphNodeView.NoInput);
                Frame();
                e.Handled = true;

                return;
            case Key.Space:
                OnAddNodeAtPointer();
                e.Handled = true;

                return;
            }

            if (!IsEditable)
            {
                return;
            }

            if (InStateMachine)
            {
                if (e.Key == Key.Delete && _viewModel.SelectedTransition >= 0)
                {
                    _viewModel.RemoveTransition(_viewModel.SelectedTransition);
                    e.Handled = true;
                }
                else if (e.Key == Key.Delete && _viewModel.SelectedState >= 0)
                {
                    _viewModel.RemoveState(_viewModel.SelectedState);
                    e.Handled = true;
                }

                return;
            }

            List<int> selection = _viewModel.SelectedNodes.ToList();

            if (e.Key == Key.Delete && selection.Count > 0)
            {
                _viewModel.RemoveNodes(selection);
                e.Handled = true;
            }
            else if (e.Key == Key.F2 && FindNode(_viewModel.SelectedNode) is { } node)
            {
                ShowRename(node);
                e.Handled = true;
            }
            else if (isControl && e.Key == Key.A)
            {
                _viewModel.SetNodeSelection(_viewModel.Nodes.Select(n => n.Index), AnimationGraphNodeView.NoInput);
                e.Handled = true;
            }
            else if (isControl && e.Key == Key.C && selection.Count > 0)
            {
                _viewModel.CopyNodes(selection);
                e.Handled = true;
            }
            else if (isControl && e.Key == Key.V)
            {
                _viewModel.PasteNodes(ToWorld(_pointer));
                e.Handled = true;
            }
            else if (isControl && e.Key == Key.D && selection.Count > 0)
            {
                _viewModel.DuplicateNodes(selection, new Vector(PasteOffset, PasteOffset));
                e.Handled = true;
            }
        }

        private void OnAddNodeAtPointer()
        {
            if (!IsEditable)
            {
                return;
            }

            if (InStateMachine)
            {
                _viewModel!.AddState(ToWorld(_pointer));

                return;
            }

            ShowAddNodePopup(ToWorld(_pointer), AnimationGraphNodeView.NoInput);
        }

        private static bool TryGetDraggedClip(DragEventArgs e, out string clipName)
        {
            clipName = string.Empty;

            if (e.DataTransfer.TryGetValue(AssetDragFormat) is not { } payload)
            {
                return false;
            }

            string[] fields = payload.Split('|', 2);

            if (fields.Length != 2 || !uint.TryParse(fields[0], out uint bucketIndex) || bucketIndex != AssetBucket.Animations.Value)
            {
                return false;
            }

            clipName = fields[1];

            return true;
        }

        private void OnDragOver(object? sender, DragEventArgs e)
        {
            e.DragEffects = IsEditable && !InStateMachine && TryGetDraggedClip(e, out _) ? DragDropEffects.Copy : DragDropEffects.None;
            e.Handled = true;
        }

        private void OnDrop(object? sender, DragEventArgs e)
        {
            if (_viewModel != null && IsEditable && !InStateMachine && TryGetDraggedClip(e, out string clipName))
            {
                _viewModel.AddClipNode(clipName, ToWorld(e.GetPosition(this)));
                e.Handled = true;
            }
        }
    }
}
