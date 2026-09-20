using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Platform;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media;
using Avalonia.Platform;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Dock.Model;
using Dock.Model.Core;
using Dock.Model.Controls;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows.Input;
using Hyperion;
using Hyperion.Editor.Views;
using Hyperion.Editor.ViewModels;
using Hyperion.Editor.Services;
using Document = Dock.Model.Avalonia.Controls.Document;
using DocumentDock = Dock.Model.Avalonia.Controls.DocumentDock;
using ProportionalDock = Dock.Model.Avalonia.Controls.ProportionalDock;
using ProportionalDockSplitter = Dock.Model.Avalonia.Controls.ProportionalDockSplitter;
using RootDock = Dock.Model.Avalonia.Controls.RootDock;
using Tool = Dock.Model.Avalonia.Controls.Tool;
using ToolDock = Dock.Model.Avalonia.Controls.ToolDock;
using DockableClosedEventArgs = Dock.Model.Core.Events.DockableClosedEventArgs;

namespace Hyperion.Editor
{
    public partial class MainWindow : Window
    {
        private const bool CappedFrameRate = true;
        private const bool IsRenderingOnMainThread = true;

        private static readonly DataFormat<NodeViewModel> NodeViewModelDragFormat =
            DataFormat.CreateInProcessFormat<NodeViewModel>("hyperion-nodeviewmodel");
        private static readonly DataFormat<string> AssetDragFormat =
            DataFormat.CreateStringApplicationFormat("hyperion-asset");
        private NodeViewModel? _dragCandidate;
        private PointerPressedEventArgs? _dragPressedArgs;
        private Point _dragStartPoint;
        private bool _isDragging;
        private bool _suppressTreeSelectionHandling;

        // Content browser drag tracking
        private ListBox? _contentBrowserAssetList;
        private AssetObjectViewModel? _assetDragCandidate;
        private PointerPressedEventArgs? _assetDragPressedArgs;
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

        private DropDownButton? _sceneDropDown;
        private EditorViewportControl? _editorViewport;
        private WorldSettingsWindow? _worldSettingsWindow;
        private IDock? _dynamicPanelsDock;
        private readonly Dictionary<EditorPanelViewModel, Tool> _dynamicPanelTools = new();
        private int _frameCounter;
        private bool _viewportNativeHidden;
        private bool _isClosing;
        private bool _suppressPanelSync;
        private readonly Dictionary<string, object?> _dockContents = new Dictionary<string, object?>();

        public MainWindow()
        {
            var viewModel = new MainWindowViewModel();
            MainWindowViewModel.Instance = viewModel;
            DataContext = viewModel;

            InitializeComponent();

            foreach (Control entry in BuildAddSceneMenuEntries(viewModel))
            {
                AddSceneContextMenu.Items.Add(entry);
            }

            PanelService.Instance.ActivePanelChanged += OnPanelServiceActivePanelChanged;

            // Detect a dynamic panel's tab being closed via its own [x] button so the
            // matching view model's OnClosed callback still runs.
            DockControl.Factory.DockableClosed += OnFactoryDockableClosed;

            AddHandler(InputElement.GotFocusEvent, OnInspectorTextBoxGotFocus, RoutingStrategies.Bubble);
            AddHandler(InputElement.LostFocusEvent, OnInspectorTextBoxLostFocus, RoutingStrategies.Bubble);
            AddHandler(InputElement.KeyDownEvent, OnInspectorTextBoxKeyDown, RoutingStrategies.Bubble);

            AddHandler(InputElement.LostFocusEvent, OnNodeRenameTextBoxLostFocus, RoutingStrategies.Bubble);
            AddHandler(InputElement.KeyDownEvent, OnNodeRenameTextBoxKeyDown, RoutingStrategies.Bubble);

            Opened += OnWindowOpened;
        }

        private void OnWindowOpened(object? sender, EventArgs e)
        {
            if (IsRenderingOnMainThread)
            {
                var topLevel = TopLevel.GetTopLevel(this);
                topLevel?.RequestAnimationFrame(OnFrame);
            }

            // The dock layout hosts the panel content inside its own template namescope, so the
            // named controls are not reachable via FindControl from the window. Locate them by
            // walking the visual tree once the dock has rendered and laid out.
            //
            // The dock realizes its content during the first layout pass, which runs at Render
            // priority - after any dispatcher priority we could post at - so a single posted
            // callback races the layout and misses the controls. Drive the setup off
            // LayoutUpdated until every control has been located.
            Dispatcher.UIThread.Post(OnWindowLoaded, DispatcherPriority.Loaded);
            DockControl.LayoutUpdated += OnDockControlLayoutUpdated;
        }

        private void OnDockControlLayoutUpdated(object? sender, EventArgs e)
        {
            OnWindowLoaded();
        }

        private void OnWindowLoaded()
        {
            bool complete =
                InitializeViewportControl()
                & InitializeSceneFlyout()
                & SetupSceneHierarchyDragDrop()
                & SetupContentBrowserDragDrop()
                & SetupViewportDropTarget();

            if (complete)
            {
                DockControl.LayoutUpdated -= OnDockControlLayoutUpdated;
                CaptureDockContents();
            }
        }

