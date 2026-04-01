using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Platform;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Platform;
using Avalonia.Threading;
using System;
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
            var tree = this.FindControl<TreeView>("SceneHierarchyTreeView");
            if (tree == null)
                return;

            DragDrop.SetAllowDrop(tree, true);

            // Tunnel (Preview) handlers so we see events before TreeView item selection consumes them.
            tree.AddHandler(InputElement.PointerPressedEvent, OnSceneTreePointerPressed, RoutingStrategies.Tunnel);
            tree.AddHandler(InputElement.PointerMovedEvent, OnSceneTreePointerMoved, RoutingStrategies.Tunnel);
            tree.AddHandler(InputElement.PointerReleasedEvent, OnSceneTreePointerReleased, RoutingStrategies.Tunnel);
            tree.AddHandler(DragDrop.DragOverEvent, OnSceneTreeDragOver);
            tree.AddHandler(DragDrop.DropEvent, OnSceneTreeDrop);
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

            await DragDrop.DoDragDrop(e, data, DragDropEffects.Move);

            _isDragging = false;
            _dragCandidate = null;
        }

        private void OnSceneTreePointerReleased(object? sender, PointerReleasedEventArgs e)
        {
            if (!_isDragging)
            {
                _dragCandidate = null;
            }
        }

        private void OnSceneTreeDragOver(object? sender, DragEventArgs e)
        {
            if (e.Data.Contains(NodeViewModelDragFormat))
            {
                var dragged = e.Data.Get(NodeViewModelDragFormat) as NodeViewModel;
                var target = FindNodeViewModelInEventSource(e.Source);

                if (dragged != null && target != null && target != dragged && !SceneHierarchyViewModel.IsAncestorOf(dragged, target))
                {
                    e.DragEffects = DragDropEffects.Move;
                }
                else
                {
                    e.DragEffects = DragDropEffects.None;
                }

                e.Handled = true;
            }
            else
            {
                e.DragEffects = DragDropEffects.None;
            }
        }

        private void OnSceneTreeDrop(object? sender, DragEventArgs e)
        {
            if (!e.Data.Contains(NodeViewModelDragFormat))
                return;

            var dragged = e.Data.Get(NodeViewModelDragFormat) as NodeViewModel;
            var target = FindNodeViewModelInEventSource(e.Source);

            if (dragged == null || target == null)
                return;

            var vm = DataContext as MainWindowViewModel;
            vm?.SceneHierarchy.ReparentNode(dragged, target);

            e.Handled = true;
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
