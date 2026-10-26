using System;
using System.Globalization;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Hyperion.Editor.ViewModels;

namespace Hyperion.Editor.Controls
{
    using Color = Avalonia.Media.Color;

    public sealed class AnimationTimelineControl : Control
    {
        private const double RulerHeight = 18.0;
        private const double MarkerHalfWidth = 5.0;
        private const double SidePadding = 8.0;
        private const double DragThreshold = 3.0;

        private static readonly Typeface LabelTypeface = new Typeface(FontFamily.Default);

        private AnimationTimelineViewModel? _viewModel;

        private bool _isScrubbing;
        private int _draggedNotify = -1;
        private Point _dragStart;
        private double _dragTime;
        private bool _isDraggingNotify;

        public AnimationTimelineControl()
        {
            ClipToBounds = true;
            Focusable = true;
        }

        protected override void OnDataContextChanged(EventArgs e)
        {
            base.OnDataContextChanged(e);

            if (_viewModel != null)
            {
                _viewModel.PropertyChanged -= OnViewModelPropertyChanged;
                _viewModel.NotifiesChanged -= InvalidateVisual;
            }

            _viewModel = DataContext as AnimationTimelineViewModel;

            if (_viewModel != null)
            {
                _viewModel.PropertyChanged += OnViewModelPropertyChanged;
                _viewModel.NotifiesChanged += InvalidateVisual;
            }

            InvalidateVisual();
        }

        private void OnViewModelPropertyChanged(object? sender, System.ComponentModel.PropertyChangedEventArgs e)
        {
            if (e.PropertyName is nameof(AnimationTimelineViewModel.Time) or nameof(AnimationTimelineViewModel.Length))
            {
                InvalidateVisual();
            }
        }

        private IBrush Resource(string key, Color fallback)
        {
            return this.TryFindResource(key, ActualThemeVariant, out object? resource) && resource is IBrush brush
                ? brush
                : new SolidColorBrush(fallback);
        }

        private double TrackWidth => Math.Max(Bounds.Width - SidePadding * 2.0, 1.0);

        private double TimeToX(double time, double length) => SidePadding + (length > 0.0 ? time / length : 0.0) * TrackWidth;

        private double XToTime(double x, double length) => Math.Clamp((x - SidePadding) / TrackWidth, 0.0, 1.0) * length;

        private int HitTestNotify(Point point)
        {
            if (_viewModel == null || point.Y < RulerHeight)
            {
                return -1;
            }

            double length = _viewModel.Length;

            for (int index = _viewModel.Notifies.Count - 1; index >= 0; --index)
            {
                if (Math.Abs(TimeToX(_viewModel.Notifies[index].Time, length) - point.X) <= MarkerHalfWidth + 2.0)
                {
                    return _viewModel.Notifies[index].Index;
                }
            }

            return -1;
        }

        public override void Render(DrawingContext context)
        {
            IBrush background = Resource("Brush.PanelAlt", Color.Parse("#FF232326"));
            IBrush rulerBackground = Resource("Brush.Panel", Color.Parse("#FF1C1C1F"));
            IBrush textBrush = Resource("Brush.TextMuted", Color.Parse("#FF9A9AA0"));
            IBrush accent = Resource("Brush.Accent", Color.Parse("#FF4A9EFF"));
            IBrush markerBrush = new SolidColorBrush(Color.Parse("#FFFFB347"));
            Pen tickPen = new Pen(Resource("Brush.BorderLight", Color.Parse("#FF3A3A40")), 1.0);

            Rect bounds = new Rect(Bounds.Size);

            context.FillRectangle(background, bounds);
            context.FillRectangle(rulerBackground, new Rect(0.0, 0.0, bounds.Width, RulerHeight));

            if (_viewModel == null || _viewModel.Length <= 0.0f)
            {
                return;
            }

            double length = _viewModel.Length;
            double frameRate = Math.Max(_viewModel.FrameRate, 1.0);
            int numFrames = (int)Math.Ceiling(length * frameRate);

            // frame ticks thin out until they're at least 6px apart; every 5th tick gets a label
            int frameStep = 1;

            while (frameStep < numFrames && TrackWidth / (numFrames / (double)frameStep) < 6.0)
            {
                frameStep = frameStep switch { 1 => 2, 2 => 5, _ => frameStep * 2 };
            }

            for (int frame = 0; frame <= numFrames; frame += frameStep)
            {
                double x = TimeToX(Math.Min(frame / frameRate, length), length);
                bool isMajor = (frame / frameStep) % 5 == 0;

                context.DrawLine(tickPen, new Point(x, isMajor ? 4.0 : RulerHeight - 5.0), new Point(x, RulerHeight));

                if (isMajor)
                {
                    FormattedText label = new FormattedText(frame.ToString(CultureInfo.InvariantCulture), CultureInfo.InvariantCulture, FlowDirection.LeftToRight, LabelTypeface, 9.0, textBrush);
                    context.DrawText(label, new Point(x + 2.0, 1.0));
                }
            }

            double markerY = RulerHeight + (bounds.Height - RulerHeight) * 0.5;

            foreach (AnimationNotifyMarker marker in _viewModel.Notifies)
            {
                double time = marker.Index == _draggedNotify && _isDraggingNotify ? _dragTime : marker.Time;
                double x = TimeToX(time, length);

                if (marker.Duration > 0.0f)
                {
                    double endX = TimeToX(Math.Min(time + marker.Duration, length), length);
                    context.FillRectangle(new SolidColorBrush(Color.Parse("#55FFB347")), new Rect(x, markerY - 4.0, Math.Max(endX - x, 1.0), 8.0));
                }

                StreamGeometry diamond = new StreamGeometry();

                using (StreamGeometryContext geometry = diamond.Open())
                {
                    geometry.BeginFigure(new Point(x, markerY - MarkerHalfWidth), true);
                    geometry.LineTo(new Point(x + MarkerHalfWidth, markerY));
                    geometry.LineTo(new Point(x, markerY + MarkerHalfWidth));
                    geometry.LineTo(new Point(x - MarkerHalfWidth, markerY));
                    geometry.EndFigure(true);
                }

                context.DrawGeometry(markerBrush, null, diamond);

                FormattedText name = new FormattedText(marker.Name, CultureInfo.InvariantCulture, FlowDirection.LeftToRight, LabelTypeface, 10.0, textBrush);
                context.DrawText(name, new Point(x + MarkerHalfWidth + 2.0, markerY - name.Height * 0.5));
            }

            double playheadX = TimeToX(_viewModel.Time, length);
            context.DrawLine(new Pen(accent, 2.0), new Point(playheadX, 0.0), new Point(playheadX, bounds.Height));
        }