        private void OnResetLayoutClick(object? sender, RoutedEventArgs e)
        {
            ResetLayout();
        }

        private void OnWorldSettingsClick(object? sender, RoutedEventArgs e)
        {
            ShowWorldSettingsWindow();
        }

        private void OnDropDownClick(object? sender, RoutedEventArgs e)
        {
            if (sender is Control { ContextMenu: ContextMenu menu })
            {
                menu.Open();
            }
        }

        private static IEnumerable<Control> BuildAddSceneMenuEntries(MainWindowViewModel viewModel)
        {
            static MenuItem GroupHeader(string text) => new MenuItem
            {
                Margin = new Thickness(0, 2),
                Padding = new Thickness(8, 4),
                HorizontalAlignment = Avalonia.Layout.HorizontalAlignment.Center,
                FontSize = 12,
                Foreground = Brush.Parse("#808080"),
                Header = text,
                IsEnabled = false,
            };

            static MenuItem Entry(string header, ICommand command) => new MenuItem
            {
                Header = header,
                Command = command,
            };

            static MenuItem Submenu(string header, params MenuItem[] children)
            {
                var item = new MenuItem { Header = header };
                foreach (MenuItem child in children)
                {
                    item.Items.Add(child);
                }
                return item;
            }

            yield return GroupHeader("Node");
            yield return Entry("Base Node", viewModel.AddEmptyNode);
            yield return new Separator();
            yield return Entry("Entity", viewModel.AddEntity);
            yield return Entry("Camera", viewModel.AddCamera);
            yield return new Separator();

            yield return GroupHeader("Rendering");
            yield return Entry("Instanced Mesh", viewModel.AddInstance);
            yield return new Separator();

            yield return GroupHeader("2D");
            yield return Entry("Sprite", viewModel.AddSprite);
            yield return Entry("Text Sprite", viewModel.AddTextSprite);
            yield return new Separator();

            yield return GroupHeader("Lighting");
            yield return Entry("Directional Light", viewModel.AddDirectionalLight);
            yield return Entry("Point Light", viewModel.AddPointLight);
            yield return Entry("Spot Light", viewModel.AddSpotLight);
            yield return Entry("Rectangular Area Light", viewModel.AddAreaRectLight);
            yield return new Separator();
            yield return Entry("Reflection Probe", viewModel.AddReflectionProbe);
            yield return Entry("Irradiance Probe", viewModel.AddIrradianceProbe);
            yield return Entry("Lightmap Volume", viewModel.AddLightmapVolume);
            yield return new Separator();

            yield return GroupHeader("FX");
            yield return Entry("Particle Volume", viewModel.AddParticleVolume);
            yield return Entry("Fog Volume", viewModel.AddFogVolume);
            yield return new Separator();

            yield return GroupHeader("Misc");
            yield return Entry("Terrain", viewModel.AddTerrainLayer);
            yield return Submenu(
                "Shapes",
                new MenuItem { Header = "Plane", Command = viewModel.AddPlane },
                new MenuItem { Header = "Cube", Command = viewModel.AddCube },
                new MenuItem { Header = "Sphere", Command = viewModel.AddNormalizedCubeSphereCommand },
                new MenuItem { Header = "Cylinder", Command = viewModel.AddCylinder });
            yield return Submenu(
                "Templates",
                new MenuItem { Header = "Player Entity", Command = viewModel.AddPlayerEntity });
        }

        /// <summary>
        /// Records each dockable's content by id so "Reset Layout" can rebuild the default
        /// layout tree with the original panel views.
        /// </summary>
        private void CaptureDockContents()
        {
            _dockContents.Clear();

            if (DockControl.Layout is not { } root)
            {
                return;
            }

            CollectDockContents(root);
        }

        private void CollectDockContents(IDockable dockable)
        {
            if (!string.IsNullOrEmpty(dockable.Id))
            {
                // Content is exposed by the concrete Tool/Document models, not by IDockable.
                object? content = dockable switch
                {
                    Tool tool => tool.Content,
                    Document document => document.Content,
                    _ => null
                };

                _dockContents[dockable.Id] = content;
            }

            if (dockable is IDock { VisibleDockables: { } visible })
            {
                foreach (IDockable child in visible)
                {
                    CollectDockContents(child);
                }
            }

            if (dockable is not IRootDock root)
            {
                return;
            }

            foreach (var list in new[] { root.LeftPinnedDockables, root.RightPinnedDockables, root.TopPinnedDockables, root.BottomPinnedDockables, root.HiddenDockables })
            {
                if (list == null)
                {
                    continue;
                }

                foreach (IDockable child in list)
                {
                    CollectDockContents(child);
                }
            }

            if (root.Windows != null)
            {
                foreach (IDockWindow? window in root.Windows)
                {
                    if (window?.Layout != null)
                    {
                        CollectDockContents(window.Layout);
                    }
                }
            }
        }

        private object? GetDockContent(string id)
        {
            return _dockContents.TryGetValue(id, out object? value) ? value : null;
        }

