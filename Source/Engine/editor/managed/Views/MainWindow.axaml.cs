using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Platform;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Platform;
using Avalonia.Threading;
using Avalonia.VisualTree;
using System;
using System.Linq;
using Hyperion.Editor.ViewModels;
using Hyperion.Editor.Services;

namespace Hyperion.Editor
{
    public partial class MainWindow : Window
    {
        private const bool CappedFrameRate = true;
        private const bool IsRenderingOnMainThread = true;

        private Grid? _bottomPanelGrid;
        private GridSplitter? _bottomPanelSplitter;
        private Control? _contentBrowserPanel;
        private Border? _contentBrowserCollapsedStrip;
        private Control? _consolePanel;
        private Border? _consoleCollapsedStrip;
        private bool _contentBrowserExpanded = true;
        private bool _consoleExpanded = true;

        private const string NodeViewModelDragFormat = "application/x-hyperion-nodeviewmodel";
        private NodeViewModel? _dragCandidate;
        private Point _dragStartPoint;
        private bool _isDragging;

        // Drop-indicator tracking and auto-scroll
        private TreeView? _sceneTree;
        private ScrollViewer? _sceneTreeScrollViewer;
        private DispatcherTimer? _autoScrollTimer;
        private double _autoScrollDelta;
        private const double AutoScrollZone = 36;  // px from edge that triggers scroll
        private const double AutoScrollSpeed = 10; // px per timer tick (~50 ms)

        public MainWindow()
        {
            InitializeComponent();

            // Provide engine window to the viewport control via factory
            EditorViewportControl? evc = this.FindControl<EditorViewportControl>("EditorViewportControl");

            if (evc == null)
            {
                throw new Exception("EditorViewportControl control not found in MainWindow.");
            }

            evc.Focus();

            DataContext = new MainWindowViewModel();

            _bottomPanelGrid = this.FindControl<Grid>("BottomPanelGrid");
            _bottomPanelSplitter = this.FindControl<GridSplitter>("BottomPanelSplitter");
            _contentBrowserPanel = this.FindControl<Control>("ContentBrowserPanel");
            _contentBrowserCollapsedStrip = this.FindControl<Border>("ContentBrowserCollapsedStrip");
            _consolePanel = this.FindControl<Control>("ConsolePanel");
            _consoleCollapsedStrip = this.FindControl<Border>("ConsoleCollapsedStrip");

            var collapseContentBrowser = this.FindControl<Button>("CollapseContentBrowser");
            var expandContentBrowser = this.FindControl<Button>("ExpandContentBrowser");
            var collapseConsole = this.FindControl<Button>("CollapseConsole");
            var expandConsole = this.FindControl<Button>("ExpandConsole");

            if (collapseContentBrowser != null) collapseContentBrowser.Click += OnCollapseContentBrowser;
            if (expandContentBrowser != null) expandContentBrowser.Click += OnExpandContentBrowser;
            if (collapseConsole != null) collapseConsole.Click += OnCollapseConsole;
            if (expandConsole != null) expandConsole.Click += OnExpandConsole;

            SetupSceneHierarchyDragDrop();

            if (IsRenderingOnMainThread)
            {
                Opened += (s, e) =>
                {
                    var topLevel = TopLevel.GetTopLevel(this);
                    topLevel?.RequestAnimationFrame(OnFrame);
                };
            }
        }

        private void SetupSceneHierarchyDragDrop()
        {
            _sceneTree = this.FindControl<TreeView>("SceneHierarchyTreeView");
            if (_sceneTree == null)
                return;

            DragDrop.SetAllowDrop(_sceneTree, true);

           
            _sceneTree.AddHandler(InputElement.PointerPressedEvent, OnSceneTreePointerPressed, RoutingStrategies.Tunnel);
            _sceneTree.AddHandler(InputElement.PointerMovedEvent, OnSceneTreePointerMoved, RoutingStrategies.Tunnel);
            _sceneTree.AddHandler(InputElement.PointerReleasedEvent, OnSceneTreePointerReleased, RoutingStrategies.Tunnel);
            _sceneTree.AddHandler(DragDrop.DragOverEvent, OnSceneTreeDragOver);
            _sceneTree.AddHandler(DragDrop.DragLeaveEvent, OnSceneTreeDragLeave);
            _sceneTree.AddHandler(DragDrop.DropEvent, OnSceneTreeDrop);

            // Lazily find the internal ScrollViewer once the template is applied.
            _sceneTree.TemplateApplied += (_, _) =>
            {
                _sceneTreeScrollViewer = _sceneTree.GetVisualDescendants().OfType<ScrollViewer>().FirstOrDefault();
            };
        }

