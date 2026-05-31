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
using Hyperion;
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
        private const string AssetDragFormat = "application/x-hyperion-asset";
        private NodeViewModel? _dragCandidate;
        private Point _dragStartPoint;
        private bool _isDragging;

        // Content browser drag tracking
        private ListBox? _contentBrowserAssetList;
        private AssetObjectViewModel? _assetDragCandidate;
        private Point _assetDragStartPoint;
        private bool _isDraggingAsset;

        // Viewport drop tracking
        private Border? _viewportDropTarget;

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
            SetupContentBrowserDragDrop();
            SetupViewportDropTarget();

            AddHandler(InputElement.LostFocusEvent, OnInspectorTextBoxLostFocus, RoutingStrategies.Bubble);
            AddHandler(InputElement.KeyDownEvent, OnInspectorTextBoxKeyDown, RoutingStrategies.Bubble);

            if (IsRenderingOnMainThread)
            {
                Opened += (s, e) =>
                {
                    var topLevel = TopLevel.GetTopLevel(this);
                    topLevel?.RequestAnimationFrame(OnFrame);
                };
            }
        }

        private void OnInspectorTextBoxLostFocus(object? sender, RoutedEventArgs e)
        {
            if (e.Source is TextBox { DataContext: InspectorPropertyViewModelBase vm })
            {
                vm.CommitValue();
            }
        }

        private void OnInspectorTextBoxKeyDown(object? sender, KeyEventArgs e)
        {
            if (e.Key == Key.Return && e.Source is TextBox { DataContext: InspectorPropertyViewModelBase vm })
            {
                vm.CommitValue();
                e.Handled = true;
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

            _sceneTree.SelectionChanged += OnSceneTreeSelectionChanged;

            // Lazily find the internal ScrollViewer once the template is applied.
            _sceneTree.TemplateApplied += (_, _) =>
            {
                _sceneTreeScrollViewer = _sceneTree.GetVisualDescendants().OfType<ScrollViewer>().FirstOrDefault();
            };
        }

        private void OnSceneTreePointerPressed(object? sender, PointerPressedEventArgs e)
        {
            var point = e.GetCurrentPoint(sender as Visual);

            if (point.Properties.IsRightButtonPressed)
            {
                // Let the TreeView handle context menu natively; do not alter selection
                return;
            }

            if (point.Properties.IsLeftButtonPressed)
            {
                var keyModifiers = e.KeyModifiers;

                if ((keyModifiers & KeyModifiers.Shift) != 0)
                {
                    // Shift+click: range selection
                    var nodeVm = FindNodeViewModelInEventSource(e.Source);
                    if (nodeVm != null)
                    {
                        var vm = DataContext as MainWindowViewModel;
                        vm?.HandleShiftClick(nodeVm);
                    }

                    e.Handled = true;
                    return;
                }

                // Suppress SelectedNodeChanged notification until SelectionChanged handles the sync
                if (DataContext is MainWindowViewModel mvm)
                {
                    mvm.SceneHierarchy.SetSuppressSelectionNotifications(true);
                }

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
            if (e.Data.Contains(AssetDragFormat))
            {
                var t = FindNodeViewModelInEventSource(e.Source);
                e.DragEffects = t != null ? DragDropEffects.Copy : DragDropEffects.None;

                var vm = DataContext as MainWindowViewModel;
                vm?.SceneHierarchy.SetDropTarget(t);
                UpdateAutoScroll(e.GetPosition(_sceneTree));
                e.Handled = true;
                return;
            }

            if (!e.Data.Contains(NodeViewModelDragFormat))
            {
                e.DragEffects = DragDropEffects.None;
                return;
            }

            var vm_node = DataContext as MainWindowViewModel;
            var dragged = e.Data.Get(NodeViewModelDragFormat) as NodeViewModel;
            var target = FindNodeViewModelInEventSource(e.Source);

            bool valid = dragged != null
                && target != null
                && target != dragged
                && !SceneHierarchyViewModel.IsAncestorOf(dragged, target);

            e.DragEffects = valid ? DragDropEffects.Move : DragDropEffects.None;

            vm_node?.SceneHierarchy.SetDropTarget(valid ? target : null);

            UpdateAutoScroll(e.GetPosition(_sceneTree));

            e.Handled = true;
        }

        private void OnSceneTreeDragLeave(object? sender, DragEventArgs e)
        {
            EndDrag();
        }

        private void OnSceneTreeDrop(object? sender, DragEventArgs e)
        {
            if (e.Data.Contains(AssetDragFormat))
            {
                var t = FindNodeViewModelInEventSource(e.Source);
                EndDrag();

                var vm = DataContext as MainWindowViewModel;
                if (vm != null)
                {
                    var assetData = e.Data.Get(AssetDragFormat) as string;
                    if (!string.IsNullOrEmpty(assetData))
                    {
                        var parts = assetData.Split('|');
                        if (parts.Length == 2 && uint.TryParse(parts[0], out uint bucketIndex))
                        {
                            vm.AddAssetToScene(bucketIndex, new Name(parts[1]));
                        }
                    }
                }

                e.Handled = true;
                return;
            }

            if (!e.Data.Contains(NodeViewModelDragFormat))
                return;

            var dragged = e.Data.Get(NodeViewModelDragFormat) as NodeViewModel;
            var target = FindNodeViewModelInEventSource(e.Source);

            EndDrag();

            if (dragged == null || target == null)
                return;

            var vm_node = DataContext as MainWindowViewModel;
            vm_node?.SceneHierarchy.ReparentNode(dragged, target);

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

        private void OnSceneTreeSelectionChanged(object? sender, SelectionChangedEventArgs e)
        {
            var mvm = DataContext as MainWindowViewModel;
            if (mvm == null)
                return;

            var added = e.AddedItems.OfType<NodeViewModel>().ToList();
            var removed = e.RemovedItems.OfType<NodeViewModel>().ToList();

            // Un-suppress after SelectedItem binding fired (suppressed)
            mvm.SceneHierarchy.SetSuppressSelectionNotifications(false);

            mvm.HandleTreeSelectionChanged(added, removed);
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

        private void SetupContentBrowserDragDrop()
        {
            _contentBrowserAssetList = this.FindControl<ListBox>("ContentBrowserAssetList");
            if (_contentBrowserAssetList == null)
                return;

            DragDrop.SetAllowDrop(_contentBrowserAssetList, true);

            _contentBrowserAssetList.AddHandler(InputElement.PointerPressedEvent, OnContentBrowserPointerPressed, RoutingStrategies.Tunnel);
            _contentBrowserAssetList.AddHandler(InputElement.PointerMovedEvent, OnContentBrowserPointerMoved, RoutingStrategies.Tunnel);
        }

        private void OnContentBrowserPointerPressed(object? sender, PointerPressedEventArgs e)
        {
            var point = e.GetCurrentPoint(sender as Visual);

            if (point.Properties.IsLeftButtonPressed)
            {
                _assetDragCandidate = FindAssetViewModelInEventSource(e.Source);
                _assetDragStartPoint = e.GetPosition(sender as Visual);
                _isDraggingAsset = false;
            }
        }

        private async void OnContentBrowserPointerMoved(object? sender, PointerEventArgs e)
        {
            if (_assetDragCandidate == null || _isDraggingAsset)
                return;

            if (!e.GetCurrentPoint(sender as Visual).Properties.IsLeftButtonPressed)
            {
                _assetDragCandidate = null;
                return;
            }

            var pos = e.GetPosition(sender as Visual);
            var delta = pos - _assetDragStartPoint;

            if (Math.Abs(delta.X) < 5 && Math.Abs(delta.Y) < 5)
                return;

            _isDraggingAsset = true;
            var candidate = _assetDragCandidate;

            var data = new DataObject();
            data.Set(AssetDragFormat, $"{candidate.Bucket?.BucketIndex ?? 0}|{candidate.AssetDesc.Name}");

            try
            {
                await DragDrop.DoDragDrop(e, data, DragDropEffects.Copy);
            }
            catch (Exception ex) when (ex is System.Runtime.InteropServices.COMException)
            {
            }
            finally
            {
                _isDraggingAsset = false;
                _assetDragCandidate = null;
            }
        }

        private static AssetObjectViewModel? FindAssetViewModelInEventSource(object? source)
        {
            var control = source as Control;
            while (control != null)
            {
                if (control.DataContext is AssetObjectViewModel avm)
                    return avm;
                control = control.Parent as Control;
            }
            return null;
        }

        private void SetupViewportDropTarget()
        {
            _viewportDropTarget = this.FindControl<Border>("ViewportDropTarget");
            if (_viewportDropTarget == null)
                return;

            DragDrop.SetAllowDrop(_viewportDropTarget, true);
            _viewportDropTarget.AddHandler(DragDrop.DragOverEvent, OnViewportDragOver);
            _viewportDropTarget.AddHandler(DragDrop.DropEvent, OnViewportDrop);
        }

        private void OnViewportDragOver(object? sender, DragEventArgs e)
        {
            if (e.Data.Contains(AssetDragFormat))
            {
                e.DragEffects = DragDropEffects.Copy;
                e.Handled = true;
            }
        }

        private void OnViewportDrop(object? sender, DragEventArgs e)
        {
            if (!e.Data.Contains(AssetDragFormat))
                return;

            var assetData = e.Data.Get(AssetDragFormat) as string;
            if (string.IsNullOrEmpty(assetData))
                return;

            var parts = assetData.Split('|');
            if (parts.Length != 2 || !uint.TryParse(parts[0], out uint bucketIndex))
                return;

            // Calculate normalized drop position within the viewport
            var pos = e.GetPosition(_viewportDropTarget);
            double nx = Math.Clamp(pos.X / _viewportDropTarget.Bounds.Width, 0.0, 1.0);
            double ny = Math.Clamp(pos.Y / _viewportDropTarget.Bounds.Height, 0.0, 1.0);

            var vm = DataContext as MainWindowViewModel;
            vm?.AddAssetToSceneAtViewport(bucketIndex, new Name(parts[1]), (float)nx, (float)ny);

            e.Handled = true;
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

        protected override void OnClosing(WindowClosingEventArgs e)
        {
            // Disable main thread loop until this is done 
            // This should prevent MainThread::Update() from being triggered by avalonia
            // directly after clicking any of the messagebox buttons
            EngineManager.DisableMainLoop = true;

            try
            {
                var vm = DataContext as MainWindowViewModel;
                var project = EngineManager.CurrentProject;

                void SaveProjectSynchronous()
                {
                    if (vm == null)
                    {
                        return;
                    }

                    bool shouldTimeout = project != null && project.IsSaved;

                    Task task = EngineManager.PostToSimThread(() => vm.SaveProject.Execute(null));
                    bool taskCompleted = true;

                    if (shouldTimeout)
                        taskCompleted = task.Wait(TimeSpan.FromSeconds(30));
                    else
                        task.Wait();

                    if (!taskCompleted)
                    {
                        Logger.Log(LogLevel.Error, "Failed to save project in a reasonable amount of time, so canceling exiting the editor process to prevent loss of data.");
                        e.Cancel = true;

                        return;
                    }
                }

                MessageBox.Info("Save changes?", "Closing will discard any unsaved changes. Do you want to save changes before exiting?")
                    .Button("Save", SaveProjectSynchronous)
                    .Button("Discard", () => { })
                    .Button("Cancel", () => e.Cancel = true)
                    .Show();

                base.OnClosing(e);

                EngineManager.DisableMainLoop = false;

                if (!e.Cancel)
                {
                    EngineManager.Shutdown();
                }
            }
            catch (Exception)
            {
                EngineManager.DisableMainLoop = false;
            }
        }

        private void OnFrame(TimeSpan time)
        {
            if (!EngineManager.DisableMainLoop)
            {
                NativeBindings.Hyp_MainThreadUpdate();
            }

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