        /// <summary>
        /// Dynamic editor panels (edit asset, new physics shape, new scene, ...) each show as
        /// their own tab in the dynamic panels dock on the right - never as a separate floating
        /// window. Multiple can be open at once; closing one only removes its own tab.
        /// </summary>
        private void OnPanelServiceActivePanelChanged(object? sender, EventArgs e)
        {
            SyncDynamicPanels();
        }

        /// <summary>
        /// Reconciles the dynamic panels dock's tabs with <see cref="PanelService"/>'s open
        /// panels: adds a tab for anything newly opened, removes tabs for anything closed, and
        /// focuses the most recently opened panel's tab.
        /// </summary>
        private void SyncDynamicPanels()
        {
            if (_suppressPanelSync || _isClosing)
            {
                return;
            }

            if (DockControl.Factory is not FactoryBase factory)
            {
                return;
            }

            if (_dynamicPanelsDock == null && DockControl.Layout is { } layout)
            {
                _dynamicPanelsDock = FindDockableById(layout, "DynamicPanelsPane") as IDock;
            }

            if (_dynamicPanelsDock is not { } dock)
            {
                Logger.Log(LogLevel.Warning, "SyncDynamicPanels: could not locate the DynamicPanelsPane dock.");
                return;
            }

            IReadOnlyList<EditorPanelViewModel> openPanels = PanelService.Instance.OpenPanels;
            var openSet = new HashSet<EditorPanelViewModel>(openPanels);

            foreach (EditorPanelViewModel closed in _dynamicPanelTools.Keys.Where(p => !openSet.Contains(p)).ToList())
            {
                Tool tool = _dynamicPanelTools[closed];
                _dynamicPanelTools.Remove(closed);

                factory.RemoveDockable(tool, false);
            }

            Tool? activeTool = null;

            foreach (EditorPanelViewModel panel in openPanels)
            {
                if (!_dynamicPanelTools.TryGetValue(panel, out Tool? tool))
                {
                    tool = new Tool
                    {
                        Id = $"DynamicPanel_{panel.GetHashCode()}",
                        Title = panel.Title,
                        Content = CreatePanelContent(panel),
                        CanFloat = true
                    };

                    dock.VisibleDockables ??= factory.CreateList<IDockable>();
                    factory.AddDockable(dock, tool);
                    _dynamicPanelTools[panel] = tool;
                }
                else
                {
                    tool.Title = panel.Title;
                }

                activeTool = tool;
            }

            if (activeTool != null)
            {
                factory.SetActiveDockable(activeTool);
                factory.SetFocusedDockable(dock, activeTool);
            }
            else
            {
                dock.ActiveDockable = null;
            }
        }

        /// <summary>
        /// The user closed a dynamic panel's tab via its own chrome (rather than through
        /// whatever opened it) - release the matching view model so its OnClosed callback
        /// still runs, without re-triggering this sync recursively.
        /// </summary>
        private void OnFactoryDockableClosed(object? sender, DockableClosedEventArgs e)
        {
            if (_suppressPanelSync || _isClosing)
            {
                return;
            }

            EditorPanelViewModel? closed = null;

            foreach (var kvp in _dynamicPanelTools)
            {
                if (ReferenceEquals(kvp.Value, e.Dockable))
                {
                    closed = kvp.Key;
                    break;
                }
            }

            if (closed == null)
            {
                return;
            }

            _dynamicPanelTools.Remove(closed);

            _suppressPanelSync = true;
            try
            {
                PanelService.Instance.RemovePanel(closed);
            }
            finally
            {
                _suppressPanelSync = false;
            }
        }

        private static IDockable? FindDockableById(IDockable dockable, string id)
        {
            if (dockable.Id == id)
            {
                return dockable;
            }

            if (dockable is IDock { VisibleDockables: { } visible })
            {
                foreach (IDockable child in visible)
                {
                    if (FindDockableById(child, id) is { } found)
                    {
                        return found;
                    }
                }
            }

            return null;
        }

        private IList<IDockable> CreateDockList(params IDockable[] items)
        {
            if (DockControl.Factory is FactoryBase factory)
            {
                return factory.CreateList<IDockable>(items);
            }

            return new List<IDockable>(items);
        }

