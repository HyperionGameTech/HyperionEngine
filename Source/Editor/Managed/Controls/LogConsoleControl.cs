using System;
using System.Collections.ObjectModel;
using System.Collections.Specialized;
using System.Globalization;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Media.Immutable;
using Avalonia.Threading;
using Hyperion.Editor.Commands;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.Controls
{
    using Color = Avalonia.Media.Color;

    /// New control for the engines's console to reduce the huge amount of memory allocations that come with linking our
    /// logger system up through XAML.
    /// This control uses a huge amount of caching to try to reduce the amount of work done during rendering and to avoid per-row allocations as much as possible.
    public sealed class LogConsoleControl : Control, ILogicalScrollable
    {
        private static readonly IBrush[] LevelBrushes =
        {
            new ImmutableSolidColorBrush(Color.Parse("#FF7A2E33")), // Fatal
            new ImmutableSolidColorBrush(Color.Parse("#FFE0555C")), // Error
            new ImmutableSolidColorBrush(Color.Parse("#FFE3A84D")), // Warning
            new ImmutableSolidColorBrush(Color.Parse("#FFE7E9ED")), // Info
            new ImmutableSolidColorBrush(Color.Parse("#FF888E97")), // Verbose
            new ImmutableSolidColorBrush(Color.Parse("#FF888E97")), // Debug
        };

        private static readonly IBrush LinkBrush = new ImmutableSolidColorBrush(Color.Parse("#FF6DA0FF"));
        private static readonly IBrush BackgroundBrush = new ImmutableSolidColorBrush(Color.Parse("#FF111215"));
        private static readonly Typeface MonoTypeface = new Typeface(new FontFamily("Cascadia Mono, Menlo, Monaco, SFMono-Regular, Consolas, Courier New, monospace"));
        private static readonly Cursor HandCursor = new Cursor(StandardCursorType.Hand);

        // @TODO make configurable through EditorConfig
        private const double FontSize = 10.0;
        private const double FilePrefixMargin = 4.0;
        private const double RowPadding = 2.0;

        private ReadOnlyObservableCollection<LogEntry>? _items;
        private INotifyCollectionChanged? _itemsChangedSource;

        private double[] _rowTopY = new double[256];
        private double[] _rowHeights = new double[256];
        private double[] _prefixWidths = new double[256];
        private FormattedText?[] _prefixFormattedText = new FormattedText?[256];
        private FormattedText?[] _messageFormattedText = new FormattedText?[256];
        private int _rowCount;
        private double _totalContentHeight;
        private double _lastRenderWidth = -1;

        private Vector _scrollOffset;
        private bool _isScrolledToBottom = true;

        public event EventHandler? ScrollInvalidated;

        public bool IsLogicalScrollEnabled => true;
        public Size ScrollSize => new Size(1, FontSize + RowPadding);
        public Size PageScrollSize => new Size(1, Math.Max(1, Bounds.Height - FontSize));
        public Size Extent => new Size(Math.Max(Bounds.Width, 1), Math.Max(_totalContentHeight, 1));
        public Size Viewport => new Size(Math.Max(Bounds.Width, 1), Math.Max(Bounds.Height, 1));

        public Vector Offset
        {
            get => _scrollOffset;
            set
            {
                var clamped = new Vector(0, Math.Clamp(value.Y, 0, Math.Max(0, _totalContentHeight - Bounds.Height)));
                if (_scrollOffset == clamped) return;
                _scrollOffset = clamped;
                _isScrolledToBottom = _scrollOffset.Y >= _totalContentHeight - Bounds.Height - 1;
                InvalidateVisual();
            }
        }

        public void RaiseScrollInvalidated(EventArgs e) => ScrollInvalidated?.Invoke(this, e);
        public bool BringIntoView(Control target, Rect targetRect) => false;
        public Control? GetControlInDirection(NavigationDirection direction, Control? from) => null;
        public bool CanHorizontallyScroll { get; set; }
        public bool CanVerticallyScroll { get; set; }

        public static readonly StyledProperty<ReadOnlyObservableCollection<LogEntry>?> ItemsSourceProperty =
            AvaloniaProperty.Register<LogConsoleControl, ReadOnlyObservableCollection<LogEntry>?>(nameof(ItemsSource));

        public ReadOnlyObservableCollection<LogEntry>? ItemsSource
        {
            get => GetValue(ItemsSourceProperty);
            set => SetValue(ItemsSourceProperty, value);
        }

        static LogConsoleControl()
        {
            ItemsSourceProperty.Changed.AddClassHandler<LogConsoleControl>((x, e) => x.OnItemsSourceChanged(e));
        }

        private void OnItemsSourceChanged(AvaloniaPropertyChangedEventArgs e)
        {
            if (_itemsChangedSource != null)
                _itemsChangedSource.CollectionChanged -= OnCollectionChanged;

            _items = e.NewValue as ReadOnlyObservableCollection<LogEntry>;
            _itemsChangedSource = _items;

            if (_itemsChangedSource != null)
                _itemsChangedSource.CollectionChanged += OnCollectionChanged;

            ResetCache();
            InvalidateVisual();
        }

        private void OnCollectionChanged(object? sender, NotifyCollectionChangedEventArgs e)
        {
            if (e.Action == NotifyCollectionChangedAction.Add && e.NewItems != null)
            {
                int startIndex = e.NewStartingIndex;
                int addCount = e.NewItems.Count;

                EnsureCapacity(startIndex + addCount);

                if (_lastRenderWidth > 0)
                {
                    for (int i = startIndex; i < startIndex + addCount; i++)
                    {
                        double prefixWidth = MeasurePrefixWidth(i);
                        double rowHeight = MeasureRowHeight(i, _lastRenderWidth, prefixWidth);
                        _prefixWidths[i] = prefixWidth;
                        _rowTopY[i] = _totalContentHeight;
                        _rowHeights[i] = rowHeight;
                        _totalContentHeight += rowHeight;
                    }
                }

                _rowCount = startIndex + addCount;

                if (_isScrolledToBottom)
                    _scrollOffset = new Vector(0, Math.Max(0, _totalContentHeight - Bounds.Height));

                RaiseScrollInvalidated(EventArgs.Empty);
            }
            else
            {
                ResetCache();
                _scrollOffset = Vector.Zero;
                _isScrolledToBottom = true;
                RaiseScrollInvalidated(EventArgs.Empty);
            }

            InvalidateVisual();
        }

        protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e)
        {
            base.OnDetachedFromVisualTree(e);

            if (_itemsChangedSource != null)
            {
                _itemsChangedSource.CollectionChanged -= OnCollectionChanged;
                _itemsChangedSource = null;
            }
        }

        protected override Size MeasureOverride(Size availableSize)
        {
            double w = double.IsInfinity(availableSize.Width) ? 800 : availableSize.Width;
            double h = double.IsInfinity(availableSize.Height) ? 600 : availableSize.Height;
            return new Size(w, h);
        }

        protected override Size ArrangeOverride(Size finalSize)
        {
            if (finalSize.Width > 0 && Math.Abs(finalSize.Width - _lastRenderWidth) > 0.5)
            {
                _lastRenderWidth = finalSize.Width;
                RebuildRowCache(finalSize.Width);
            }

            return finalSize;
        }

        public override void Render(DrawingContext context)
        {
            context.FillRectangle(BackgroundBrush, new Rect(Bounds.Size));

            var items = _items;
            if (items == null || items.Count == 0) return;

            double viewWidth = Bounds.Width;
            double viewHeight = Bounds.Height;

            if (_lastRenderWidth <= 0 || Math.Abs(_lastRenderWidth - viewWidth) > 0.5 || _rowCount != items.Count)
            {
                _lastRenderWidth = viewWidth;
                RebuildRowCache(viewWidth, deferScrollNotify: true);
            }

            if (_rowCount == 0) return;

            int firstRow = FindFirstVisibleRow(_scrollOffset.Y);
            double y = _rowTopY[firstRow] - _scrollOffset.Y;

            for (int i = firstRow; i < _rowCount && y < viewHeight; i++)
            {
                DrawRow(context, items[i], i, y);
                y += _rowHeights[i];
            }
        }

        private void DrawRow(DrawingContext ctx, in LogEntry entry, int index, double y)
        {
            double x = 0;

            if (entry.HasFileLocation && _prefixFormattedText[index] is { } prefixFt)
            {
                ctx.DrawText(prefixFt, new Point(x, y));
                x += _prefixWidths[index] + FilePrefixMargin;
            }

            if (_messageFormattedText[index] is { } messageFt)
                ctx.DrawText(messageFt, new Point(x, y));
        }

        protected override void OnPointerPressed(PointerPressedEventArgs e)
        {
            base.OnPointerPressed(e);

            if (_items == null || _rowCount == 0) return;

            var pos = e.GetPosition(this);
            int index = FindFirstVisibleRow(_scrollOffset.Y + pos.Y);

            if ((uint)index < (uint)_items.Count && _items[index].HasFileLocation && pos.X <= _prefixWidths[index])
            {
                NavigateToFileCommand.DefaultInstance.Execute(_items[index]);
                e.Handled = true;
            }
        }

        protected override void OnPointerMoved(PointerEventArgs e)
        {
            base.OnPointerMoved(e);

            if (_items == null || _rowCount == 0)
            {
                Cursor = Cursor.Default;
                return;
            }

            var pos = e.GetPosition(this);
            int index = FindFirstVisibleRow(_scrollOffset.Y + pos.Y);

            bool overLink = (uint)index < (uint)_items.Count
                && _items[index].HasFileLocation
                && pos.X <= _prefixWidths[index];

            Cursor = overLink ? HandCursor : Cursor.Default;
        }

        private double MeasurePrefixWidth(int index)
        {
            if (_items == null || index >= _items.Count) return 0;

            var entry = _items[index];
            if (!entry.HasFileLocation) return 0;

            var ft = _prefixFormattedText[index] ??= new FormattedText(
                entry.FileLocationText,
                CultureInfo.CurrentCulture,
                FlowDirection.LeftToRight,
                MonoTypeface,
                FontSize,
                LinkBrush);

            return ft.Width;
        }

        private double MeasureRowHeight(int index, double availableWidth, double prefixWidth)
        {
            if (_items == null || index >= _items.Count) return 0;

            var entry = _items[index];
            double messageWidth = Math.Max(1, availableWidth - (prefixWidth > 0 ? prefixWidth + FilePrefixMargin : 0));

            var ft = _messageFormattedText[index];
            if (ft == null)
            {
                ft = new FormattedText(
                    entry.Message.Length > 0 ? entry.Message : " ",
                    CultureInfo.CurrentCulture,
                    FlowDirection.LeftToRight,
                    MonoTypeface,
                    FontSize,
                    GetBrushForLevel(entry.Level));
                ft.MaxTextWidth = messageWidth;
                _messageFormattedText[index] = ft;
            }

            return Math.Max(ft.Height, FontSize) + RowPadding;
        }

        private void RebuildRowCache(double width, bool deferScrollNotify = false)
        {
            var items = _items;
            if (items == null)
            {
                _totalContentHeight = 0;
                _rowCount = 0;
                NotifyScrollInvalidated(deferScrollNotify);
                return;
            }

            int count = items.Count;
            EnsureCapacity(count);

            int clearCount = Math.Min(_rowCount, _messageFormattedText.Length);
            if (clearCount > 0)
                Array.Clear(_messageFormattedText, 0, clearCount);

            _totalContentHeight = 0;
            for (int i = 0; i < count; i++)
            {
                double prefixWidth = MeasurePrefixWidth(i);
                double rowHeight = MeasureRowHeight(i, width, prefixWidth);
                _prefixWidths[i] = prefixWidth;
                _rowTopY[i] = _totalContentHeight;
                _rowHeights[i] = rowHeight;
                _totalContentHeight += rowHeight;
            }

            _rowCount = count;

            double maxOffset = Math.Max(0, _totalContentHeight - Bounds.Height);
            _scrollOffset = _isScrolledToBottom
                ? new Vector(0, maxOffset)
                : new Vector(0, Math.Clamp(_scrollOffset.Y, 0, maxOffset));

            NotifyScrollInvalidated(deferScrollNotify);
        }

        private void NotifyScrollInvalidated(bool deferred)
        {
            if (deferred)
                Dispatcher.UIThread.Post(() => RaiseScrollInvalidated(EventArgs.Empty), DispatcherPriority.Normal);
            else
                RaiseScrollInvalidated(EventArgs.Empty);
        }

        private int FindFirstVisibleRow(double targetY)
        {
            if (_rowCount == 0) return 0;

            int lo = 0, hi = _rowCount - 1;
            while (lo < hi)
            {
                int mid = (lo + hi + 1) >> 1;
                if (_rowTopY[mid] <= targetY)
                    lo = mid;
                else
                    hi = mid - 1;
            }

            return lo;
        }

        private static IBrush GetBrushForLevel(LogLevel level)
        {
            int index = (int)level;
            return (uint)index < (uint)LevelBrushes.Length ? LevelBrushes[index] : Brushes.White;
        }

        private void EnsureCapacity(int needed)
        {
            if (needed <= _rowHeights.Length) return;

            int newSize = Math.Max(needed, _rowHeights.Length * 2);
            Array.Resize(ref _rowTopY, newSize);
            Array.Resize(ref _rowHeights, newSize);
            Array.Resize(ref _prefixWidths, newSize);
            Array.Resize(ref _prefixFormattedText, newSize);
            Array.Resize(ref _messageFormattedText, newSize);
        }

        private void ResetCache()
        {
            int count = _rowCount;
            _rowCount = 0;
            _totalContentHeight = 0;
            _lastRenderWidth = -1;

            if (count > 0)
            {
                int n = Math.Min(count, _rowTopY.Length);
                Array.Clear(_rowTopY, 0, n);
                Array.Clear(_rowHeights, 0, n);
                Array.Clear(_prefixWidths, 0, n);
                Array.Clear(_prefixFormattedText, 0, Math.Min(count, _prefixFormattedText.Length));
                Array.Clear(_messageFormattedText, 0, Math.Min(count, _messageFormattedText.Length));
            }
        }
    }
}