        private void OnSceneTreePointerPressed(object? sender, PointerPressedEventArgs e)
        {
            if (e.GetCurrentPoint(sender as Visual).Properties.IsLeftButtonPressed)
            {
                _dragCandidate = FindNodeViewModelInEventSource(e.Source);
                _dragStartPoint = e.GetPosition(sender as Visual);
                _isDragging = false;
            }
        }

        private async void OnSceneTreePointerMoved(object? sender, PointerEventArgs e)
        {
            if (_dragCandidate == null || _isDragging)
                return;

            if (!e.GetCurrentPoint(sender as Visual).Properties.IsLeftButtonPressed)
            {
                _dragCandidate = null;
                return;
            }

            var pos = e.GetPosition(sender as Visual);
            var delta = pos - _dragStartPoint;

            // Only start a drag after a small movement threshold to avoid stealing normal clicks.
            if (Math.Abs(delta.X) < 5 && Math.Abs(delta.Y) < 5)
                return;

            _isDragging = true;
            var candidate = _dragCandidate;

            var data = new DataObject();
            data.Set(NodeViewModelDragFormat, candidate);

            try
            {
                await DragDrop.DoDragDrop(e, data, DragDropEffects.Move);
            }
            catch (Exception ex) when (ex is System.Runtime.InteropServices.COMException)
            {
                // DoDragDrop can throw on Windows if the drag is cancelled externally
                // or the pointer state is unexpected; treat as a cancelled drag.
            }
            finally
            {
                EndDrag();
            }
        }

        private void OnSceneTreePointerReleased(object? sender, PointerReleasedEventArgs e)
        {
            if (!_isDragging)
                _dragCandidate = null;
        }

        private void OnSceneTreeDragOver(object? sender, DragEventArgs e)
        {
            if (!e.Data.Contains(NodeViewModelDragFormat))
            {
                e.DragEffects = DragDropEffects.None;
                return;
            }

            var vm = DataContext as MainWindowViewModel;
            var dragged = e.Data.Get(NodeViewModelDragFormat) as NodeViewModel;
            var target = FindNodeViewModelInEventSource(e.Source);

            bool valid = dragged != null
                && target != null
                && target != dragged
                && !SceneHierarchyViewModel.IsAncestorOf(dragged, target);

            e.DragEffects = valid ? DragDropEffects.Move : DragDropEffects.None;

            vm?.SceneHierarchy.SetDropTarget(valid ? target : null);

            UpdateAutoScroll(e.GetPosition(_sceneTree));

            e.Handled = true;
        }

        private void OnSceneTreeDragLeave(object? sender, DragEventArgs e)
        {
            EndDrag();
        }

        private void OnSceneTreeDrop(object? sender, DragEventArgs e)
        {
            if (!e.Data.Contains(NodeViewModelDragFormat))
                return;

            var dragged = e.Data.Get(NodeViewModelDragFormat) as NodeViewModel;
            var target = FindNodeViewModelInEventSource(e.Source);

            EndDrag();

            if (dragged == null || target == null)
                return;

            var vm = DataContext as MainWindowViewModel;
            vm?.SceneHierarchy.ReparentNode(dragged, target);

            e.Handled = true;
        }

        private void EndDrag()
        {
            _isDragging = false;
            _dragCandidate = null;

            var vm = DataContext as MainWindowViewModel;
            vm?.SceneHierarchy.SetDropTarget(null);

            _autoScrollDelta = 0;
            _autoScrollTimer?.Stop();
        }