        /// <summary>
        /// Builds the default dock layout, mirroring the layout declared in MainWindow.axaml.
        /// </summary>
        private IDock BuildDefaultLayout()
        {
            var hierarchyTool = new Tool { Id = "SceneHierarchy", Title = "Scene", Content = GetDockContent("SceneHierarchy") };
            var inspectorTool = new Tool { Id = "Inspector", Title = "Inspector", Content = GetDockContent("Inspector") };
            var assetsTool = new Tool { Id = "ContentBrowser", Title = "Assets", Content = GetDockContent("ContentBrowser") };
            var consoleTool = new Tool { Id = "Console", Title = "Console", Content = GetDockContent("Console") };
            var sceneDocument = new Document
            {
                Id = "Scene",
                Title = "Viewport",
                Content = GetDockContent("Scene"),
                CanDrag = false,
                CanFloat = false,
                CanPin = false,
                CanClose = false
            };

            var hierarchyPane = new ToolDock { Id = "HierarchyPane", Alignment = Alignment.Left, ActiveDockable = hierarchyTool, VisibleDockables = CreateDockList(hierarchyTool) };
            var inspectorPane = new ToolDock { Id = "InspectorPane", Alignment = Alignment.Bottom, ActiveDockable = inspectorTool, VisibleDockables = CreateDockList(inspectorTool) };
            var leftDock = new ProportionalDock { Id = "Left", Orientation = Orientation.Vertical, Proportion = 0.25, VisibleDockables = CreateDockList(hierarchyPane, new ProportionalDockSplitter(), inspectorPane) };

            var documentsPane = new DocumentDock { Id = "Documents", ActiveDockable = sceneDocument, VisibleDockables = CreateDockList(sceneDocument) };
            var assetsPane = new ToolDock { Id = "AssetsPane", Alignment = Alignment.Left, ActiveDockable = assetsTool, VisibleDockables = CreateDockList(assetsTool) };
            var consolePane = new ToolDock { Id = "ConsolePane", Alignment = Alignment.Right, ActiveDockable = consoleTool, VisibleDockables = CreateDockList(consoleTool) };
            var bottomDock = new ProportionalDock { Id = "Bottom", Orientation = Orientation.Horizontal, Proportion = 0.3, VisibleDockables = CreateDockList(assetsPane, new ProportionalDockSplitter(), consolePane) };
            var centerDock = new ProportionalDock { Id = "Center", Orientation = Orientation.Vertical, VisibleDockables = CreateDockList(documentsPane, new ProportionalDockSplitter(), bottomDock) };

            var dynamicPanelsPane = new ToolDock { Id = "DynamicPanelsPane", Alignment = Alignment.Right, Proportion = 0.22, VisibleDockables = CreateDockList() };

            var mainLayout = new ProportionalDock
            {
                Id = "MainLayout",
                Orientation = Orientation.Horizontal,
                VisibleDockables = CreateDockList(leftDock, new ProportionalDockSplitter(), centerDock, new ProportionalDockSplitter(), dynamicPanelsPane)
            };

            return new RootDock { Id = "Root", IsCollapsable = false, DefaultDockable = mainLayout, VisibleDockables = CreateDockList(mainLayout) };
        }

        /// <summary>
        /// Restores the default dock layout. The XAML-declared layout instance is mutated
        /// in place as panels are moved, so reset rebuilds a fresh tree from code and
        /// re-assigns the captured panel contents.
        /// </summary>
        private void ResetLayout()
        {
            if (_dockContents.Count == 0)
            {
                CaptureDockContents();
            }

            if (DockControl.Layout is IDock oldLayout && oldLayout.Close.CanExecute(null))
            {
                oldLayout.Close.Execute(null);
            }

            // Any open dynamic panel tabs belong to the layout tree being torn down here -
            // close them so their view models don't linger with stale Tool references.
            _dynamicPanelsDock = null;
            _dynamicPanelTools.Clear();
            PanelService.Instance.ClosePanel();

            IDock root = BuildDefaultLayout();
            DockControl.Layout = root;

            // Mirror DockControl.OnAttachedToVisualTree: give the new root its window model.
            if (root is IRootDock rootDock && rootDock.Window == null && root.Factory is { } factory)
            {
                IDockWindow? windowModel = factory.CreateWindowFrom(rootDock);
                if (windowModel != null)
                {
                    windowModel.Layout = rootDock;
                    rootDock.Window = windowModel;
                }
            }
        }

        /// <summary>
        /// The World Settings editor is a standalone modal window (not a PanelService
        /// dynamic panel) - it is a long-lived properties surface similar in spirit to
        /// VS Code's settings editor, so it bypasses the dock layout and panel stack.
        /// </summary>
        private async void ShowWorldSettingsWindow()
        {
            if (_worldSettingsWindow != null)
            {
                _worldSettingsWindow.Activate();
                return;
            }

            var window = new WorldSettingsWindow
            {
                DataContext = new WorldSettingsPanelViewModel()
            };

            _worldSettingsWindow = window;
            window.Closed += (_, _) => _worldSettingsWindow = null;

            await window.ShowDialog(this);
        }

        private T? FindVisualChildByName<T>(string name) where T : Control
        {
            return this.GetVisualDescendants().OfType<T>().FirstOrDefault(c => c.Name == name);
        }

        private bool InitializeViewportControl()
        {
            if (_editorViewport != null)
                return true;

            _editorViewport = FindVisualChildByName<EditorViewportControl>("EditorViewportControl");
            _editorViewport?.Focus();
            return _editorViewport != null;
        }

        private bool IsPinnedPanelActive()
        {
            if (DockControl.Layout is not IRootDock root || root.ActiveDockable is not IDockable active)
            {
                return false;
            }

            return DockControl.Factory?.IsDockablePinned(active, root) == true;
        }

