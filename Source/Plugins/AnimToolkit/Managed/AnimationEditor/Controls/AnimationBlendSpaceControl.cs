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

    public sealed class AnimationBlendSpaceControl : Control
    {
        private const double Margin = 28.0;
        private const double SampleRadius = 5.0;

        private static readonly Typeface LabelTypeface = new Typeface(FontFamily.Default);

        private AnimationGraphDocumentViewModel? _viewModel;

        private Rect _range = new Rect(0.0, 0.0, 1.0, 1.0);
        private int _dragSample = -1;
        private Vector _dragSamplePosition;
        private bool _isDraggingSample;
        private bool _isDraggingCursor;

        public AnimationBlendSpaceControl()
        {
            ClipToBounds = true;
        }

        protected override void OnDataContextChanged(EventArgs e)
        {
            base.OnDataContextChanged(e);

            if (_viewModel != null)
            {
                _viewModel.GraphChanged -= InvalidateVisual;
                _viewModel.PreviewChanged -= InvalidateVisual;
                _viewModel.PropertyChanged -= OnViewModelPropertyChanged;
            }

            _viewModel = DataContext as AnimationGraphDocumentViewModel;

            if (_viewModel != null)
            {
                _viewModel.GraphChanged += InvalidateVisual;
                _viewModel.PreviewChanged += InvalidateVisual;
                _viewModel.PropertyChanged += OnViewModelPropertyChanged;
            }

            InvalidateVisual();
        }

        private void OnViewModelPropertyChanged(object? sender, System.ComponentModel.PropertyChangedEventArgs e)
        {
            if (e.PropertyName == nameof(AnimationGraphDocumentViewModel.BlendSamples))
            {
                _dragSample = -1;
                InvalidateVisual();
            }
        }

        private IBrush Resource(string key, Color fallback)
        {
            return this.TryFindResource(key, ActualThemeVariant, out object? resource) && resource is IBrush brush
                ? brush
                : new SolidColorBrush(fallback);
        }

        private bool Is2D => _viewModel?.IsBlend2D ?? false;

        private Rect PlotRect => new Rect(Margin, Margin * 0.5, Math.Max(Bounds.Width - Margin * 1.5, 1.0), Math.Max(Bounds.Height - Margin * 1.5, 1.0));

        private void UpdateRange()
        {
            if (_viewModel == null || _viewModel.BlendSamples.Count == 0)
            {
                _range = new Rect(0.0, 0.0, 1.0, 1.0);
                return;
            }

            double minX = double.MaxValue;
            double minY = double.MaxValue;
            double maxX = double.MinValue;
            double maxY = double.MinValue;

            foreach ((Vector position, string _) in _viewModel.BlendSamples)
            {
                minX = Math.Min(minX, position.X);
                minY = Math.Min(minY, position.Y);
                maxX = Math.Max(maxX, position.X);
                maxY = Math.Max(maxY, position.Y);
            }

            double padX = Math.Max((maxX - minX) * 0.15, 0.5);
            double padY = Math.Max((maxY - minY) * 0.15, 0.5);

            _range = new Rect(minX - padX, minY - padY, maxX - minX + padX * 2.0, maxY - minY + padY * 2.0);
        }

        private Point ToScreen(Vector value)
        {
            Rect plot = PlotRect;
            double x = plot.X + (value.X - _range.X) / _range.Width * plot.Width;

            if (!Is2D)
            {
                return new Point(x, plot.Center.Y);
            }

            return new Point(x, plot.Bottom - (value.Y - _range.Y) / _range.Height * plot.Height);
        }

        private Vector ToValue(Point screen)
        {
            Rect plot = PlotRect;
            double x = _range.X + (screen.X - plot.X) / plot.Width * _range.Width;

            if (!Is2D)
            {
                return new Vector(x, 0.0);
            }

            return new Vector(x, _range.Y + (plot.Bottom - screen.Y) / plot.Height * _range.Height);
        }

        private Vector SamplePosition(int sample)
        {
            if (sample == _dragSample)
            {
                return _dragSamplePosition;
            }

            return _viewModel!.BlendSamples[sample].Position;
        }

        public override void Render(DrawingContext context)
        {
            IBrush background = Resource("Brush.PanelAlt", Color.Parse("#FF0A0B0D"));
            IBrush textBrush = Resource("Brush.Text", Color.Parse("#FFE0E0E0"));
            IBrush mutedBrush = Resource("Brush.TextMuted", Color.Parse("#FF9A9AA0"));
            IBrush accent = Resource("Brush.Accent", Color.Parse("#FF3D7EFF"));

            Pen axisPen = new Pen(Resource("Brush.BorderLight", Color.Parse("#FF1A1C1F")), 1.0);
            Pen cursorPen = new Pen(new SolidColorBrush(Color.Parse("#FFFFB347")), 2.0);

            context.FillRectangle(background, new Rect(Bounds.Size));

            if (_viewModel == null || !_viewModel.IsBlendSelected)
            {
                return;
            }

            if (_dragSample < 0)
            {
                UpdateRange();
            }

            Rect plot = PlotRect;

            if (Is2D)
            {
                context.DrawRectangle(null, axisPen, plot);
            }
            else
            {
                context.DrawLine(axisPen, new Point(plot.X, plot.Center.Y), new Point(plot.Right, plot.Center.Y));
            }

            DrawText(context, _range.X.ToString("0.##", CultureInfo.InvariantCulture), mutedBrush, new Point(plot.X, plot.Bottom + 2.0));
            DrawText(context, _range.Right.ToString("0.##", CultureInfo.InvariantCulture), mutedBrush, new Point(plot.Right - 24.0, plot.Bottom + 2.0));

            if (Is2D)
            {
                DrawText(context, _range.Bottom.ToString("0.##", CultureInfo.InvariantCulture), mutedBrush, new Point(2.0, plot.Y));
                DrawText(context, _range.Y.ToString("0.##", CultureInfo.InvariantCulture), mutedBrush, new Point(2.0, plot.Bottom - 12.0));
            }

            DrawText(context, _viewModel.BlendParameter.Length > 0 ? _viewModel.BlendParameter : "(no parameter)", mutedBrush, new Point(plot.X + plot.Width * 0.5 - 30.0, plot.Bottom + 2.0));

            for (int sample = 0; sample < _viewModel.BlendSamples.Count; ++sample)
            {
                Point point = ToScreen(SamplePosition(sample));

                context.DrawEllipse(sample == _dragSample ? accent : textBrush, null, point, SampleRadius, SampleRadius);
                DrawText(context, _viewModel.BlendSamples[sample].Label, textBrush, new Point(point.X + 8.0, point.Y - 16.0));
            }

            Point cursor = ToScreen(_viewModel.BlendCursor);

            context.DrawLine(cursorPen, new Point(cursor.X - 6.0, cursor.Y), new Point(cursor.X + 6.0, cursor.Y));
            context.DrawLine(cursorPen, new Point(cursor.X, cursor.Y - 6.0), new Point(cursor.X, cursor.Y + 6.0));
        }

        private static void DrawText(DrawingContext context, string text, IBrush brush, Point origin)
        {
            context.DrawText(new FormattedText(text, CultureInfo.InvariantCulture, FlowDirection.LeftToRight, LabelTypeface, 10.0, brush), origin);
        }

        private int HitSample(Point screen)
        {
            if (_viewModel == null)
            {
                return -1;
            }

            for (int sample = _viewModel.BlendSamples.Count - 1; sample >= 0; --sample)
            {
                Point point = ToScreen(_viewModel.BlendSamples[sample].Position);

                if (Math.Abs(point.X - screen.X) <= SampleRadius + 3.0 && Math.Abs(point.Y - screen.Y) <= SampleRadius + 3.0)
                {
                    return sample;
                }
            }

            return -1;
        }

        protected override void OnPointerPressed(PointerPressedEventArgs e)
        {
            base.OnPointerPressed(e);

            if (_viewModel == null || !_viewModel.IsBlendSelected || !e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
            {
                return;
            }

            Point position = e.GetPosition(this);

            _dragSample = HitSample(position);

            if (_dragSample >= 0)
            {
                _dragSamplePosition = _viewModel.BlendSamples[_dragSample].Position;
                _isDraggingSample = true;
            }
            else
            {
                _isDraggingCursor = true;
                _viewModel.SetBlendCursor(ToValue(position));
            }

            e.Pointer.Capture(this);
            e.Handled = true;

            InvalidateVisual();
        }

        protected override void OnPointerMoved(PointerEventArgs e)
        {
            base.OnPointerMoved(e);

            if (_viewModel == null)
            {
                return;
            }

            Vector value = ToValue(e.GetPosition(this));

            if (_isDraggingSample)
            {
                _dragSamplePosition = Is2D ? value : new Vector(value.X, _dragSamplePosition.Y);
                InvalidateVisual();
            }
            else if (_isDraggingCursor)
            {
                _viewModel.SetBlendCursor(value);
                InvalidateVisual();
            }
        }

        protected override void OnPointerReleased(PointerReleasedEventArgs e)
        {
            base.OnPointerReleased(e);

            if (_viewModel != null && _isDraggingSample)
            {
                if (_dragSamplePosition != _viewModel.BlendSamples[_dragSample].Position)
                {
                    _viewModel.MoveBlendSample(_dragSample, _dragSamplePosition);
                }
                else
                {
                    _dragSample = -1;
                }
            }

            _isDraggingSample = false;
            _isDraggingCursor = false;

            e.Pointer.Capture(null);
        }
    }
}
