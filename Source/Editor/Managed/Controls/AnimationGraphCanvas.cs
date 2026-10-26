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
        private const double GridSpacing = 32.0;
        private const double MinZoom = 0.25;
        private const double MaxZoom = 2.0;

        private static readonly DataFormat<string> AssetDragFormat = DataFormat.CreateStringApplicationFormat("hyperion-asset");
        private static readonly Typeface LabelTypeface = new Typeface(FontFamily.Default);

        private AnimationGraphDocumentViewModel? _viewModel;

        private Vector _pan = new Vector(40.0, 40.0);
        private double _zoom = 1.0;

        private enum DragMode
        {
            None,
            Pan,
            MoveNode,
            Wire
        }

        private DragMode _dragMode;
        private Point _dragStart;
        private Point _pointer;
        private int _dragNode = AnimationGraphNodeView.NoInput;
        private Point _dragNodeStart;
        private bool _dragMoved;

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
            }

            _viewModel = DataContext as AnimationGraphDocumentViewModel;

            if (_viewModel != null)
            {
                _viewModel.GraphChanged += InvalidateVisual;
                _viewModel.PreviewChanged += InvalidateVisual;
            }

            InvalidateVisual();
        }

        private IBrush Resource(string key, Color fallback)
        {
            return this.TryFindResource(key, ActualThemeVariant, out object? resource) && resource is IBrush brush
                ? brush
                : new SolidColorBrush(fallback);
        }

        private Point ToScreen(Point world) => new Point(world.X * _zoom + _pan.X, world.Y * _zoom + _pan.Y);

        private Point ToWorld(Point screen) => new Point((screen.X - _pan.X) / _zoom, (screen.Y - _pan.Y) / _zoom);

        private static int NumRows(AnimationGraphNodeView node) => Math.Max(1, node.Inputs.Count + (node.CanAddInputs ? 1 : 0));

        private static double StatusHeight(AnimationGraphNodeView node) => node.Kind == "State Machine" ? RowHeight : 0.0;

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
            double radius = (PinRadius + 3.0) / Math.Min(_zoom, 1.0);

            return _viewModel?.Nodes.LastOrDefault(node => Distance(OutputPin(node), world) <= radius);
        }

        private (AnimationGraphNodeView Node, int Slot)? HitInputPin(Point world)
        {
            if (_viewModel == null)
            {
                return null;
            }

            double radius = (PinRadius + 3.0) / Math.Min(_zoom, 1.0);

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

        private static double Distance(Point a, Point b) => Math.Sqrt((a.X - b.X) * (a.X - b.X) + (a.Y - b.Y) * (a.Y - b.Y));

        public override void Render(DrawingContext context)
        {
            IBrush background = Resource("Brush.Background", Color.Parse("#FF020304"));
            IBrush nodeBrush = Resource("Brush.PanelRaised", Color.Parse("#FF0E0F12"));
            IBrush headerBrush = Resource("Brush.PanelAlt", Color.Parse("#FF0A0B0D"));
            IBrush textBrush = Resource("Brush.Text", Color.Parse("#FFE0E0E0"));
            IBrush mutedBrush = Resource("Brush.TextMuted", Color.Parse("#FF9A9AA0"));
            IBrush accent = Resource("Brush.Accent", Color.Parse("#FF3D7EFF"));
            IBrush borderBrush = Resource("Brush.BorderLight", Color.Parse("#FF1A1C1F"));

            Pen gridPen = new Pen(Resource("Brush.Border", Color.Parse("#FF121416")), 1.0);
            Pen wirePen = new Pen(mutedBrush, 2.0);
            Pen activeWirePen = new Pen(accent, 2.5);
            Pen borderPen = new Pen(borderBrush, 1.0);
            Pen selectedPen = new Pen(accent, 2.0);
            Pen outputPen = new Pen(new SolidColorBrush(Color.Parse("#FFFFB347")), 2.0);

            context.FillRectangle(background, new Rect(Bounds.Size));

            double spacing = GridSpacing * _zoom;

            for (double x = _pan.X % spacing; x < Bounds.Width; x += spacing)
            {
                context.DrawLine(gridPen, new Point(x, 0.0), new Point(x, Bounds.Height));
            }

            for (double y = _pan.Y % spacing; y < Bounds.Height; y += spacing)
            {
                context.DrawLine(gridPen, new Point(0.0, y), new Point(Bounds.Width, y));
            }

            if (_viewModel == null)
            {
                return;
            }

            using (context.PushTransform(Matrix.CreateScale(_zoom, _zoom) * Matrix.CreateTranslation(_pan)))
            {
                foreach (AnimationGraphNodeView node in _viewModel.Nodes)
                {
                    for (int slot = 0; slot < node.Inputs.Count; ++slot)
                    {
                        if (FindNode(node.Inputs[slot].Source) is { } source)
                        {
                            DrawWire(context, source.Weight > 0.0f && node.Weight > 0.0f ? activeWirePen : wirePen, OutputPin(source), InputPin(node, slot));
                        }
                    }
                }

                foreach (AnimationGraphNodeView node in _viewModel.Nodes)
                {
                    Rect rect = NodeRect(node);
                    Pen nodePen = borderPen;

                    if (node.Index == _viewModel.SelectedNode)
                    {
                        nodePen = selectedPen;
                    }
                    else if (node.IsOutput)
                    {
                        nodePen = outputPen;
                    }

                    context.DrawRectangle(nodeBrush, nodePen, rect, 4.0, 4.0);
                    context.DrawRectangle(headerBrush, null, new Rect(rect.X, rect.Y, rect.Width, HeaderHeight), 4.0, 4.0);

                    if (node.Weight > 0.0f)
                    {
                        context.FillRectangle(accent, new Rect(rect.X, rect.Y + HeaderHeight - 2.0, rect.Width * Math.Min(node.Weight, 1.0f), 2.0));
                    }

                    DrawText(context, node.Title, 11.0, textBrush, new Point(rect.X + 8.0, rect.Y + 4.0), NodeWidth - 30.0);

                    if (node.Status.Length > 0)
                    {
                        DrawText(context, node.Status, 10.0, accent, new Point(rect.X + 8.0, rect.Bottom - RowHeight - 3.0), NodeWidth - 16.0);
                    }

                    if (node.Title != node.Kind)
                    {
                        DrawText(context, node.Kind, 9.0, mutedBrush, new Point(rect.Right - 8.0 - MeasureText(node.Kind, 9.0), rect.Y + 6.0), 60.0);
                    }

                    context.DrawEllipse(node.IsOutput ? outputPen.Brush : mutedBrush, null, OutputPin(node), PinRadius, PinRadius);

                    for (int slot = 0; slot < NumRows(node); ++slot)
                    {
                        Point pin = InputPin(node, slot);

                        if (slot < node.Inputs.Count)
                        {
                            bool isConnected = node.Inputs[slot].Source != AnimationGraphNodeView.NoInput;

                            context.DrawEllipse(isConnected ? mutedBrush : null, new Pen(mutedBrush, 1.0), pin, PinRadius, PinRadius);
                            DrawText(context, node.Inputs[slot].Name, 10.0, mutedBrush, new Point(pin.X + 10.0, pin.Y - 7.0), NodeWidth - 20.0);
                        }
                        else if (node.CanAddInputs)
                        {
                            context.DrawEllipse(null, new Pen(mutedBrush, 1.0) { DashStyle = DashStyle.Dash }, pin, PinRadius, PinRadius);
                            DrawText(context, "+", 10.0, mutedBrush, new Point(pin.X + 10.0, pin.Y - 7.0), 20.0);
                        }
                    }
                }

                if (_dragMode == DragMode.Wire && FindNode(_dragNode) is { } wireSource)
                {
                    DrawWire(context, activeWirePen, OutputPin(wireSource), ToWorld(_pointer));
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
                ShowContextMenu(world);
                e.Handled = true;

                return;
            }
            else if (pointer.Properties.IsLeftButtonPressed)
            {
                if (HitOutputPin(world) is { } outputNode)
                {
                    _dragMode = DragMode.Wire;
                    _dragNode = outputNode.Index;
                }
                else if (HitInputPin(world) is { } input && input.Slot < input.Node.Inputs.Count && input.Node.Inputs[input.Slot].Source != AnimationGraphNodeView.NoInput)
                {
                    _dragMode = DragMode.Wire;
                    _dragNode = input.Node.Inputs[input.Slot].Source;
                    _viewModel.Disconnect(input.Node.Index, input.Slot);
                }
                else if (HitNode(world) is { } node)
                {
                    _dragMode = DragMode.MoveNode;
                    _dragNode = node.Index;
                    _dragNodeStart = node.Position;

                    _viewModel.Select(node.Index);
                }
                else
                {
                    _dragMode = DragMode.Pan;

                    _viewModel.Select(AnimationGraphNodeView.NoInput);
                }
            }

            e.Pointer.Capture(this);
            e.Handled = true;
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
                _pan += moved;
                InvalidateVisual();
                break;
            case DragMode.MoveNode:
                if (FindNode(_dragNode) is { } node)
                {
                    node.Position = _dragNodeStart + (position - _dragStart) / _zoom;
                    InvalidateVisual();
                }

                break;
            case DragMode.Wire:
                InvalidateVisual();
                break;
            }
        }

        protected override void OnPointerReleased(PointerReleasedEventArgs e)
        {
            base.OnPointerReleased(e);

            if (_viewModel != null)
            {
                Point world = ToWorld(e.GetPosition(this));

                if (_dragMode == DragMode.MoveNode && _dragMoved && FindNode(_dragNode) is { } node)
                {
                    _viewModel.MoveNode(node.Index, node.Position);
                }
                else if (_dragMode == DragMode.Wire && HitInputPin(world) is { } input && input.Node.Index != _dragNode)
                {
                    _viewModel.Connect(_dragNode, input.Node.Index, input.Slot);
                }
            }

            _dragMode = DragMode.None;
            _dragNode = AnimationGraphNodeView.NoInput;

            e.Pointer.Capture(null);

            InvalidateVisual();
        }

        protected override void OnPointerWheelChanged(PointerWheelEventArgs e)
        {
            base.OnPointerWheelChanged(e);

            Point position = e.GetPosition(this);
            Point world = ToWorld(position);

            _zoom = Math.Clamp(_zoom * (e.Delta.Y > 0 ? 1.1 : 1.0 / 1.1), MinZoom, MaxZoom);

            _pan = new Vector(position.X - world.X * _zoom, position.Y - world.Y * _zoom);

            InvalidateVisual();
            e.Handled = true;
        }

        protected override void OnKeyDown(KeyEventArgs e)
        {
            base.OnKeyDown(e);

            if (_viewModel != null && e.Key == Key.Delete && _viewModel.SelectedNode >= 0)
            {
                _viewModel.RemoveNode(_viewModel.SelectedNode);
                e.Handled = true;
            }
        }

        private void ShowContextMenu(Point world)
        {
            if (_viewModel == null)
            {
                return;
            }

            AnimationGraphDocumentViewModel viewModel = _viewModel;
            ContextMenu menu = new ContextMenu();

            if (HitNode(world) is { } node)
            {
                MenuItem output = new MenuItem { Header = "Set as Output", IsEnabled = !node.IsOutput };
                output.Click += (_, _) => viewModel.SetOutput(node.Index);

                MenuItem delete = new MenuItem { Header = "Delete" };
                delete.Click += (_, _) => viewModel.RemoveNode(node.Index);

                menu.Items.Add(output);
                menu.Items.Add(delete);
            }
            else
            {
                foreach ((string className, string label) in viewModel.NodeKinds)
                {
                    MenuItem add = new MenuItem { Header = $"Add {label}" };
                    add.Click += (_, _) => viewModel.AddNode(className, world);

                    menu.Items.Add(add);
                }
            }

            menu.Open(this);
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
            e.DragEffects = TryGetDraggedClip(e, out _) ? DragDropEffects.Copy : DragDropEffects.None;
            e.Handled = true;
        }

        private void OnDrop(object? sender, DragEventArgs e)
        {
            if (_viewModel != null && TryGetDraggedClip(e, out string clipName))
            {
                _viewModel.AddClipNode(clipName, ToWorld(e.GetPosition(this)));
                e.Handled = true;
            }
        }
    }
}
