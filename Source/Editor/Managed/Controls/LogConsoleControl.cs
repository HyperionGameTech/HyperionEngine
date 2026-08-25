using System;
using System.Collections.ObjectModel;
using System.Collections.Specialized;
using System.Text;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Media.Immutable;
using Avalonia.Media.TextFormatting;
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
        private static readonly IBrush SelectionBrush = new ImmutableSolidColorBrush(Color.Parse("#336DA0FF"));
        private static readonly Typeface MonoTypeface = new Typeface(new FontFamily("Cascadia Mono, Menlo, Monaco, SFMono-Regular, Consolas, Courier New, monospace"));
        private static readonly Cursor HandCursor = new Cursor(StandardCursorType.Hand);
        private static readonly Cursor IbeamCursor = new Cursor(StandardCursorType.Ibeam);

        // @TODO make configurable through EditorConfig
        private const double FontSize = 10.0;
        private const double FilePrefixMargin = 4.0;
        private const double RowPadding = 2.0;

        private ReadOnlyObservableCollection<LogEntry>? _items;
        private INotifyCollectionChanged? _itemsChangedSource;

        private double[] _rowTopY = new double[256];
        private double[] _rowHeights = new double[256];
        private double[] _prefixWidths = new double[256];
        private TextLayout?[] _prefixLayout = new TextLayout?[256];
        private TextLayout?[] _messageLayout = new TextLayout?[256];
        private int _rowCount;
        private double _totalContentHeight;
        private double _lastRenderWidth = -1;

        private Vector _scrollOffset;
        private bool _isScrolledToBottom = true;

        private (int Row, int Col) _selectionAnchor;
        private (int Row, int Col) _selectionActive;
        private bool _isSelecting;
        private bool _hasDragged;
        private bool _pressOnLink;
        private Point _pressPosition;
        private readonly ContextMenu _contextMenu;

        private bool HasSelection => _selectionAnchor != _selectionActive && _rowCount > 0 && _items != null;

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

        public LogConsoleControl()
        {
            Focusable = true;

            var copyItem = new MenuItem { Header = "Copy", InputGesture = new KeyGesture(Key.C, KeyModifiers.Control) };
            copyItem.Click += (_, _) => CopySelection();

            var selectAllItem = new MenuItem { Header = "Select All", InputGesture = new KeyGesture(Key.A, KeyModifiers.Control) };
            selectAllItem.Click += (_, _) => SelectAll();

            _contextMenu = new ContextMenu();
            _contextMenu.ItemsSource = new object[] { copyItem, selectAllItem };
            _contextMenu.Opened += (_, _) =>
            {
                copyItem.IsEnabled = HasSelection;
                selectAllItem.IsEnabled = _rowCount > 0;
            };
            ContextMenu = _contextMenu;
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

            bool hasSelection = HasSelection;
            int minRow = 0, minCol = 0, maxRow = 0, maxCol = 0;
            if (hasSelection)
                GetNormalizedSelection(out minRow, out minCol, out maxRow, out maxCol);

            int firstRow = FindFirstVisibleRow(_scrollOffset.Y);
            double y = _rowTopY[firstRow] - _scrollOffset.Y;

            for (int i = firstRow; i < _rowCount && y < viewHeight; i++)
            {
                if (hasSelection)
                    DrawRowSelection(context, i, y, minRow, minCol, maxRow, maxCol);
                DrawRow(context, items[i], i, y);
                y += _rowHeights[i];
            }
        }

        private void DrawRow(DrawingContext ctx, in LogEntry entry, int index, double y)
        {
            double x = 0;

            if (entry.HasFileLocation && _prefixLayout[index] is { } prefixLayout)
            {
                prefixLayout.Draw(ctx, new Point(x, y));
                x += _prefixWidths[index] + FilePrefixMargin;
            }

            if (_messageLayout[index] is { } messageLayout)
                messageLayout.Draw(ctx, new Point(x, y));
        }

        private void DrawRowSelection(DrawingContext ctx, int index, double y, int minRow, int minCol, int maxRow, int maxCol)
        {
            if (_items == null)
                return;

            var (startCol, endCol) = GetSelectedRangeForRow(index, minRow, minCol, maxRow, maxCol);
            if (startCol >= endCol)
                return;

            var entry = _items[index];
            int prefixLen = entry.HasFileLocation ? entry.FileLocationText.Length : 0;
            int messageStartCol = entry.HasFileLocation ? prefixLen + 1 : 0;
            double messageX = entry.HasFileLocation ? _prefixWidths[index] + FilePrefixMargin : 0;

            if (prefixLen > 0 && startCol < prefixLen && _prefixLayout[index] is { } prefixLayout)
            {
                int pStart = Math.Clamp(startCol, 0, prefixLen);
                int pEnd = Math.Clamp(endCol, 0, prefixLen);
                if (pEnd > pStart)
                {
                    foreach (Rect rect in prefixLayout.HitTestTextRange(pStart, pEnd - pStart))
                        ctx.FillRectangle(SelectionBrush, new Rect(rect.X, rect.Y + y, rect.Width, rect.Height));
                }
            }

            if (_messageLayout[index] is { } messageLayout)
            {
                int mLen = Math.Max(1, entry.Message.Length);
                int mStart = Math.Clamp(startCol - messageStartCol, 0, mLen);
                int mEnd = Math.Clamp(endCol - messageStartCol, 0, mLen);
                if (mEnd > mStart)
                {
                    foreach (Rect rect in messageLayout.HitTestTextRange(mStart, mEnd - mStart))
                        ctx.FillRectangle(SelectionBrush, new Rect(rect.X + messageX, rect.Y + y, rect.Width, rect.Height));
                }
            }
        }

        private (int Row, int Col) HitTestPosition(Point pos)
        {
            if (_items == null || _rowCount == 0)
                return (0, 0);

            double contentY = pos.Y + _scrollOffset.Y;
            if (contentY < 0)
                return (0, 0);
            if (contentY >= _totalContentHeight)
                return (_rowCount - 1, GetRowTextLength(_items[_rowCount - 1]));

            int row = Math.Min(FindFirstVisibleRow(contentY), _rowCount - 1);
            double localY = contentY - _rowTopY[row];
            var entry = _items[row];

            if (entry.HasFileLocation)
            {
                int prefixLen = entry.FileLocationText.Length;

                if (_prefixLayout[row] is { } prefixLayout)
                {
                    if (pos.X <= _prefixWidths[row])
                    {
                        var p = new Point(pos.X, localY);
                        return (row, Math.Clamp(prefixLayout.HitTestPoint(ref p).TextPosition, 0, prefixLen));
                    }

                    if (_messageLayout[row] is { } messageLayout)
                    {
                        var p = new Point(pos.X - (_prefixWidths[row] + FilePrefixMargin), localY);
                        int mLen = Math.Max(1, entry.Message.Length);
                        return (row, prefixLen + 1 + Math.Clamp(messageLayout.HitTestPoint(ref p).TextPosition, 0, mLen));
                    }

                    return (row, prefixLen + 1);
                }

                return (row, pos.X <= _prefixWidths[row] / 2 ? 0 : prefixLen);
            }

            if (_messageLayout[row] is { } layout)
            {
                var p = new Point(pos.X, localY);
                int len = Math.Max(1, entry.Message.Length);
                return (row, Math.Clamp(layout.HitTestPoint(ref p).TextPosition, 0, len));
            }

            return (row, 0);
        }

        private void GetNormalizedSelection(out int minRow, out int minCol, out int maxRow, out int maxCol)
        {
            if (_selectionAnchor.Row < _selectionActive.Row || (_selectionAnchor.Row == _selectionActive.Row && _selectionAnchor.Col <= _selectionActive.Col))
            {
                minRow = _selectionAnchor.Row;
                minCol = _selectionAnchor.Col;
                maxRow = _selectionActive.Row;
                maxCol = _selectionActive.Col;
            }
            else
            {
                minRow = _selectionActive.Row;
                minCol = _selectionActive.Col;
                maxRow = _selectionAnchor.Row;
                maxCol = _selectionAnchor.Col;
            }
        }

        private (int Start, int End) GetSelectedRangeForRow(int row, int minRow, int minCol, int maxRow, int maxCol)
        {
            if (_items == null || row < minRow || row > maxRow)
                return (0, 0);

            int rowLen = GetRowTextLength(_items[row]);

            if (row == minRow && row == maxRow)
                return (Math.Min(minCol, rowLen), Math.Min(maxCol, rowLen));
            if (row == minRow)
                return (Math.Min(minCol, rowLen), rowLen);
            if (row == maxRow)
                return (0, Math.Min(maxCol, rowLen));
            return (0, rowLen);
        }

        private void SelectWordAt((int Row, int Col) hit)
        {
            if (_items == null)
                return;

            string text = GetRowText(_items[hit.Row]);
            int len = text.Length;
            if (len == 0)
            {
                _selectionAnchor = (hit.Row, 0);
                _selectionActive = (hit.Row, 0);
                return;
            }

            int col = Math.Clamp(hit.Col, 0, len - 1);
            bool targetIsWord = IsWordChar(text[col]);

            int start = col;
            int end = col + 1;
            while (start > 0 && IsWordChar(text[start - 1]) == targetIsWord)
                start--;
            while (end < len && IsWordChar(text[end]) == targetIsWord)
                end++;

            _selectionAnchor = (hit.Row, start);
            _selectionActive = (hit.Row, end);
        }

        private static bool IsWordChar(char c) => char.IsLetterOrDigit(c) || c == '_';

        private static string GetRowText(in LogEntry entry) =>
            entry.HasFileLocation ? entry.FileLocationText + " " + entry.Message : entry.Message;

        private static int GetRowTextLength(in LogEntry entry) =>
            entry.HasFileLocation ? entry.FileLocationText.Length + 1 + entry.Message.Length : entry.Message.Length;

        public void SelectAll()
        {
            if (_items == null || _rowCount == 0)
                return;

            _selectionAnchor = (0, 0);
            _selectionActive = (_rowCount - 1, GetRowTextLength(_items[_rowCount - 1]));
            InvalidateVisual();
        }

        private string BuildSelectedText()
        {
            if (_items == null)
                return string.Empty;

            GetNormalizedSelection(out int minRow, out int minCol, out int maxRow, out int maxCol);

            var sb = new StringBuilder(256);
            for (int row = minRow; row <= maxRow; row++)
            {
                var (start, end) = GetSelectedRangeForRow(row, minRow, minCol, maxRow, maxCol);
                if (end > start)
                {
                    string text = GetRowText(_items[row]);
                    int s = Math.Clamp(start, 0, text.Length);
                    int e = Math.Clamp(end, 0, text.Length);
                    if (e > s)
                        sb.Append(text, s, e - s);
                }

                if (row < maxRow)
                    sb.Append('\n');
            }

            return sb.ToString();
        }

        private async void CopySelection()
        {
            if (!HasSelection)
                return;

            string text = BuildSelectedText();
            if (text.Length == 0)
                return;

            var clipboard = TopLevel.GetTopLevel(this)?.Clipboard;
            if (clipboard != null)
                await clipboard.SetTextAsync(text);
        }

        protected override void OnPointerPressed(PointerPressedEventArgs e)
        {
            base.OnPointerPressed(e);

            if (!e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
                return;

            if (_items == null || _rowCount == 0)
                return;

            Focus();

            var pos = e.GetPosition(this);
            var hit = HitTestPosition(pos);

            if (e.KeyModifiers.HasFlag(KeyModifiers.Shift) && HasSelection)
            {
                _selectionActive = hit;
            }
            else if (e.ClickCount == 2)
            {
                SelectWordAt(hit);
            }
            else if (e.ClickCount >= 3)
            {
                _selectionAnchor = (hit.Row, 0);
                _selectionActive = (hit.Row, GetRowTextLength(_items[hit.Row]));
            }
            else
            {
                _selectionAnchor = hit;
                _selectionActive = hit;
                _isSelecting = true;
                _hasDragged = false;
                _pressPosition = pos;
                _pressOnLink = (uint)hit.Row < (uint)_items.Count && _items[hit.Row].HasFileLocation && pos.X <= _prefixWidths[hit.Row];
                e.Pointer.Capture(this);
            }

            InvalidateVisual();
            e.Handled = true;
        }

        protected override void OnPointerMoved(PointerEventArgs e)
        {
            base.OnPointerMoved(e);

            if (_isSelecting)
            {
                var pos = e.GetPosition(this);
                if (Math.Abs(pos.X - _pressPosition.X) > 3 || Math.Abs(pos.Y - _pressPosition.Y) > 3)
                    _hasDragged = true;
                _selectionActive = HitTestPosition(pos);
                InvalidateVisual();
                return;
            }

            if (_items == null || _rowCount == 0)
            {
                Cursor = Cursor.Default;
                return;
            }

            var pos2 = e.GetPosition(this);
            int index = FindFirstVisibleRow(_scrollOffset.Y + pos2.Y);

            bool overLink = (uint)index < (uint)_items.Count
                && _items[index].HasFileLocation
                && pos2.X <= _prefixWidths[index];

            Cursor = overLink ? HandCursor : IbeamCursor;
        }

        protected override void OnPointerReleased(PointerReleasedEventArgs e)
        {
            base.OnPointerReleased(e);

            if (e.InitialPressMouseButton != MouseButton.Left || !_isSelecting)
                return;

            _isSelecting = false;
            if (e.Pointer.Captured == this)
                e.Pointer.Capture(null);

            var pos = e.GetPosition(this);
            if (Math.Abs(pos.X - _pressPosition.X) > 3 || Math.Abs(pos.Y - _pressPosition.Y) > 3)
                _hasDragged = true;

            if (!_hasDragged && _pressOnLink && _items != null)
                NavigateToFileCommand.DefaultInstance.Execute(_items[(int)Math.Clamp(_selectionAnchor.Row, 0, _items.Count - 1)]);

            e.Handled = true;
        }

        protected override void OnKeyDown(KeyEventArgs e)
        {
            base.OnKeyDown(e);

            bool ctrl = e.KeyModifiers.HasFlag(KeyModifiers.Control);

            if (ctrl && e.Key == Key.A)
            {
                SelectAll();
                e.Handled = true;
            }
            else if (ctrl && (e.Key == Key.C || e.Key == Key.Insert))
            {
                CopySelection();
                e.Handled = true;
            }
            else if (e.Key == Key.Escape && HasSelection)
            {
                _selectionActive = _selectionAnchor;
                InvalidateVisual();
                e.Handled = true;
            }
        }

        private static TextLayout CreateLayout(string text, IBrush foreground, double maxWidth, bool wrap)
        {
            return new TextLayout(
                text,
                MonoTypeface,
                FontSize,
                foreground,
                TextAlignment.Left,
                wrap ? TextWrapping.Wrap : TextWrapping.NoWrap,
                TextTrimming.None,
                null,
                FlowDirection.LeftToRight,
                maxWidth,
                double.PositiveInfinity,
                double.NaN,
                0,
                0,
                null);
        }

        private double MeasurePrefixWidth(int index)
        {
            if (_items == null || index >= _items.Count) return 0;

            var entry = _items[index];
            if (!entry.HasFileLocation) return 0;

            var layout = _prefixLayout[index] ??= CreateLayout(entry.FileLocationText, LinkBrush, double.PositiveInfinity, wrap: false);

            return layout.Width;
        }

        private double MeasureRowHeight(int index, double availableWidth, double prefixWidth)
        {
            if (_items == null || index >= _items.Count) return 0;

            var entry = _items[index];
            double messageWidth = Math.Max(1, availableWidth - (prefixWidth > 0 ? prefixWidth + FilePrefixMargin : 0));

            var layout = _messageLayout[index];
            if (layout == null)
            {
                layout = CreateLayout(
                    entry.Message.Length > 0 ? entry.Message : " ",
                    GetBrushForLevel(entry.Level),
                    messageWidth,
                    wrap: true);
                _messageLayout[index] = layout;
            }

            return Math.Max(layout.Height, FontSize) + RowPadding;
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

            int clearCount = Math.Min(_rowCount, _messageLayout.Length);
            if (clearCount > 0)
                Array.Clear(_messageLayout, 0, clearCount);

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
            Array.Resize(ref _prefixLayout, newSize);
            Array.Resize(ref _messageLayout, newSize);
        }

        private void ResetCache()
        {
            int count = _rowCount;
            _rowCount = 0;
            _totalContentHeight = 0;
            _lastRenderWidth = -1;
            _isSelecting = false;
            _selectionAnchor = (0, 0);
            _selectionActive = (0, 0);

            if (count > 0)
            {
                int n = Math.Min(count, _rowTopY.Length);
                Array.Clear(_rowTopY, 0, n);
                Array.Clear(_rowHeights, 0, n);
                Array.Clear(_prefixWidths, 0, n);
                Array.Clear(_prefixLayout, 0, Math.Min(count, _prefixLayout.Length));
                Array.Clear(_messageLayout, 0, Math.Min(count, _messageLayout.Length));
            }
        }
    }
}
