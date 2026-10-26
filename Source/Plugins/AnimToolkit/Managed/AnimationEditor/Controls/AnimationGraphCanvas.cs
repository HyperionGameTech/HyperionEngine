using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using Avalonia;
using Avalonia.Controls;
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
        private const double RowHeight = 18.0;
        private const double PinRadius = 5.0;
        private const double StateWidth = 150.0;
        private const double StateHeight = 46.0;
        private const double TransitionOffset = 5.0;
        private const double GridSpacing = 32.0;
        private const double MinZoom = 0.25;
        private const double MaxZoom = 2.0;

        private static readonly DataFormat<string> AssetDragFormat = DataFormat.CreateStringApplicationFormat("hyperion-asset");
        private static readonly Typeface LabelTypeface = new Typeface(FontFamily.Default);
        private static readonly Color OutputColor = Color.Parse("#FFFFB347");

        private AnimationGraphDocumentViewModel? _viewModel;

        private Vector _graphPan = new Vector(40.0, 40.0);
        private double _graphZoom = 1.0;
        private Vector _statePan = new Vector(260.0, 60.0);
        private double _stateZoom = 1.0;

        private enum DragMode
        {
            None,
            Pan,
            MoveNode,
            Wire,
            MoveState
        }

        private DragMode _dragMode;
        private Point _dragStart;
        private Point _pointer;
        private int _dragNode = AnimationGraphNodeView.NoInput;
        private Point _dragNodeStart;
        private bool _dragMoved;
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
            }

            _viewModel = DataContext as AnimationGraphDocumentViewModel;

            if (_viewModel != null)
            {
                _viewModel.GraphChanged += InvalidateVisual;
                _viewModel.PreviewChanged += InvalidateVisual;
                _viewModel.FocusNodeRequested += FocusNode;
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

        #region Graph

        private static int NumRows(AnimationGraphNodeView node) => Math.Max(1, node.Inputs.Count + (node.CanAddInputs ? 1 : 0));

        private static double StatusHeight(AnimationGraphNodeView node) => node.IsStateMachine ? RowHeight : 0.0;

        private static Rect NodeRect(AnimationGraphNodeView node) => new Rect(node.Position, new Size(NodeWidth, HeaderHeight + NumRows(node) * RowHeight + StatusHeight(node) + 6.0));

        private static Point OutputPin(AnimationGraphNodeView node) => new Point(node.Position.X + NodeWidth, node.Position.Y + HeaderHeight * 0.5);

        private static Point InputPin(AnimationGraphNodeView node, int slot) => new Point(node.Position.X, node.Position.Y + HeaderHeight + (slot + 0.5) * RowHeight);

        private AnimationGraphNodeView? FindNode(int index) => _viewModel?.Nodes.FirstOrDefault(node => node.Index == index);

        private AnimationGraphNodeView? HitNode(Point world)
        {
            return _viewModel?.Nodes.LastOrDefault(node => NodeRect(node).Contains(world));
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

            foreach (AnimationGraphNodeView node in viewModel.Nodes)
            {
                Rect rect = NodeRect(node);
                Pen nodePen = palette.Border;

                if (node.Index == viewModel.SelectedNode)
                {
                    nodePen = palette.Selected;
                }
                else if (node.IsOutput)
                {
                    nodePen = palette.Output;
                }

                context.DrawRectangle(palette.Node, nodePen, rect, 4.0, 4.0);
                context.DrawRectangle(palette.Header, null, new Rect(rect.X, rect.Y, rect.Width, HeaderHeight), 4.0, 4.0);

                if (node.Weight > 0.0f)
                {
                    context.FillRectangle(palette.Accent, new Rect(rect.X, rect.Y + HeaderHeight - 2.0, rect.Width * Math.Min(node.Weight, 1.0f), 2.0));
                }

                DrawText(context, node.Title, 11.0, palette.Text, new Point(rect.X + 8.0, rect.Y + 4.0), NodeWidth - 30.0);

                if (node.Status.Length > 0)
                {
                    DrawText(context, node.Status, 10.0, palette.Accent, new Point(rect.X + 8.0, rect.Bottom - RowHeight - 3.0), NodeWidth - 16.0);
                }

                if (node.Title != node.Kind)
                {
                    DrawText(context, node.Kind, 9.0, palette.Muted, new Point(rect.Right - 8.0 - MeasureText(node.Kind, 9.0), rect.Y + 6.0), 60.0);
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

            if (_dragMode == DragMode.Wire && FindNode(_dragNode) is { } wireSource)
            {
                DrawWire(context, palette.ActiveWire, OutputPin(wireSource), ToWorld(_pointer));
            }
        }

        private void PressGraph(Point world, int clickCount)
        {
            AnimationGraphDocumentViewModel viewModel = _viewModel!;

            if (clickCount == 2 && HitNode(world) is { IsStateMachine: true } stateMachine)
            {
                viewModel.OpenStateMachineNode(stateMachine.Index);
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
                _dragMode = DragMode.MoveNode;
                _dragNode = node.Index;
                _dragNodeStart = node.Position;

                viewModel.Select(node.Index);
            }
            else
            {
                _dragMode = DragMode.Pan;

                viewModel.Select(AnimationGraphNodeView.NoInput);
            }
        }

        private void ShowGraphContextMenu(Point world)
        {
            AnimationGraphDocumentViewModel viewModel = _viewModel!;
            ContextMenu menu = new ContextMenu();

            if (HitNode(world) is { } node)
            {
                if (node.IsStateMachine)
                {
                    MenuItem open = new MenuItem { Header = "Open" };
                    open.Click += (_, _) => viewModel.OpenStateMachineNode(node.Index);

                    menu.Items.Add(open);
                }

                if (IsEditable)
                {
                    MenuItem output = new MenuItem { Header = "Set as Output", IsEnabled = !node.IsOutput };
                    output.Click += (_, _) => viewModel.SetOutput(node.Index);

                    MenuItem delete = new MenuItem { Header = "Delete" };
                    delete.Click += (_, _) => viewModel.RemoveNode(node.Index);

                    menu.Items.Add(output);
                    menu.Items.Add(delete);
                }
            }
            else if (IsEditable)
            {
                foreach ((string className, string label) in viewModel.NodeKinds)
                {
                    MenuItem add = new MenuItem { Header = $"Add {label}" };
                    add.Click += (_, _) => viewModel.AddNode(className, world);

                    menu.Items.Add(add);
                }
            }

            if (menu.Items.Count > 0)
            {
                menu.Open(this);
            }
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
                    MenuItem open = new MenuItem { Header = "Open Input" };
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
            public Pen Wire = new Pen();
            public Pen ActiveWire = new Pen();
            public Pen Border = new Pen();
            public Pen Selected = new Pen();
            public Pen Output = new Pen();
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

            palette.Wire = new Pen(palette.Muted, 2.0);
            palette.ActiveWire = new Pen(palette.Accent, 2.5);
            palette.Border = new Pen(Resource("Brush.BorderLight", Color.Parse("#FF1A1C1F")), 1.0);
            palette.Selected = new Pen(palette.Text, 2.0);
            palette.Output = new Pen(new SolidColorBrush(OutputColor), 2.0);

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

            if (pointer.Properties.IsMiddleButtonPressed)
            {
                _dragMode = DragMode.Pan;
            }
            else if (pointer.Properties.IsRightButtonPressed)
            {
                _transitionSource = null;

                if (InStateMachine)
                {
                    ShowStateMachineContextMenu(world);
                }
                else
                {
                    ShowGraphContextMenu(world);
                }

                e.Handled = true;

                return;
            }
            else if (pointer.Properties.IsLeftButtonPressed)
            {
                if (InStateMachine)
                {
                    PressStateMachine(world, e.ClickCount);
                }
                else
                {
                    PressGraph(world, e.ClickCount);
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
            case DragMode.MoveNode:
                if (IsEditable && FindNode(_dragNode) is { } node)
                {
                    node.Position = _dragNodeStart + (position - _dragStart) / Zoom;
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
                InvalidateVisual();
                break;
            default:
                if (_transitionSource != null)
                {
                    InvalidateVisual();
                }

                break;
            }
        }

        protected override void OnPointerReleased(PointerReleasedEventArgs e)
        {
            base.OnPointerReleased(e);

            if (_viewModel != null)
            {
                Point world = ToWorld(e.GetPosition(this));

                if (_dragMode == DragMode.MoveNode && _dragMoved && IsEditable && FindNode(_dragNode) is { } node)
                {
                    _viewModel.MoveNode(node.Index, node.Position);
                }
                else if (_dragMode == DragMode.MoveState && _dragMoved && _viewModel.States.FirstOrDefault(s => s.Index == _dragNode) is { } stateView)
                {
                    _viewModel.MoveState(stateView.Index, stateView.Position);
                }
                else if (_dragMode == DragMode.Wire && _rewiredInput is { } rewired)
                {
                    if (HitInputPin(world) is { } input)
                    {
                        if (input.Node.Index != _dragNode)
                        {
                            _viewModel.Rewire(rewired.Node, rewired.Slot, input.Node.Index, input.Slot);
                        }
                    }
                    else if (_dragMoved)
                    {
                        _viewModel.Disconnect(rewired.Node, rewired.Slot);
                    }
                }
                else if (_dragMode == DragMode.Wire && HitInputPin(world) is { } input && input.Node.Index != _dragNode)
                {
                    _viewModel.Connect(_dragNode, input.Node.Index, input.Slot);
                }
            }

            _dragMode = DragMode.None;
            _dragNode = AnimationGraphNodeView.NoInput;
            _rewiredInput = null;

            e.Pointer.Capture(null);

            InvalidateVisual();
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

            if (e.Key == Key.Escape)
            {
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
            }

            if (e.Key != Key.Delete || !IsEditable)
            {
                return;
            }

            if (InStateMachine && _viewModel.SelectedTransition >= 0)
            {
                _viewModel.RemoveTransition(_viewModel.SelectedTransition);
            }
            else if (InStateMachine && _viewModel.SelectedState >= 0)
            {
                _viewModel.RemoveState(_viewModel.SelectedState);
            }
            else if (!InStateMachine && _viewModel.SelectedNode >= 0)
            {
                _viewModel.RemoveNode(_viewModel.SelectedNode);
            }

            e.Handled = true;
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