        private bool InitializeSceneFlyout()
        {
            if (_sceneDropDown != null)
                return true;

            _sceneDropDown = FindVisualChildByName<DropDownButton>("SceneDropDown");
            if (_sceneDropDown?.Flyout is Flyout flyout)
            {
                flyout.Opened += OnSceneFlyoutOpened;
                return true;
            }

            return _sceneDropDown != null;
        }

        private static void OnInspectorTextBoxGotFocus(object? sender, FocusChangedEventArgs e)
        {
            if (e.Source is TextBox { DataContext: InspectorPropertyViewModelBase vm })
            {
                vm.IsEditing = true;
            }
        }

        private static void OnInspectorTextBoxLostFocus(object? sender, FocusChangedEventArgs e)
        {
            if (e.Source is TextBox { DataContext: InspectorPropertyViewModelBase vm })
            {
                vm.IsEditing = false;
                vm.CommitValue();
            }
        }

        private static void OnInspectorTextBoxKeyDown(object? sender, KeyEventArgs e)
        {
            if (e.Key == Key.Return && e.Source is TextBox { DataContext: InspectorPropertyViewModelBase vm })
            {
                vm.CommitValue();
                e.Handled = true;
            }
        }

        private static void AttachInspectorFocusHandlers(TopLevel topLevel)
        {
            topLevel.AddHandler(InputElement.GotFocusEvent, OnInspectorTextBoxGotFocus, RoutingStrategies.Bubble);
            topLevel.AddHandler(InputElement.LostFocusEvent, OnInspectorTextBoxLostFocus, RoutingStrategies.Bubble);
            topLevel.AddHandler(InputElement.KeyDownEvent, OnInspectorTextBoxKeyDown, RoutingStrategies.Bubble);
        }

        private static void DetachInspectorFocusHandlers(TopLevel topLevel)
        {
            topLevel.RemoveHandler(InputElement.GotFocusEvent, OnInspectorTextBoxGotFocus);
            topLevel.RemoveHandler(InputElement.LostFocusEvent, OnInspectorTextBoxLostFocus);
            topLevel.RemoveHandler(InputElement.KeyDownEvent, OnInspectorTextBoxKeyDown);
        }

        private ContentControl CreatePanelContent(EditorPanelViewModel panel)
        {
            var content = new ContentControl { Content = panel };
            TopLevel? attachedFloatingTopLevel = null;

            content.AttachedToVisualTree += (_, _) =>
            {
                TopLevel? topLevel = TopLevel.GetTopLevel(content);

                if (topLevel == null || ReferenceEquals(topLevel, attachedFloatingTopLevel))
                {
                    return;
                }

                if (attachedFloatingTopLevel != null)
                {
                    DetachInspectorFocusHandlers(attachedFloatingTopLevel);
                    attachedFloatingTopLevel = null;
                }

                if (!ReferenceEquals(topLevel, this))
                {
                    AttachInspectorFocusHandlers(topLevel);
                    attachedFloatingTopLevel = topLevel;
                }
            };

            content.DetachedFromVisualTree += (_, _) =>
            {
                if (attachedFloatingTopLevel != null)
                {
                    DetachInspectorFocusHandlers(attachedFloatingTopLevel);
                    attachedFloatingTopLevel = null;
                }
            };

            return content;
        }

        private void OnNodeContextMenuOpened(object? sender, RoutedEventArgs e)
        {
            if (sender is not ContextMenu contextMenu)
            {
                return;
            }

            if (contextMenu.DataContext is NodeViewModel nodeViewModel)
            {
                nodeViewModel.RefreshActions();
                nodeViewModel.RefreshMoveToSceneTargets();
                nodeViewModel.RefreshCollisionState();

                if (DataContext is MainWindowViewModel mvm)
                {
                    nodeViewModel.RefreshSelectionContext(mvm.SceneHierarchy.SelectedNodes);
                }
            }

            if (contextMenu.Items.OfType<MenuItem>().FirstOrDefault(item => item.Tag as string == "AddSceneMenu") is { Items.Count: 0 } addMenuItem
                && MainWindowViewModel.Instance is { } mainViewModel)
            {
                foreach (Control entry in BuildAddSceneMenuEntries(mainViewModel))
                {
                    addMenuItem.Items.Add(entry);
                }
            }
        }

        private void OnAssetContextMenuOpened(object? sender, RoutedEventArgs e)
        {
            if ((sender as ContextMenu)?.DataContext is AssetObjectViewModel assetViewModel)
            {
                assetViewModel.RefreshActions();
            }
        }

        private void OnToolSettingsMenuOpened(object? sender, RoutedEventArgs e)
        {
            if (DataContext is MainWindowViewModel vm)
            {
                vm.RefreshDebugOverlayToggleStates();
            }
        }

        /// <summary>
        /// Brings the selected asset into view. The content browser selects assets programmatically -
        /// a newly created one, for instance - and the selection is otherwise invisible if the item
        /// sits below the scrolled region.
        /// </summary>
        private void OnContentBrowserSelectionChanged(object? sender, SelectionChangedEventArgs e)
        {
            if (sender is not ListBox listBox || listBox.SelectedItem is not AssetObjectViewModel selected)
            {
                return;
            }

            // The list was likely just repopulated, so the containers are not laid out yet and there
            // is nothing to scroll to until the next layout pass.
            Dispatcher.UIThread.Post(
                () =>
                {
                    if (ReferenceEquals(listBox.SelectedItem, selected))
                    {
                        listBox.ScrollIntoView(selected);
                    }
                },
                DispatcherPriority.Loaded);
        }