        protected override void OnPointerPressed(PointerPressedEventArgs e)
        {
            base.OnPointerPressed(e);

            if (_viewModel == null || !_viewModel.HasClip)
            {
                return;
            }

            PointerPoint pointer = e.GetCurrentPoint(this);
            Point position = pointer.Position;

            if (pointer.Properties.IsRightButtonPressed)
            {
                ShowContextMenu(position, HitTestNotify(position));
                e.Handled = true;

                return;
            }

            if (!pointer.Properties.IsLeftButtonPressed)
            {
                return;
            }

            int notifyIndex = HitTestNotify(position);

            if (notifyIndex >= 0)
            {
                if (e.ClickCount >= 2)
                {
                    ShowRenameFlyout(notifyIndex, position);
                    e.Handled = true;

                    return;
                }

                _draggedNotify = notifyIndex;
                _dragStart = position;
                _dragTime = _viewModel.Notifies.Find(marker => marker.Index == notifyIndex).Time;
                _isDraggingNotify = false;
            }
            else
            {
                _isScrubbing = true;
                _viewModel.Scrub((float)XToTime(position.X, _viewModel.Length));
            }

            e.Pointer.Capture(this);
            e.Handled = true;
        }

        protected override void OnPointerMoved(PointerEventArgs e)
        {
            base.OnPointerMoved(e);

            if (_viewModel == null)
            {
                return;
            }

            Point position = e.GetPosition(this);

            if (_isScrubbing)
            {
                _viewModel.Scrub((float)XToTime(position.X, _viewModel.Length));
            }
            else if (_draggedNotify >= 0)
            {
                _isDraggingNotify |= Math.Abs(position.X - _dragStart.X) > DragThreshold;

                if (_isDraggingNotify)
                {
                    _dragTime = XToTime(position.X, _viewModel.Length);
                    InvalidateVisual();
                }
            }
        }

        protected override void OnPointerReleased(PointerReleasedEventArgs e)
        {
            base.OnPointerReleased(e);

            if (_viewModel != null && _draggedNotify >= 0)
            {
                if (_isDraggingNotify)
                {
                    _viewModel.MoveNotify(_draggedNotify, (float)_dragTime);
                }
                else
                {
                    _viewModel.Scrub(_viewModel.Notifies.Find(marker => marker.Index == _draggedNotify).Time);
                }
            }

            _isScrubbing = false;
            _isDraggingNotify = false;
            _draggedNotify = -1;

            e.Pointer.Capture(null);
        }

        private void ShowContextMenu(Point position, int notifyIndex)
        {
            if (_viewModel == null)
            {
                return;
            }

            AnimationTimelineViewModel viewModel = _viewModel;
            float time = (float)XToTime(position.X, viewModel.Length);

            ContextMenu menu = new ContextMenu();

            if (notifyIndex >= 0)
            {
                MenuItem rename = new MenuItem { Header = "Rename Notify" };
                rename.Click += (_, _) => ShowRenameFlyout(notifyIndex, position);

                MenuItem remove = new MenuItem { Header = "Remove Notify" };
                remove.Click += (_, _) => viewModel.RemoveNotify(notifyIndex);

                menu.Items.Add(rename);
                menu.Items.Add(remove);
            }
            else
            {
                MenuItem add = new MenuItem { Header = "Add Notify" };
                add.Click += (_, _) => viewModel.AddNotify(time);

                menu.Items.Add(add);
            }

            menu.Open(this);
        }

        private void ShowRenameFlyout(int notifyIndex, Point position)
        {
            if (_viewModel == null)
            {
                return;
            }

            AnimationTimelineViewModel viewModel = _viewModel;

            TextBox nameBox = new TextBox
            {
                Text = viewModel.Notifies.Find(marker => marker.Index == notifyIndex).Name,
                MinWidth = 140
            };

            Flyout flyout = new Flyout { Content = nameBox, Placement = PlacementMode.Pointer };

            nameBox.KeyDown += (_, keyEvent) =>
            {
                if (keyEvent.Key == Key.Enter)
                {
                    viewModel.RenameNotify(notifyIndex, nameBox.Text ?? string.Empty);
                    flyout.Hide();
                }
                else if (keyEvent.Key == Key.Escape)
                {
                    flyout.Hide();
                }
            };

            flyout.Opened += (_, _) =>
            {
                nameBox.Focus();
                nameBox.SelectAll();
            };

            flyout.ShowAt(this, true);
        }
    }
}