        private void UpdateAutoScroll(Point posRelativeToTree)
        {
            if (_sceneTreeScrollViewer == null || _sceneTree == null)
                return;

            var treeHeight = _sceneTree.Bounds.Height;

            if (posRelativeToTree.Y < AutoScrollZone)
                _autoScrollDelta = -AutoScrollSpeed * (1.0 - posRelativeToTree.Y / AutoScrollZone);
            else if (posRelativeToTree.Y > treeHeight - AutoScrollZone)
                _autoScrollDelta = AutoScrollSpeed * (1.0 - (treeHeight - posRelativeToTree.Y) / AutoScrollZone);
            else
                _autoScrollDelta = 0;

            if (_autoScrollDelta != 0)
            {
                if (_autoScrollTimer == null)
                {
                    _autoScrollTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(50) };
                    _autoScrollTimer.Tick += OnAutoScrollTick;
                }

                if (!_autoScrollTimer.IsEnabled)
                    _autoScrollTimer.Start();
            }
            else
            {
                _autoScrollTimer?.Stop();
            }
        }

        private void OnAutoScrollTick(object? sender, EventArgs e)
        {
            if (_sceneTreeScrollViewer == null || _autoScrollDelta == 0)
            {
                _autoScrollTimer?.Stop();
                return;
            }

            var offset = _sceneTreeScrollViewer.Offset;
            _sceneTreeScrollViewer.Offset = new Vector(offset.X, Math.Max(0, offset.Y + _autoScrollDelta));
        }

        private static NodeViewModel? FindNodeViewModelInEventSource(object? source)
        {
            // Walk up the visual tree from the event source to find a DataContext that is a NodeViewModel.
            var control = source as Control;
            while (control != null)
            {
                if (control.DataContext is NodeViewModel nvm)
                    return nvm;
                control = control.Parent as Control;
            }
            return null;
        }

        private void OnCollapseContentBrowser(object? sender, RoutedEventArgs e) { _contentBrowserExpanded = false; UpdateBottomPanelLayout(); }
        private void OnExpandContentBrowser(object? sender, RoutedEventArgs e) { _contentBrowserExpanded = true; UpdateBottomPanelLayout(); }
        private void OnCollapseConsole(object? sender, RoutedEventArgs e) { _consoleExpanded = false; UpdateBottomPanelLayout(); }
        private void OnExpandConsole(object? sender, RoutedEventArgs e) { _consoleExpanded = true; UpdateBottomPanelLayout(); }

        private void UpdateBottomPanelLayout()
        {
            if (_bottomPanelGrid == null) return;

            var cols = _bottomPanelGrid.ColumnDefinitions;
            cols[0].Width = _contentBrowserExpanded ? new GridLength(1, GridUnitType.Star) : new GridLength(30);
            cols[1].Width = (!_contentBrowserExpanded && !_consoleExpanded) ? new GridLength(0) : new GridLength(2);
            cols[2].Width = _consoleExpanded ? new GridLength(1, GridUnitType.Star) : new GridLength(30);

             bool bothExpanded = _contentBrowserExpanded && _consoleExpanded;
            if (_bottomPanelSplitter != null) _bottomPanelSplitter.IsEnabled = bothExpanded;

            if (_contentBrowserPanel != null) _contentBrowserPanel.IsVisible = _contentBrowserExpanded;
            if (_contentBrowserCollapsedStrip != null) _contentBrowserCollapsedStrip.IsVisible = !_contentBrowserExpanded;
            if (_consolePanel != null) _consolePanel.IsVisible = _consoleExpanded;
            if (_consoleCollapsedStrip != null) _consoleCollapsedStrip.IsVisible = !_consoleExpanded;
        }

        // need to destroy the engine window when MainWindow is closed
        protected override void OnClosed(EventArgs e)
        {
            base.OnClosed(e);
        }

        private void OnFrame(TimeSpan time)
        {
            NativeBindings.Hyp_MainThreadUpdate();

            ConsoleService.Instance.ProcessLogQueue();

            var topLevel = GetTopLevel(this);
            topLevel?.RequestAnimationFrame(OnFrame);
        }

        // protected override void OnKeyDown(Avalonia.Input.KeyEventArgs e)
        // {
        //     base.OnKeyDown(e);

        //     var vm = DataContext as MainWindowViewModel;
        //     vm?.HandleKeyDown(e);
        // }
    }
}