        private void OnRenameNodeMenuItemClick(object? sender, RoutedEventArgs e)
        {
            if ((sender as MenuItem)?.DataContext is not NodeViewModel nodeViewModel)
                return;

            nodeViewModel.BeginRename();

            Dispatcher.UIThread.Post(() => FocusNodeRenameTextBox(nodeViewModel), DispatcherPriority.Loaded);
        }

        private void OnMoveToGrandparentMenuItemClick(object? sender, RoutedEventArgs e)
        {
            if ((sender as MenuItem)?.DataContext is not NodeViewModel nodeViewModel)
                return;

            NodeViewModel? grandparent = nodeViewModel.Parent?.Parent;
            if (grandparent == null)
                return;

            var vm = DataContext as MainWindowViewModel;
            vm?.SceneHierarchy.ReparentNode(nodeViewModel, grandparent);
        }

        private void FocusNodeRenameTextBox(NodeViewModel nodeViewModel)
        {
            if (_sceneTree == null)
                return;

            TextBox? textBox = _sceneTree.GetVisualDescendants()
                .OfType<TextBox>()
                .FirstOrDefault(t => t.DataContext == nodeViewModel);

            if (textBox != null)
            {
                textBox.Focus();
                textBox.SelectAll();
            }
        }

        private void OnNodeRenameTextBoxKeyDown(object? sender, KeyEventArgs e)
        {
            if (e.Source is not TextBox { DataContext: NodeViewModel nodeViewModel } textBox)
                return;

            if (e.Key == Key.Return)
            {
                CommitNodeRename(nodeViewModel, textBox.Text ?? string.Empty);
                e.Handled = true;
            }
            else if (e.Key == Key.Escape)
            {
                nodeViewModel.CancelRename();
                e.Handled = true;
            }
        }

        private void OnNodeRenameTextBoxLostFocus(object? sender, FocusChangedEventArgs e)
        {
            if (e.Source is TextBox { DataContext: NodeViewModel nodeViewModel } textBox && nodeViewModel.IsEditingName)
            {
                CommitNodeRename(nodeViewModel, textBox.Text ?? string.Empty);
            }
        }

        private void CommitNodeRename(NodeViewModel nodeViewModel, string newName)
        {
            nodeViewModel.IsEditingName = false;

            string trimmedName = newName.Trim();

            if (string.IsNullOrEmpty(trimmedName) || trimmedName == nodeViewModel.Name)
                return;

            if (DataContext is MainWindowViewModel mvm)
            {
                mvm.SceneHierarchy.RenameNode(nodeViewModel, trimmedName);
            }
        }

        private bool SetupSceneHierarchyDragDrop()
        {
            if (_sceneTree != null)
                return true;

            _sceneTree = FindVisualChildByName<TreeView>("SceneHierarchyTreeView");
            if (_sceneTree == null)
                return false;

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
            return true;
        }

        private void OnSceneTreePointerPressed(object? sender, PointerPressedEventArgs e)
        {
            var point = e.GetCurrentPoint(sender as Visual);

            if (point.Properties.IsRightButtonPressed)
            {
                var nodeVm = FindNodeViewModelInEventSource(e.Source);
                if (nodeVm != null
                    && DataContext is MainWindowViewModel mvm
                    && !mvm.SceneHierarchy.SelectedNodes.Contains(nodeVm))
                {
                    // Volumes get right-clicked to fit them around the current selection, so keep that selection.
                    // Handling the press stops the TreeViewItem selecting; the context menu opens on release regardless.
                    if (nodeVm.IsVolume)
                    {
                        e.Handled = true;
                        return;
                    }

                    mvm.SelectSingleNodeExclusive(nodeVm);

                    _suppressTreeSelectionHandling = true;
                    try
                    {
                        _sceneTree.SelectedItem = nodeVm;
                    }
                    finally
                    {
                        _suppressTreeSelectionHandling = false;
                    }
                }

                return;
            }

            if (point.Properties.IsLeftButtonPressed)
            {
                var keyModifiers = e.KeyModifiers;

                if ((keyModifiers & KeyModifiers.Shift) != 0)
                {
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
                _dragPressedArgs = e;
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
                _dragPressedArgs = null;
                return;
            }

            var pos = e.GetPosition(sender as Visual);
            var delta = pos - _dragStartPoint;

            // Only start a drag after a small movement threshold to avoid stealing normal clicks.
            if (Math.Abs(delta.X) < 5 && Math.Abs(delta.Y) < 5)
                return;

            if (_dragPressedArgs == null)
            {
                _dragCandidate = null;
                return;
            }

            _isDragging = true;
            var candidate = _dragCandidate;

            e.Pointer.Capture(_sceneTree);

            var data = new DataTransfer();
            data.Add(DataTransferItem.Create(NodeViewModelDragFormat, candidate));

            try
            {
                await DragDrop.DoDragDropAsync(_dragPressedArgs, data, DragDropEffects.Move);
            }
            catch (Exception ex) when (ex is System.Runtime.InteropServices.COMException)
            {
                // DoDragDrop can throw on Windows if the drag is cancelled externally
                // or the pointer state is unexpected; treat as a cancelled drag.
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Error, $"Scene hierarchy drag-and-drop failed: {ex}");
            }
            finally
            {
                EndDrag();
            }
        }

        private void OnSceneTreePointerReleased(object? sender, PointerReleasedEventArgs e)
        {
            if (!_isDragging)
            {
                _dragCandidate = null;
                _dragPressedArgs = null;
            }
        }

        private void OnSceneTreeDragOver(object? sender, DragEventArgs e)
        {
            if (e.DataTransfer.Contains(AssetDragFormat))
            {
                e.DragEffects = DragDropEffects.Copy;

                var vm = DataContext as MainWindowViewModel;
                vm?.SceneHierarchy.SetDropTarget(null);
                UpdateAutoScroll(e.GetPosition(_sceneTree));
                e.Handled = true;
                return;
            }

            if (!e.DataTransfer.Contains(NodeViewModelDragFormat))
            {
                e.DragEffects = DragDropEffects.None;
                return;
            }

            var vm_node = DataContext as MainWindowViewModel;
            var dragged = e.DataTransfer.TryGetValue(NodeViewModelDragFormat);
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
            if (e.DataTransfer.Contains(AssetDragFormat))
            {
                EndDrag();

                var vm = DataContext as MainWindowViewModel;
                if (vm != null)
                {
                    var assetData = e.DataTransfer.TryGetValue(AssetDragFormat);
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

            if (!e.DataTransfer.Contains(NodeViewModelDragFormat))
                return;

            var dragged = e.DataTransfer.TryGetValue(NodeViewModelDragFormat);
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
            _dragPressedArgs?.Pointer.Capture(null);

            _isDragging = false;
            _dragCandidate = null;
            _dragPressedArgs = null;

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

            if (_suppressTreeSelectionHandling)
            {
                mvm.SceneHierarchy.SetSuppressSelectionNotifications(false);
                return;
            }

            var added = e.AddedItems.OfType<NodeViewModel>().ToList();
            var removed = e.RemovedItems.OfType<NodeViewModel>().ToList();

            // Un-suppress after SelectedItem binding fired (suppressed)
            mvm.SceneHierarchy.SetSuppressSelectionNotifications(false);

            mvm.HandleTreeSelectionChanged(added, removed);
        }

        private void UpdateAutoScroll(Point posRelativeToTree)
        {
            if (_sceneTree == null)
                return;

            _sceneTreeScrollViewer ??= _sceneTree.GetVisualDescendants().OfType<ScrollViewer>().FirstOrDefault();

            if (_sceneTreeScrollViewer == null)
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

                // Also step immediately on this DragOver rather than waiting for the next timer
                // tick, since DispatcherTimer ticks can be delayed while the OS-driven drag loop
                // is pumping messages.
                ApplyAutoScrollStep();
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

            ApplyAutoScrollStep();
        }

        private void ApplyAutoScrollStep()
        {
            if (_sceneTreeScrollViewer == null || _autoScrollDelta == 0)
                return;

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

        private bool SetupContentBrowserDragDrop()
        {
            if (_contentBrowserAssetList != null)
                return true;

            _contentBrowserAssetList = FindVisualChildByName<ListBox>("ContentBrowserAssetList");
            if (_contentBrowserAssetList == null)
                return false;

            DragDrop.SetAllowDrop(_contentBrowserAssetList, true);

            _contentBrowserAssetList.AddHandler(InputElement.PointerPressedEvent, OnContentBrowserPointerPressed, RoutingStrategies.Tunnel);
            _contentBrowserAssetList.AddHandler(InputElement.PointerMovedEvent, OnContentBrowserPointerMoved, RoutingStrategies.Tunnel);
            return true;
        }

        private void OnContentBrowserPointerPressed(object? sender, PointerPressedEventArgs e)
        {
            var point = e.GetCurrentPoint(sender as Visual);

            if (point.Properties.IsLeftButtonPressed)
            {
                _assetDragCandidate = FindAssetViewModelInEventSource(e.Source);
                _assetDragPressedArgs = e;
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
                _assetDragPressedArgs = null;
                return;
            }

            var pos = e.GetPosition(sender as Visual);
            var delta = pos - _assetDragStartPoint;

            if (Math.Abs(delta.X) < 5 && Math.Abs(delta.Y) < 5)
                return;

            if (_assetDragPressedArgs == null)
            {
                _assetDragCandidate = null;
                return;
            }

            _isDraggingAsset = true;
            var candidate = _assetDragCandidate;

            var data = new DataTransfer();
            data.Add(DataTransferItem.Create(AssetDragFormat, $"{candidate.Bucket?.BucketIndex ?? 0}|{candidate.AssetDesc.Name}"));

            try
            {
                await DragDrop.DoDragDropAsync(_assetDragPressedArgs, data, DragDropEffects.Copy);
            }
            catch (Exception ex) when (ex is System.Runtime.InteropServices.COMException)
            {
            }
            finally
            {
                _isDraggingAsset = false;
                _assetDragCandidate = null;
                _assetDragPressedArgs = null;
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

        private bool SetupViewportDropTarget()
        {
            if (_viewportDropTarget != null)
                return true;

            _viewportDropTarget = FindVisualChildByName<Border>("ViewportDropTarget");
            if (_viewportDropTarget == null)
                return false;

            DragDrop.SetAllowDrop(_viewportDropTarget, true);
            _viewportDropTarget.AddHandler(DragDrop.DragOverEvent, OnViewportDragOver);
            _viewportDropTarget.AddHandler(DragDrop.DropEvent, OnViewportDrop);
            return true;
        }

        private void OnViewportDragOver(object? sender, DragEventArgs e)
        {
            if (e.DataTransfer.Contains(AssetDragFormat))
            {
                e.DragEffects = DragDropEffects.Copy;
                e.Handled = true;
            }
        }

        private void OnViewportDrop(object? sender, DragEventArgs e)
        {
            if (!e.DataTransfer.Contains(AssetDragFormat))
                return;

            var assetData = e.DataTransfer.TryGetValue(AssetDragFormat);
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

        protected override void OnClosing(WindowClosingEventArgs e)
        {
            _isClosing = true;

            // Disable main thread loop until this is done
            // This should prevent MainThread::Update() from being triggered by avalonia
            // directly after clicking any of the messagebox buttons
            EngineManager.DisableMainLoop = true;

            try
            {
                MainWindowViewModel vm = DataContext as MainWindowViewModel;
                EditorProject? project = EngineManager.CurrentProject;

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

                if (e.Cancel)
                {
                    // @TODO Implement windows-only hack here for removing the parent hwnd -- otherwise we get that nasty crash when closing the editor
                    // we'll need to remove the parent hwnd on close, add it back in here.
                }
                else
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

        private bool _flyoutHandlerAttached;

        private void OnSceneFlyoutOpened(object? sender, EventArgs e)
        {
            if (_flyoutHandlerAttached
                || _sceneDropDown is not { } dropDown
                || dropDown.Flyout is not Flyout flyout
                || flyout.Content is not StackPanel panel)
            {
                return;
            }

            panel.AddHandler(
                InputElement.PointerReleasedEvent,
                OnSceneFlyoutPointerReleased,
                RoutingStrategies.Bubble,
                handledEventsToo: true);

            _flyoutHandlerAttached = true;
        }

        private void OnSceneFlyoutPointerReleased(object? sender, Avalonia.Input.PointerReleasedEventArgs e)
        {
            _sceneDropDown?.Flyout?.Hide();
        }

        protected override void OnKeyDown(Avalonia.Input.KeyEventArgs e)
        {
            base.OnKeyDown(e);

            if (e.Handled || DataContext is not MainWindowViewModel vm)
            {
                return;
            }

            // Let text editing controls keep their native editing shortcuts
            // (undo, redo, clipboard, select all) instead of running scene commands.
            for (Visual? visual = e.Source as Visual; visual != null; visual = visual.GetVisualParent())
            {
                if (visual is TextBox)
                {
                    return;
                }
            }

            KeyModifiers modifiers = e.KeyModifiers;
            bool ctrl = modifiers.HasFlag(KeyModifiers.Control) || modifiers.HasFlag(KeyModifiers.Meta);
            bool shift = modifiers.HasFlag(KeyModifiers.Shift);
            bool alt = modifiers.HasFlag(KeyModifiers.Alt);

            if (alt)
            {
                return;
            }

            if (!ctrl)
            {
                if (e.Key == Key.Delete && vm.CanCopy)
                {
                    vm.Delete.Execute(null);
                    e.Handled = true;
                }

                return;
            }

            switch (e.Key)
            {
                case Key.Z:
                    if (shift ? vm.CanRedo : vm.CanUndo)
                    {
                        (shift ? vm.Redo : vm.Undo).Execute(null);
                        e.Handled = true;
                    }
                    break;

                case Key.Y:
                    if (vm.CanRedo)
                    {
                        vm.Redo.Execute(null);
                        e.Handled = true;
                    }
                    break;

                case Key.C:
                    if (vm.CanCopy)
                    {
                        vm.Copy.Execute(null);
                        e.Handled = true;
                    }
                    break;

                case Key.V:
                    if (vm.CanPaste)
                    {
                        vm.Paste.Execute(null);
                        e.Handled = true;
                    }
                    break;

                case Key.A:
                    vm.SelectAll.Execute(null);
                    e.Handled = true;
                    break;

                case Key.S:
                    if (!vm.IsSimulating)
                    {
                        (shift ? vm.SaveProjectAs : vm.SaveProject).Execute(null);
                        e.Handled = true;
                    }
                    break;
            }
        }
    }
}
