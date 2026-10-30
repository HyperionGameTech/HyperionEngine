using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Platform;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Input.Platform;
using Avalonia.Interactivity;
using Avalonia.Media;
using Avalonia.Platform;
using Avalonia.Platform.Storage;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Dock.Model;
using Dock.Model.Core;
using Dock.Model.Controls;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using System.Windows.Input;
using Hyperion;
using Hyperion.Editor.Views;
using Hyperion.Editor.ViewModels;
using Hyperion.Editor.Services;
using Hyperion.Editor.Commands;
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
        private const bool IsRenderingOnMainThread = true;

        private static readonly DataFormat<NodeViewModel> NodeViewModelDragFormat =
            DataFormat.CreateInProcessFormat<NodeViewModel>("hyperion-nodeviewmodel");

        private static readonly DataFormat<string> AssetDragFormat =
            DataFormat.CreateStringApplicationFormat("hyperion-asset");

        private static readonly DataFormat<string> EntityAssetDragFormat =
            DataFormat.CreateStringApplicationFormat("hyperion-entity-asset");

        // what can be applied to an Entity via D&D
        private static readonly HashSet<string> EntityAssetTypeNames
            = ["Script", "ScriptAsset", "Material", "Texture"];

        private static readonly object AssetCreationEntryTag = new();
        
        private NodeViewModel? _dragCandidate;
        private PointerPressedEventArgs? _dragPressedArgs;
        private Point _dragStartPoint;
        private bool _isDragging;
        private bool _suppressTreeSelectionHandling;
        private bool _syncingTreeSelection;
        private NodeViewModel? _pendingSingleSelect;

        // Content browser drag tracking
        private ListBox? _contentBrowserAssetList;
        private TreeView? _contentBrowserFolderTree;
        private ItemsControl? _contentBrowserPathBar;
        private object? _browserDragCandidate;
        private object? _pendingBrowserSingleSelect;
        private PointerPressedEventArgs? _browserDragPressedArgs;
        private Point _browserDragStartPoint;
        private bool _isDraggingBrowserItems;
        private object? _renameClickCandidate;
        private DispatcherTimer? _renameClickTimer;
        private AssetFolderViewModel? _browserDropTarget;
        private DateTime _browserDropHoverStart;

        // Viewport drop tracking
        private Border? _viewportDropTarget;
        private NodeViewModel? _sceneTreeAssetDropTarget;

        // Drop-indicator tracking and auto-scroll
        private TreeView? _sceneTree;
        private ScrollViewer? _sceneTreeScrollViewer;
        private DispatcherTimer? _autoScrollTimer;
        private double _autoScrollDelta;
        private const double AutoScrollZone = 36;  // px from edge that triggers scroll
        private const double AutoScrollSpeed = 10; // px per timer tick (~50 ms)

        private EditorViewportControl? _editorViewport;
        private WorldSettingsWindow? _worldSettingsWindow;
        private ProjectSettingsWindow? _projectSettingsWindow;
        private PluginsWindow? _pluginsWindow;
        private PackagingSettingsWindow? _packagingSettingsWindow;
        private IDock? _dynamicPanelsDock;
        private readonly Dictionary<EditorPanelViewModel, Tool> _dynamicPanelTools = new();
        private readonly Dictionary<EditorDocumentViewModel, IDockable> _pluginDocuments = new();
        private int _frameCounter;
        private bool _viewportNativeHidden;
        private bool _isClosing;
        private int _mainThreadUpdateDepth;
        private bool _closeDeferred;
        private bool _suppressPanelSync;
        private readonly Dictionary<string, object?> _dockContents = new Dictionary<string, object?>();
        private readonly Dictionary<EditorViewportType, Control> _orthoViewportContents = new();

        private enum ViewportLayout
        {
            Single,
            Grid,      // Top / Front over Side / Perspective
            OneOverThree
        }

        private ViewportLayout _viewportLayout = ViewportLayout.Single;

        private enum ToolbarDensity
        {
            Full,
            Compact,   // text labels hidden
            Condensed, // project and edit groups folded into the overflow menu
            Wrapped    // last resort. center tool group on its own row
        }

        private ToolbarDensity _toolbarDensity;
        private double _toolbarWidthBeforeStep;
        // Width each density saves over the one before it, measured when it is first applied
        private readonly double[] _toolbarSavedWidths = new double[4];
        private const double ToolbarGroupGap = 8;
        private const double ToolbarDensityHysteresis = 12;

        public MainWindow()
        {
            var viewModel = new MainWindowViewModel();
            MainWindowViewModel.Instance = viewModel;
            DataContext = viewModel;

            InitializeComponent();

            viewModel.SceneHierarchy.PropertyChanged += OnSceneHierarchyPropertyChanged;

            EditorPluginService.Instance.Documents.CollectionChanged += (_, e) =>
            {
                foreach (EditorDocumentViewModel document in e.NewItems?.OfType<EditorDocumentViewModel>() ?? [])
                {
                    document.PropertyChanged += OnPluginDocumentPropertyChanged;
                }

                foreach (EditorDocumentViewModel document in e.OldItems?.OfType<EditorDocumentViewModel>() ?? [])
                {
                    document.PropertyChanged -= OnPluginDocumentPropertyChanged;
                }

                SyncPluginDocuments();
            };

            foreach (EditorDocumentViewModel document in EditorPluginService.Instance.Documents)
            {
                document.PropertyChanged += OnPluginDocumentPropertyChanged;
            }

            foreach (Control entry in BuildAddSceneMenuEntries(viewModel))
            {
                AddSceneContextMenu.Items.Add(entry);
            }

            PanelService.Instance.ActivePanelChanged += OnPanelServiceActivePanelChanged;
            ContentBrowserViewModel.BringToFrontRequested += OnContentBrowserBringToFrontRequested;
            ContentBrowserViewModel.FolderRenameRequested += OnContentBrowserFolderRenameRequested;
            ContentBrowserViewModel.AssetRenameRequested += OnContentBrowserAssetRenameRequested;

            EngineManager.ViewportLayoutRequested += layout => Dispatcher.UIThread.Post(() => ApplyViewportLayout((ViewportLayout)layout));

            // Detect a dynamic panel's tab being closed via its own [x] button so the
            // matching view model's OnClosed callback still runs.
            DockControl.Factory.DockableClosed += OnFactoryDockableClosed;

            AddHandler(InputElement.GotFocusEvent, OnInspectorTextBoxGotFocus, RoutingStrategies.Bubble);
            AddHandler(InputElement.LostFocusEvent, OnInspectorTextBoxLostFocus, RoutingStrategies.Bubble);
            AddHandler(InputElement.KeyDownEvent, OnInspectorTextBoxKeyDown, RoutingStrategies.Bubble);

            AddHandler(InputElement.LostFocusEvent, OnNodeRenameTextBoxLostFocus, RoutingStrategies.Bubble);
            AddHandler(InputElement.KeyDownEvent, OnNodeRenameTextBoxKeyDown, RoutingStrategies.Bubble);

            ToolbarGrid.LayoutUpdated += OnToolbarLayoutUpdated;

            Opened += OnWindowOpened;
        }

        private void OnToolbarLayoutUpdated(object? sender, EventArgs e)
        {
            double availableWidth = ToolbarGrid.Bounds.Width;

            if (availableWidth <= 0)
            {
                return;
            }

            double contentWidth = GetToolbarGroupWidth(ToolbarLeftGroup)
                + GetToolbarGroupWidth(ToolbarCenterGroup)
                + GetToolbarGroupWidth(ToolbarRightGroup)
                + ToolbarGroupGap * 2;

            int level = (int)_toolbarDensity;

            // First layout since stepping down, so the difference is what the step saved
            if (double.IsNaN(_toolbarSavedWidths[level]))
            {
                _toolbarSavedWidths[level] = Math.Max(_toolbarWidthBeforeStep - contentWidth, 0);
            }

            if (contentWidth > availableWidth)
            {
                if (_toolbarDensity != ToolbarDensity.Wrapped)
                {
                    _toolbarWidthBeforeStep = contentWidth;
                    _toolbarSavedWidths[level + 1] = double.NaN;
                    SetToolbarDensity(_toolbarDensity + 1);
                }
            }
            else if (level > 0 && contentWidth + _toolbarSavedWidths[level] + ToolbarDensityHysteresis <= availableWidth)
            {
                SetToolbarDensity(_toolbarDensity - 1);
            }
        }

        private void OnToolbarOverflowMenuOpened(object? sender, RoutedEventArgs e)
        {
            if (DataContext is not MainWindowViewModel viewModel)
            {
                return;
            }

            ToolbarOverflowOpenRecentMenuItem.Items.Clear();

            foreach (Control entry in BuildRecentProjectMenuEntries(viewModel))
            {
                ToolbarOverflowOpenRecentMenuItem.Items.Add(entry);
            }
        }

        private void SetToolbarDensity(ToolbarDensity density)
        {
            _toolbarDensity = density;

            bool wrapped = density == ToolbarDensity.Wrapped;

            Toolbar.Classes.Set("compact", density >= ToolbarDensity.Compact);
            Toolbar.Classes.Set("condensed", density >= ToolbarDensity.Condensed);

            Grid.SetRow(ToolbarCenterGroup, wrapped ? 1 : 0);
            Grid.SetColumn(ToolbarCenterGroup, wrapped ? 0 : 1);
            Grid.SetColumnSpan(ToolbarCenterGroup, wrapped ? 3 : 1);
            ToolbarCenterGroup.Margin = wrapped ? new Thickness(0, 4, 0, 0) : default;
        }

        private static double GetToolbarGroupWidth(StackPanel group)
        {
            double width = 0;
            int visibleCount = 0;

            foreach (Control child in group.Children)
            {
                if (!child.IsVisible)
                {
                    continue;
                }

                width += child.DesiredSize.Width;
                visibleCount++;
            }

            return width + group.Spacing * Math.Max(visibleCount - 1, 0);
        }

        private void OnWindowOpened(object? sender, EventArgs e)
        {
            if (IsRenderingOnMainThread)
            {
                var topLevel = TopLevel.GetTopLevel(this);
                topLevel?.RequestAnimationFrame(OnFrame);
            }

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
            _viewportLayout = ViewportLayout.Single;

            ResetLayout();
        }

        private void OnViewportLayoutSingleClick(object? sender, RoutedEventArgs e) => ApplyViewportLayout(ViewportLayout.Single);
        private void OnViewportLayoutGridClick(object? sender, RoutedEventArgs e) => ApplyViewportLayout(ViewportLayout.Grid);
        private void OnViewportLayoutOneOverThreeClick(object? sender, RoutedEventArgs e) => ApplyViewportLayout(ViewportLayout.OneOverThree);

        private void ApplyViewportLayout(ViewportLayout layout)
        {
            if (layout == _viewportLayout)
            {
                return;
            }

            _viewportLayout = layout;

            ResetLayout();
        }

        private void OnWorldSettingsClick(object? sender, RoutedEventArgs e)
        {
            ShowWorldSettingsWindow();
        }

        private void OnProjectSettingsClick(object? sender, RoutedEventArgs e)
        {
            ShowProjectSettingsWindow();
        }

        private void OnPluginsClick(object? sender, RoutedEventArgs e)
        {
            ShowPluginsWindow();
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
                new MenuItem { Header = "Cylinder", Command = viewModel.AddCylinderCommand });
            yield return BuildTemplatesSubmenu(viewModel);
        }

        private static readonly object UserTemplateEntryTag = new object();

        private static MenuItem BuildTemplatesSubmenu(MainWindowViewModel viewModel)
        {
            var templatesMenuItem = new MenuItem { Header = "Templates" };

            templatesMenuItem.Items.Add(new MenuItem { Header = "Third Person Player", Command = viewModel.AddThirdPersonPlayer });
            templatesMenuItem.Items.Add(new MenuItem { Header = "First Person Player", Command = viewModel.AddFirstPersonPlayer });
            templatesMenuItem.Items.Add(new Separator());
            templatesMenuItem.Items.Add(new MenuItem { Header = "Open Templates Folder", Command = viewModel.OpenTemplatesFolder });

            templatesMenuItem.SubmenuOpened += (sender, e) =>
            {
                if (ReferenceEquals(e.Source, sender))
                {
                    RefreshUserTemplateEntries(templatesMenuItem, viewModel);
                }
            };

            return templatesMenuItem;
        }

        private static void RefreshUserTemplateEntries(MenuItem templatesMenuItem, MainWindowViewModel viewModel)
        {
            foreach (Control staleEntry in templatesMenuItem.Items.OfType<Control>().Where(item => ReferenceEquals(item.Tag, UserTemplateEntryTag)).ToList())
            {
                templatesMenuItem.Items.Remove(staleEntry);
            }

            List<string> templateNames = viewModel.GetTemplateNames();

            if (templateNames.Count == 0)
            {
                return;
            }

            int insertIndex = 1;

            templatesMenuItem.Items.Insert(insertIndex++, new Separator { Tag = UserTemplateEntryTag });

            foreach (string templateName in templateNames)
            {
                templatesMenuItem.Items.Insert(insertIndex++, new MenuItem
                {
                    Header = templateName.Replace("_", "__"),
                    Command = viewModel.AddTemplate,
                    CommandParameter = templateName,
                    Tag = UserTemplateEntryTag,
                });
            }
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

        private void OnContentBrowserBringToFrontRequested()
        {
            if (DockControl.Layout is not { } layout || DockControl.Factory is not FactoryBase factory)
            {
                return;
            }

            if (FindDockableById(layout, "ContentBrowser") is not { } contentBrowserTool)
            {
                return;
            }

            factory.SetActiveDockable(contentBrowserTool);

            if (contentBrowserTool.Owner is IDock ownerDock)
            {
                factory.SetFocusedDockable(ownerDock, contentBrowserTool);
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
        private Control GetOrCreateOrthoViewportContent(EditorViewportType viewportType)
        {
            if (_orthoViewportContents.TryGetValue(viewportType, out Control? content))
            {
                return content;
            }

            var border = new Border
            {
                BorderThickness = new Thickness(0),
                Child = new EditorViewportControl
                {
                    ViewportType = viewportType,
                    HorizontalAlignment = Avalonia.Layout.HorizontalAlignment.Stretch,
                    VerticalAlignment = Avalonia.Layout.VerticalAlignment.Stretch
                }
            };

            border.Bind(Border.BackgroundProperty, border.GetResourceObservable("Brush.Background"));

            content = new Grid { Children = { border } };

            _orthoViewportContents[viewportType] = content;

            return content;
        }

        private DocumentDock CreateOrthoViewportPane(EditorViewportType viewportType)
        {
            var document = new Document
            {
                Id = $"Viewport.{viewportType}",
                Title = viewportType.ToString(),
                Content = GetOrCreateOrthoViewportContent(viewportType),
                CanFloat = false,
                CanPin = false,
                CanClose = false
            };

            return new DocumentDock { Id = $"ViewportPane.{viewportType}", CanCreateDocument = false, ActiveDockable = document, VisibleDockables = CreateDockList(document) };
        }

        /// <summary>
        /// The viewport area of the center column for the current viewport layout. The perspective
        /// viewport always stays in the "Documents" pane.
        /// </summary>
        private IDockable BuildViewportArea(DocumentDock perspectivePane)
        {
            switch (_viewportLayout)
            {
            case ViewportLayout.Grid:
            {
                var topRow = new ProportionalDock
                {
                    Orientation = Orientation.Horizontal,
                    VisibleDockables = CreateDockList(CreateOrthoViewportPane(EditorViewportType.Top), new ProportionalDockSplitter(), CreateOrthoViewportPane(EditorViewportType.Front))
                };

                var bottomRow = new ProportionalDock
                {
                    Orientation = Orientation.Horizontal,
                    VisibleDockables = CreateDockList(CreateOrthoViewportPane(EditorViewportType.Side), new ProportionalDockSplitter(), perspectivePane)
                };

                return new ProportionalDock { Id = "Viewports", Orientation = Orientation.Vertical, VisibleDockables = CreateDockList(topRow, new ProportionalDockSplitter(), bottomRow) };
            }
            case ViewportLayout.OneOverThree:
            {
                perspectivePane.Proportion = 0.65;

                var orthoRow = new ProportionalDock
                {
                    Orientation = Orientation.Horizontal,
                    Proportion = 0.35,
                    VisibleDockables = CreateDockList(
                        CreateOrthoViewportPane(EditorViewportType.Top), new ProportionalDockSplitter(),
                        CreateOrthoViewportPane(EditorViewportType.Front), new ProportionalDockSplitter(),
                        CreateOrthoViewportPane(EditorViewportType.Side))
                };

                return new ProportionalDock { Id = "Viewports", Orientation = Orientation.Vertical, VisibleDockables = CreateDockList(perspectivePane, new ProportionalDockSplitter(), orthoRow) };
            }
            default:
                return perspectivePane;
            }
        }

        private IDock BuildDefaultLayout()
        {
            var hierarchyTool = new Tool { Id = "SceneHierarchy", Title = "Scene", Content = GetDockContent("SceneHierarchy") };
            var inspectorTool = new Tool { Id = "Inspector", Title = "Inspector", Content = GetDockContent("Inspector") };
            var assetsTool = new Tool { Id = "ContentBrowser", Title = "Assets", Content = GetDockContent("ContentBrowser") };
            var consoleTool = new Tool { Id = "Console", Title = "Console", Content = GetDockContent("Console") };
            var sceneDocument = new Document
            {
                Id = "Scene",
                Title = _viewportLayout == ViewportLayout.Single ? "Viewport" : "Perspective",
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
            var centerDock = new ProportionalDock { Id = "Center", Orientation = Orientation.Vertical, VisibleDockables = CreateDockList(BuildViewportArea(documentsPane), new ProportionalDockSplitter(), bottomDock) };

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
            // No sync while closing, or it would find and cache the old layout's DynamicPanelsPane
            _dynamicPanelsDock = null;
            _dynamicPanelTools.Clear();

            _suppressPanelSync = true;

            try
            {
                PanelService.Instance.ClosePanel();
            }
            finally
            {
                _suppressPanelSync = false;
            }

            _pluginDocuments.Clear();
            _savedBottomProportions = null;

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

            // Dock builds each Tool's content from its template, so the new layout gets new controls -
            // drop the cached ones and locate them again once the dock has laid out.
            ReleaseSceneHierarchyTree();
            _contentBrowserAssetList = null;
            _contentBrowserFolderTree = null;
            _contentBrowserPathBar = null;
            _viewportDropTarget = null;
            _editorViewport = null;

            DockControl.LayoutUpdated -= OnDockControlLayoutUpdated;
            DockControl.LayoutUpdated += OnDockControlLayoutUpdated;

            SyncPluginDocuments();
        }

        private void OnPluginDocumentPropertyChanged(object? sender, System.ComponentModel.PropertyChangedEventArgs e)
        {
            if (sender is not EditorDocumentViewModel document)
            {
                return;
            }

            if (e.PropertyName == nameof(EditorDocumentViewModel.Title) && _pluginDocuments.TryGetValue(document, out IDockable? dockable))
            {
                dockable.Title = document.Title;
            }
            else if (e.PropertyName == nameof(EditorDocumentViewModel.IsOpen))
            {
                SyncPluginDocuments();
            }
        }

        // a plugin document is a dock tab while it's open; its content is the view the plugin registered for it
        private void SyncPluginDocuments()
        {
            if (_isClosing || DockControl.Factory is not FactoryBase factory || DockControl.Layout is not { } layout)
            {
                return;
            }

            foreach ((EditorDocumentViewModel document, IDockable dockable) in _pluginDocuments.ToList())
            {
                if (!document.IsOpen || !EditorPluginService.Instance.Documents.Contains(document))
                {
                    factory.RemoveDockable(dockable, false);
                    _pluginDocuments.Remove(document);
                }
            }

            if (!_pluginDocuments.Keys.Any(document => document.DocksAtBottom))
            {
                RestoreBottomDock();
            }

            foreach (EditorDocumentViewModel document in EditorPluginService.Instance.Documents)
            {
                if (!document.IsOpen || _pluginDocuments.ContainsKey(document))
                {
                    continue;
                }

                IDock? targetDock = null;

                if (document.DocksAtBottom)
                {
                    targetDock = FindDockableById(layout, "AssetsPane") as IDock;
                }

                targetDock ??= FindDockableById(layout, "Documents") as IDock;

                if (targetDock == null)
                {
                    return;
                }

                Control? content = EditorPluginService.Instance.ViewLocator.Build(document);

                if (content == null)
                {
                    continue;
                }

                content.DataContext = document;

                IDockable dockable;

                if (targetDock is ToolDock)
                {
                    dockable = new Tool
                    {
                        Id = document.Id,
                        Title = document.Title,
                        Content = content,
                        CanFloat = false,
                        CanPin = false,
                        CanClose = false
                    };
                }
                else
                {
                    dockable = new Document
                    {
                        Id = document.Id,
                        Title = document.Title,
                        Content = content,
                        CanFloat = false,
                        CanPin = false,
                        CanClose = false
                    };
                }

                _pluginDocuments[document] = dockable;

                targetDock.VisibleDockables ??= factory.CreateList<IDockable>();
                factory.AddDockable(targetDock, dockable);
                factory.SetActiveDockable(dockable);
                factory.SetFocusedDockable(targetDock, dockable);

                if (targetDock is ToolDock)
                {
                    ExpandBottomDock(layout);
                }
            }
        }

        private List<(IDockable Dockable, double Proportion)>? _savedBottomProportions;

        private void ExpandBottomDock(IDock layout)
        {
            if (_savedBottomProportions != null
                || FindDockableById(layout, "Bottom") is not { } bottom
                || FindDockableById(layout, "AssetsPane") is not { } assets)
            {
                return;
            }

            _savedBottomProportions = new List<(IDockable, double)> { (bottom, bottom.Proportion), (assets, assets.Proportion) };

            bottom.Proportion = Math.Max(double.IsNaN(bottom.Proportion) ? 0.0 : bottom.Proportion, 0.45);
            assets.Proportion = 0.72;

            if (FindDockableById(layout, "ConsolePane") is { } console)
            {
                _savedBottomProportions.Add((console, console.Proportion));
                console.Proportion = 0.28;
            }
        }

        private void RestoreBottomDock()
        {
            if (_savedBottomProportions == null)
            {
                return;
            }

            foreach ((IDockable dockable, double proportion) in _savedBottomProportions)
            {
                dockable.Proportion = proportion;
            }

            _savedBottomProportions = null;
        }

        /// <summary>
        /// The World Settings editor is a standalone modal window (not a PanelService
        /// dynamic panel) - it is a long-lived properties surface similar in spirit to
        /// VS Code's settings editor, so it bypasses the dock layout and panel stack.
        /// </summary>
        private void OnPackagingSettingsClick(object? sender, RoutedEventArgs e)
        {
            ShowPackagingSettingsWindow();
        }

        private async void ShowPackagingSettingsWindow()
        {
            if (_packagingSettingsWindow != null)
            {
                _packagingSettingsWindow.Activate();
                return;
            }

            var window = new PackagingSettingsWindow
            {
                DataContext = new PackagingSettingsViewModel()
            };

            _packagingSettingsWindow = window;
            window.Closed += (_, _) => _packagingSettingsWindow = null;

            await window.ShowDialog(this);
        }

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

        private async void ShowPluginsWindow()
        {
            if (_pluginsWindow != null)
            {
                _pluginsWindow.Activate();

                return;
            }

            var viewModel = new PluginsViewModel();
            var window = new PluginsWindow
            {
                DataContext = viewModel
            };

            _pluginsWindow = window;
            window.Closed += (_, _) =>
            {
                _pluginsWindow = null;
                viewModel.Dispose();
            };

            await window.ShowDialog(this);
        }

        private async void ShowProjectSettingsWindow()
        {
            if (_projectSettingsWindow != null)
            {
                _projectSettingsWindow.Activate();

                return;
            }

            var window = new ProjectSettingsWindow
            {
                DataContext = new ProjectSettingsViewModel()
            };

            _projectSettingsWindow = window;
            window.Closed += (_, _) => _projectSettingsWindow = null;

            await window.ShowDialog(this);
        }

        protected override void OnDataContextChanged(EventArgs e)
        {
            base.OnDataContextChanged(e);

            if (DataContext is MainWindowViewModel viewModel)
            {
                viewModel.GameProjectWindowRequested += ShowGameProjectWindow;
            }
        }

        private async void ShowGameProjectWindow()
        {
            if (DataContext is not MainWindowViewModel viewModel)
            {
                return;
            }

            this.FindControl<DropDownButton>("BuildDropDown")?.Flyout?.Hide();

            var gameProjectViewModel = new GameProjectWindowViewModel(EngineManager.CurrentProject?.FilePath, viewModel.CurrentGameProjectLanguage);

            await new GameProjectWindow { DataContext = gameProjectViewModel }.ShowDialog(this);

            bool openInVisualStudio = gameProjectViewModel.ConfirmedOpenInVisualStudio;

            viewModel.GenerateGameProject(gameProjectViewModel.ConfirmedLanguage, openFolder: !openInVisualStudio);

            if (openInVisualStudio)
            {
                _ = VisualStudioService.OpenGameProjectAsync(() => EngineManager.CurrentProject?.FilePath, gameProjectViewModel.ConfirmedLanguage);
            }
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
                nodeViewModel.RefreshConvertToEntityTypeTargets();
                nodeViewModel.RefreshCollisionState();
                nodeViewModel.RefreshSourcePrefab();

                if (DataContext is MainWindowViewModel mvm)
                {
                    nodeViewModel.RefreshSelectionContext(mvm.SceneHierarchy.SelectedNodes);
                }

                AddPluginNodeMenuItems(contextMenu, nodeViewModel.Node);
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

        // entries from editor plugins go where the anchor sits; whether each applies is read on the sim thread
        private static void AddPluginNodeMenuItems(ContextMenu contextMenu, Node node)
        {
            foreach (Control staleEntry in contextMenu.Items.OfType<Control>().Where(item => ReferenceEquals(item.Tag, PluginMenuEntryTag)).ToList())
            {
                contextMenu.Items.Remove(staleEntry);
            }

            List<EditorNodeMenuItem> pluginItems = EditorPluginService.Instance.NodeMenuItems.ToList();

            if (pluginItems.Count == 0)
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                List<EditorNodeMenuItem> applicableItems = pluginItems.Where(pluginItem => pluginItem.CanExecute(node)).ToList();

                Dispatcher.UIThread.Post(() =>
                {
                    int anchorIndex = contextMenu.Items.IndexOf(contextMenu.Items.OfType<Control>().FirstOrDefault(item => item.Tag as string == "PluginNodeMenuAnchor"));
                    int insertIndex = anchorIndex >= 0 ? anchorIndex + 1 : contextMenu.Items.Count;

                    foreach (EditorNodeMenuItem pluginItem in applicableItems)
                    {
                        contextMenu.Items.Insert(insertIndex++, new MenuItem
                        {
                            Header = pluginItem.Header,
                            Command = new RelayCommand(() => pluginItem.Execute(node)),
                            Tag = PluginMenuEntryTag
                        });
                    }
                });
            });
        }

        private void OnAssetContextMenuOpened(object? sender, RoutedEventArgs e)
        {
            if ((sender as ContextMenu)?.DataContext is AssetObjectViewModel assetViewModel)
            {
                assetViewModel.RefreshActions();
            }
        }

        private async void OnAddAssetMenuFlyoutOpened(object? sender, EventArgs e)
        {
            if (sender is MenuFlyout flyout)
            {
                await PopulateAddAssetMenuItems(flyout.Items);
            }
        }

        private async void OnAssetListContextMenuOpened(object? sender, RoutedEventArgs e)
        {
            if (sender is ContextMenu contextMenu
                && contextMenu.Items.OfType<MenuItem>().FirstOrDefault(item => item.Tag as string == "AddAssetMenu") is { } addMenuItem)
            {
                await PopulateAddAssetMenuItems(addMenuItem.Items);
            }
        }

        private static async Task PopulateAddAssetMenuItems(ItemCollection items)
        {
            if (MainWindowViewModel.Instance?.ContentBrowser is not { } contentBrowser)
            {
                return;
            }

            IReadOnlyList<AssetCreationEntry> entries = await contentBrowser.GetCreatableAssetEntriesAsync();

            // Swap in the new entries only once they are ready, so two overlapping opens can't stack duplicates
            foreach (object staleEntry in items.OfType<Control>().Where(item => ReferenceEquals(item.Tag, AssetCreationEntryTag)).ToList())
            {
                items.Remove(staleEntry);
            }

            bool addedUserDefinedGroup = false;

            foreach (AssetCreationEntry entry in entries)
            {
                if (entry.IsUserDefined && !addedUserDefinedGroup)
                {
                    items.Add(new Separator { Tag = AssetCreationEntryTag });
                    items.Add(new MenuItem { Header = "Game Types", IsEnabled = false, Tag = AssetCreationEntryTag });

                    addedUserDefinedGroup = true;
                }

                items.Add(new MenuItem { Header = entry.Header, Command = entry.Command, Tag = AssetCreationEntryTag });
            }
        }

        private static readonly object PluginMenuEntryTag = new object();

        private void OnMeshToolsMenuOpened(object? sender, RoutedEventArgs e)
        {
            if (sender is not ContextMenu menu)
            {
                return;
            }

            foreach (Control staleEntry in menu.Items.OfType<Control>().Where(item => ReferenceEquals(item.Tag, PluginMenuEntryTag)).ToList())
            {
                menu.Items.Remove(staleEntry);
            }

            foreach (EditorMeshToolViewModel tool in EditorPluginService.Instance.MeshTools)
            {
                MenuItem menuItem = new MenuItem
                {
                    Header = tool.Header,
                    Command = tool.ToggleCommand,
                    ToggleType = MenuItemToggleType.CheckBox,
                    IsChecked = tool.IsActive,
                    IsEnabled = tool.CanEnable,
                    Tag = PluginMenuEntryTag
                };

                ToolTip.SetTip(menuItem, tool.Tooltip);

                menu.Items.Add(menuItem);
            }
        }

        private void OnToolSettingsMenuOpened(object? sender, RoutedEventArgs e)
        {
            if (DataContext is MainWindowViewModel vm)
            {
                vm.RefreshDebugOverlayToggleStates();
            }

            if (sender is not ContextMenu menu)
            {
                return;
            }

            // entries from enabled editor plugins go at the end; they're rebuilt each time since plugins change with the project
            foreach (Control staleEntry in menu.Items.OfType<Control>().Where(item => ReferenceEquals(item.Tag, PluginMenuEntryTag)).ToList())
            {
                menu.Items.Remove(staleEntry);
            }

            List<EditorPluginMenuItem> pluginItems = EditorPluginService.Instance.ToolMenuItems.ToList();

            if (pluginItems.Count == 0)
            {
                return;
            }

            menu.Items.Add(new Separator { Tag = PluginMenuEntryTag });

            foreach (EditorPluginMenuItem pluginItem in pluginItems)
            {
                MenuItem menuItem = new MenuItem { Header = pluginItem.Header, Command = pluginItem.Command, Tag = PluginMenuEntryTag };

                if (pluginItem.IsChecked != null)
                {
                    menuItem.ToggleType = MenuItemToggleType.CheckBox;
                    menuItem.IsChecked = pluginItem.IsChecked();
                }

                menu.Items.Add(menuItem);
            }
        }

        private void OnBuildFlyoutOpened(object? sender, EventArgs e)
        {
            if (DataContext is MainWindowViewModel vm)
            {
                vm.RefreshNativeProjectMenuText();
            }
        }

        private static readonly object RecentProjectEntryTag = new object();

        private void OnOpenProjectMenuOpened(object? sender, RoutedEventArgs e)
        {
            if (sender is not ContextMenu menu || DataContext is not MainWindowViewModel viewModel)
            {
                return;
            }

            // Recent entries sit directly in the dropdown, above the static items declared in XAML
            foreach (Control staleEntry in menu.Items.OfType<Control>().Where(item => ReferenceEquals(item.Tag, RecentProjectEntryTag)).ToList())
            {
                menu.Items.Remove(staleEntry);
            }

            List<Control> entries = BuildRecentProjectMenuEntries(viewModel);

            for (int index = 0; index < entries.Count; index++)
            {
                entries[index].Tag = RecentProjectEntryTag;
                menu.Items.Insert(index, entries[index]);
            }
        }

        private void OnFileMenuSubmenuOpened(object? sender, RoutedEventArgs e)
        {
            // SubmenuOpened bubbles up from nested submenus too, and those must not be rebuilt while opening
            if (!ReferenceEquals(e.Source, sender) || DataContext is not MainWindowViewModel viewModel)
            {
                return;
            }

            FileOpenRecentMenuItem.Items.Clear();

            foreach (Control entry in BuildRecentProjectMenuEntries(viewModel))
            {
                FileOpenRecentMenuItem.Items.Add(entry);
            }
        }

        private static List<Control> BuildRecentProjectMenuEntries(MainWindowViewModel viewModel)
        {
            List<Control> entries = new List<Control>();

            IReadOnlyList<string> recentProjects = RecentProjectsService.Instance.RecentProjects;

            if (recentProjects.Count == 0)
            {
                entries.Add(new MenuItem { Header = "No Recent Projects", IsEnabled = false });

                return entries;
            }

            string? currentProjectFilepath = EngineManager.CurrentProject?.FilePath;

            for (int index = 0; index < recentProjects.Count; index++)
            {
                string projectFilepath = recentProjects[index];

                // Numbered access keys for the first nine entries; underscores in the name are escaped so they aren't read as access keys
                string accessKey = index < 9 ? $"_{index + 1}  " : "    ";

                MenuItem projectMenuItem = new MenuItem
                {
                    Header = $"{accessKey}{RecentProjectsService.GetProjectName(projectFilepath).Replace("_", "__")}",
                    Command = viewModel.OpenRecentProject,
                    CommandParameter = projectFilepath,
                    IsEnabled = !RecentProjectsService.IsSameProject(projectFilepath, currentProjectFilepath),
                };

                ToolTip.SetTip(projectMenuItem, projectFilepath);

                entries.Add(projectMenuItem);
            }

            entries.Add(new Separator());
            entries.Add(new MenuItem
            {
                Header = "Clear Recent Projects",
                Command = viewModel.ClearRecentProjects,
            });

            return entries;
        }

        /// <summary>
        /// Brings the selected asset into view. The content browser selects assets programmatically -
        /// a newly created one, for instance - and the selection is otherwise invisible if the item
        /// sits below the scrolled region.
        /// </summary>
        private void OnContentBrowserSelectionChanged(object? sender, SelectionChangedEventArgs e)
        {
            if (sender is not ListBox listBox)
            {
                return;
            }

            if (MainWindowViewModel.Instance?.ContentBrowser is { } contentBrowser)
            {
                contentBrowser.SelectedItems = listBox.SelectedItems?.Cast<object>().ToList() ?? new List<object>();
            }

            if (listBox.SelectedItem is not { } selected)
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

        private void OnContentBrowserAssetDoubleTapped(object? sender, TappedEventArgs e)
        {
            CancelRenameClick();

            object? item = (e.Source as StyledElement)?.DataContext;

            if (item is not AssetFolderViewModel and not AssetObjectViewModel)
            {
                return;
            }

            MainWindowViewModel? viewModel = MainWindowViewModel.Instance;

            // Same gate as the context menu's Edit / Switch to This World items
            if (viewModel == null || (item is AssetObjectViewModel && viewModel.IsSimulating))
            {
                return;
            }

            viewModel.ContentBrowser.OpenItem(item);

            e.Handled = true;
        }

        private void OnContentBrowserKeyDown(object? sender, KeyEventArgs e)
        {
            ContentBrowserViewModel? contentBrowser = MainWindowViewModel.Instance?.ContentBrowser;

            if (contentBrowser == null || e.Source is TextBox)
            {
                return;
            }

            bool isSimulating = MainWindowViewModel.Instance?.IsSimulating ?? false;

            switch (e.Key)
            {
                case Key.Enter when contentBrowser.SelectedItem != null:
                    if (contentBrowser.SelectedItem is AssetFolderViewModel || !isSimulating)
                    {
                        contentBrowser.OpenItem(contentBrowser.SelectedItem);
                    }

                    e.Handled = true;
                    break;
                case Key.Back when contentBrowser.GoUpCommand.CanExecute(null):
                    contentBrowser.GoUpCommand.Execute(null);
                    e.Handled = true;
                    break;
                case Key.Delete when !isSimulating:
                    contentBrowser.DeleteSelectionCommand.Execute(null);
                    e.Handled = true;
                    break;
                case Key.F2 when !isSimulating && GetFolderForRename(e.Source, contentBrowser) is { } folder:
                    BeginFolderRename(folder);
                    e.Handled = true;
                    break;
                case Key.F2 when !isSimulating && contentBrowser.SelectedItem is AssetObjectViewModel { Bucket: not null } asset:
                    BeginAssetRename(asset);
                    e.Handled = true;
                    break;
            }
        }

        private static AssetFolderViewModel? GetFolderForRename(object? source, ContentBrowserViewModel contentBrowser)
        {
            if (source is TreeViewItem { DataContext: AssetFolderViewModel { IsRoot: false } treeFolder })
            {
                return treeFolder;
            }

            return contentBrowser.SelectedItem is AssetFolderViewModel { IsRoot: false } selectedFolder ? selectedFolder : null;
        }

        private void OnContentBrowserPointerReleased(object? sender, PointerReleasedEventArgs e)
        {
            ICommand? command = e.InitialPressMouseButton switch
            {
                MouseButton.XButton1 => MainWindowViewModel.Instance?.ContentBrowser.GoBackCommand,
                MouseButton.XButton2 => MainWindowViewModel.Instance?.ContentBrowser.GoForwardCommand,
                _ => null
            };

            if (command?.CanExecute(null) == true)
            {
                command.Execute(null);
                e.Handled = true;
            }
        }

        private GridLength _folderTreeWidth = new GridLength(200);

        private void OnToggleFolderTreeClick(object? sender, RoutedEventArgs e)
        {
            if (FindVisualChildByName<Grid>("ContentBrowserBody") is not { } body || body.ColumnDefinitions.Count < 2)
            {
                return;
            }

            ColumnDefinition treeColumn = body.ColumnDefinitions[0];
            bool isVisible = treeColumn.Width.Value > 0;

            if (isVisible)
            {
                _folderTreeWidth = treeColumn.Width;
            }

            treeColumn.Width = isVisible ? new GridLength(0) : _folderTreeWidth;
            body.ColumnDefinitions[1].Width = isVisible ? new GridLength(0) : new GridLength(4);
        }

        private void OnRenameFolderMenuItemClick(object? sender, RoutedEventArgs e)
        {
            if ((sender as MenuItem)?.DataContext is AssetFolderViewModel { IsRoot: false } folder)
            {
                BeginFolderRename(folder);
            }
        }

        private void BeginFolderRename(AssetFolderViewModel folder)
        {
            folder.BeginRename();

            Dispatcher.UIThread.Post(() => FocusFolderRenameTextBox(folder), DispatcherPriority.Loaded);
        }

        private void OnContentBrowserFolderRenameRequested(AssetFolderViewModel folder)
        {
            Dispatcher.UIThread.Post(() => FocusFolderRenameTextBox(folder), DispatcherPriority.Loaded);
        }

        private void OnContentBrowserAssetRenameRequested(AssetObjectViewModel asset)
        {
            Dispatcher.UIThread.Post(() => FocusRenameTextBox("AssetRenameTextBox", asset), DispatcherPriority.Loaded);
        }

        private void FocusFolderRenameTextBox(AssetFolderViewModel folder)
        {
            FocusRenameTextBox("FolderRenameTextBox", folder);
        }

        private void FocusRenameTextBox(string textBoxName, object item)
        {
            foreach (TextBox textBox in this.GetVisualDescendants().OfType<TextBox>())
            {
                if (textBox.Name == textBoxName && ReferenceEquals(textBox.DataContext, item) && textBox.IsEffectivelyVisible)
                {
                    textBox.Focus();
                    textBox.SelectAll();

                    return;
                }
            }
        }

        private void OnFolderRenameTextBoxKeyDown(object? sender, KeyEventArgs e)
        {
            if ((sender as TextBox)?.DataContext is not AssetFolderViewModel folder || !folder.IsEditingName)
            {
                return;
            }

            if (e.Key == Key.Enter)
            {
                MainWindowViewModel.Instance?.ContentBrowser.RenameFolder(folder, folder.EditingName);
                e.Handled = true;
            }
            else if (e.Key == Key.Escape)
            {
                folder.CancelRename();
                e.Handled = true;
            }
        }

        private void OnFolderRenameTextBoxLostFocus(object? sender, RoutedEventArgs e)
        {
            if ((sender as TextBox)?.DataContext is AssetFolderViewModel { IsEditingName: true } folder)
            {
                MainWindowViewModel.Instance?.ContentBrowser.RenameFolder(folder, folder.EditingName);
            }
        }

        private void OnRenameAssetMenuItemClick(object? sender, RoutedEventArgs e)
        {
            if ((sender as MenuItem)?.DataContext is AssetObjectViewModel { Bucket: not null } asset)
            {
                BeginAssetRename(asset);
            }
        }

        private void BeginAssetRename(AssetObjectViewModel asset)
        {
            asset.BeginRename();

            Dispatcher.UIThread.Post(() => FocusRenameTextBox("AssetRenameTextBox", asset), DispatcherPriority.Loaded);
        }

        private void OnAssetRenameTextBoxKeyDown(object? sender, KeyEventArgs e)
        {
            if ((sender as TextBox)?.DataContext is not AssetObjectViewModel asset || !asset.IsEditingName)
            {
                return;
            }

            if (e.Key == Key.Enter)
            {
                MainWindowViewModel.Instance?.ContentBrowser.RenameAsset(asset, asset.EditingName);
                e.Handled = true;
            }
            else if (e.Key == Key.Escape)
            {
                asset.CancelRename();
                e.Handled = true;
            }
        }

        private void OnAssetRenameTextBoxLostFocus(object? sender, RoutedEventArgs e)
        {
            if ((sender as TextBox)?.DataContext is AssetObjectViewModel { IsEditingName: true } asset)
            {
                MainWindowViewModel.Instance?.ContentBrowser.RenameAsset(asset, asset.EditingName);
            }
        }

        private static bool IsBrowserItemNameText(object? source)
        {
            return source is Visual visual && visual.GetSelfAndVisualAncestors().OfType<TextBlock>().Any(textBlock => textBlock.Name == "BrowserItemNameText");
        }

        /// <summary>A second click on the name of the selected item renames it, once it's clear the click wasn't the start of a double click</summary>
        private void StartRenameClickTimer(object item)
        {
            CancelRenameClick();

            TimeSpan doubleTapTime = this.GetPlatformSettings()?.GetDoubleTapTime(PointerType.Mouse) ?? TimeSpan.FromMilliseconds(500);

            _renameClickTimer = new DispatcherTimer { Interval = doubleTapTime };
            _renameClickTimer.Tick += (_, _) =>
            {
                CancelRenameClick();

                if (MainWindowViewModel.Instance is not { IsSimulating: false } viewModel
                    || viewModel.ContentBrowser.SelectedItems.Count != 1
                    || !ReferenceEquals(viewModel.ContentBrowser.SelectedItem, item))
                {
                    return;
                }

                switch (item)
                {
                    case AssetFolderViewModel { IsRoot: false } folder:
                        BeginFolderRename(folder);
                        break;
                    case AssetObjectViewModel { Bucket: not null } asset:
                        BeginAssetRename(asset);
                        break;
                }
            };
            _renameClickTimer.Start();
        }

        private void CancelRenameClick()
        {
            _renameClickTimer?.Stop();
            _renameClickTimer = null;
        }

        private async void OnCopyAssetPathClick(object? sender, RoutedEventArgs e)
        {
            if ((sender as MenuItem)?.DataContext is not AssetObjectViewModel { Bucket: { } bucket } asset || Clipboard == null)
            {
                return;
            }

            await Clipboard.SetTextAsync($"Game://{bucket.Name}/{asset.DisplayName}");
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
            vm?.SceneHierarchy.ReparentNodes(new[] { nodeViewModel }, grandparent);
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
            TreeView sceneTree = _sceneTree;
            sceneTree.TemplateApplied += (_, _) =>
            {
                if (ReferenceEquals(sceneTree, _sceneTree))
                {
                    _sceneTreeScrollViewer = sceneTree.GetVisualDescendants().OfType<ScrollViewer>().FirstOrDefault();
                }
            };

            // a rebuilt tree starts out with nothing selected
            SyncTreeSelectionFromViewModel();

            return true;
        }

        private void ReleaseSceneHierarchyTree()
        {
            if (_sceneTree == null)
                return;

            _sceneTree.RemoveHandler(InputElement.PointerPressedEvent, OnSceneTreePointerPressed);
            _sceneTree.RemoveHandler(InputElement.PointerMovedEvent, OnSceneTreePointerMoved);
            _sceneTree.RemoveHandler(InputElement.PointerReleasedEvent, OnSceneTreePointerReleased);
            _sceneTree.RemoveHandler(DragDrop.DragOverEvent, OnSceneTreeDragOver);
            _sceneTree.RemoveHandler(DragDrop.DragLeaveEvent, OnSceneTreeDragLeave);
            _sceneTree.RemoveHandler(DragDrop.DropEvent, OnSceneTreeDrop);

            // the torn down tree would otherwise keep pushing its own selection changes to the engine
            _sceneTree.SelectionChanged -= OnSceneTreeSelectionChanged;

            _sceneTree = null;
            _sceneTreeScrollViewer = null;
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

                // Keep a multi-selection intact on press so it can be dragged as a group; a plain click
                // collapses it to the pressed item on release instead.
                if (_dragCandidate != null
                    && (keyModifiers & KeyModifiers.Control) == 0
                    && DataContext is MainWindowViewModel selectionVm
                    && selectionVm.SceneHierarchy.SelectedNodes.Count > 1
                    && selectionVm.SceneHierarchy.SelectedNodes.Contains(_dragCandidate))
                {
                    _pendingSingleSelect = _dragCandidate;
                    e.Handled = true;
                }
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

            _dragPressedArgs.Source = _sceneTree;

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
                if (_pendingSingleSelect != null && _sceneTree != null)
                {
                    _sceneTree.SelectedItem = _pendingSingleSelect;
                }

                _pendingSingleSelect = null;
                _dragCandidate = null;
                _dragPressedArgs = null;
            }
        }

        private void OnSceneTreeDragOver(object? sender, DragEventArgs e)
        {
            if (e.DataTransfer.Contains(AssetDragFormat))
            {
                var vm = DataContext as MainWindowViewModel;

                if (e.DataTransfer.Contains(EntityAssetDragFormat))
                {
                    var entityTarget = FindNodeViewModelInEventSource(e.Source);
                    bool isEntityTarget = entityTarget?.Node is Entity;

                    e.DragEffects = isEntityTarget ? DragDropEffects.Copy : DragDropEffects.None;
                    vm?.SceneHierarchy.SetDropTarget(isEntityTarget ? entityTarget : null);

                    UpdateSceneTreeAssetDropTarget(e, isEntityTarget ? entityTarget : null);
                }
                else
                {
                    e.DragEffects = DragDropEffects.Copy;
                    vm?.SceneHierarchy.SetDropTarget(null);
                }

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
                && GetDraggedNodes(dragged).All(node => node != target && !SceneHierarchyViewModel.IsAncestorOf(node, target));

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
                var assetTarget = FindNodeViewModelInEventSource(e.Source);

                EndDrag();

                var vm = DataContext as MainWindowViewModel;
                if (vm != null && TryGetDraggedAsset(e, out uint bucketIndex, out Name assetName))
                {
                    vm.DropAssetOnSceneHierarchy(bucketIndex, assetName, assetTarget?.UUID);
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
            vm_node?.SceneHierarchy.ReparentNodes(GetDraggedNodes(dragged), target);

            e.Handled = true;
        }

        // Dragging any node of the current selection drags the whole selection.
        private List<NodeViewModel> GetDraggedNodes(NodeViewModel dragged)
        {
            if (DataContext is MainWindowViewModel mvm && mvm.SceneHierarchy.SelectedNodes.Contains(dragged))
            {
                return mvm.SceneHierarchy.SelectedNodes.ToList();
            }

            return new List<NodeViewModel> { dragged };
        }

        private void EndDrag()
        {
            _dragPressedArgs?.Pointer.Capture(null);

            _isDragging = false;
            _dragCandidate = null;
            _dragPressedArgs = null;
            _pendingSingleSelect = null;

            var vm = DataContext as MainWindowViewModel;
            vm?.SceneHierarchy.SetDropTarget(null);

            if (_sceneTreeAssetDropTarget != null)
            {
                _sceneTreeAssetDropTarget = null;
                vm?.ClearAssetDropTarget();
            }

            _autoScrollDelta = 0;
            _autoScrollTimer?.Stop();
        }

        private void OnSceneTreeSelectionChanged(object? sender, SelectionChangedEventArgs e)
        {
            var mvm = DataContext as MainWindowViewModel;
            if (mvm == null || _syncingTreeSelection)
                return;

            if (_suppressTreeSelectionHandling)
            {
                mvm.SceneHierarchy.SetSuppressSelectionNotifications(false);
                return;
            }

            var added = e.AddedItems.OfType<NodeViewModel>().ToList();
            var removed = e.RemovedItems.OfType<NodeViewModel>().ToList();
            var current = _sceneTree.SelectedItems.OfType<NodeViewModel>().ToList();

            // Un-suppress after SelectedItem binding fired (suppressed)
            mvm.SceneHierarchy.SetSuppressSelectionNotifications(false);

            mvm.HandleTreeSelectionChanged(added, removed, current);
        }

        private void OnSceneHierarchyPropertyChanged(object? sender, System.ComponentModel.PropertyChangedEventArgs e)
        {
            if (e.PropertyName == nameof(SceneHierarchyViewModel.SelectedNodes))
            {
                SyncTreeSelectionFromViewModel();
            }
        }

        // The tree's highlight follows SceneHierarchy.SelectedNodes, which also changes from the engine
        // side (viewport picking, undo, etc.) without the tree being clicked.
        private void SyncTreeSelectionFromViewModel()
        {
            if (_sceneTree == null || DataContext is not MainWindowViewModel mvm)
                return;

            var target = mvm.SceneHierarchy.SelectedNodes;
            var selected = _sceneTree.SelectedItems;

            if (selected.Count == target.Count && target.All(selected.Contains))
                return;

            _syncingTreeSelection = true;
            mvm.SceneHierarchy.SetSuppressSelectionNotifications(true);

            try
            {
                if (target.Count == 1)
                {
                    // Goes through the single-select path, which also scrolls the item into view.
                    _sceneTree.SelectedItem = target[0];
                }
                else
                {
                    foreach (NodeViewModel vm in target)
                    {
                        if (!selected.Contains(vm))
                            selected.Add(vm);
                    }

                    for (int i = selected.Count - 1; i >= 0; i--)
                    {
                        if (selected[i] is not NodeViewModel vm || !target.Contains(vm))
                            selected.RemoveAt(i);
                    }
                }
            }
            finally
            {
                mvm.SceneHierarchy.SetSuppressSelectionNotifications(false);
                _syncingTreeSelection = false;
            }
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

        private sealed class ContentBrowserDragPayload
        {
            public List<object> Items { get; init; } = new List<object>();
        }

        private static readonly DataFormat<ContentBrowserDragPayload> ContentBrowserDragFormat =
            DataFormat.CreateInProcessFormat<ContentBrowserDragPayload>("hyperion-content-browser-items");

        private static readonly string[] ImportableFileExtensions =
        {
            ".obj", ".fbx", ".gltf", ".glb", ".mesh.xml", ".skeleton.xml", ".jpg", ".jpeg", ".png", ".tga", ".bmp", ".wav"
        };

        private bool SetupContentBrowserDragDrop()
        {
            if (_contentBrowserAssetList != null && _contentBrowserFolderTree != null && _contentBrowserPathBar != null)
                return true;

            _contentBrowserAssetList ??= FindVisualChildByName<ListBox>("ContentBrowserAssetList");
            _contentBrowserFolderTree ??= FindVisualChildByName<TreeView>("ContentBrowserFolderTree");
            _contentBrowserPathBar ??= FindVisualChildByName<ItemsControl>("ContentBrowserPathBar");

            if (_contentBrowserAssetList == null || _contentBrowserFolderTree == null || _contentBrowserPathBar == null)
                return false;

            foreach (Control dragSource in new Control[] { _contentBrowserAssetList, _contentBrowserFolderTree })
            {
                dragSource.AddHandler(InputElement.PointerPressedEvent, OnContentBrowserPointerPressed, RoutingStrategies.Tunnel);
                dragSource.AddHandler(InputElement.PointerMovedEvent, OnContentBrowserPointerMoved, RoutingStrategies.Tunnel);
                dragSource.AddHandler(InputElement.PointerReleasedEvent, OnContentBrowserDragSourcePointerReleased, RoutingStrategies.Tunnel);
            }

            _contentBrowserFolderTree.AddHandler(InputElement.KeyDownEvent, OnContentBrowserFolderTreeKeyDown, RoutingStrategies.Tunnel);

            // a long path keeps the open folder in view
            if (FindVisualChildByName<ScrollViewer>("ContentBrowserPathScroll") is { } pathScroll)
            {
                pathScroll.PropertyChanged += (_, e) =>
                {
                    if (e.Property == ScrollViewer.ExtentProperty || e.Property == ScrollViewer.ViewportProperty)
                    {
                        pathScroll.Offset = pathScroll.Offset.WithX(Math.Max(0, pathScroll.Extent.Width - pathScroll.Viewport.Width));
                    }
                };
            }

            foreach (Control dropTarget in new Control[] { _contentBrowserAssetList, _contentBrowserFolderTree, _contentBrowserPathBar })
            {
                DragDrop.SetAllowDrop(dropTarget, true);
                dropTarget.AddHandler(DragDrop.DragOverEvent, OnContentBrowserDragOver);
                dropTarget.AddHandler(DragDrop.DragLeaveEvent, OnContentBrowserDragLeave);
                dropTarget.AddHandler(DragDrop.DropEvent, OnContentBrowserDrop);
            }

            return true;
        }

        private static object? FindBrowserItemInEventSource(object? source)
        {
            for (var control = source as Control; control != null; control = control.Parent as Control)
            {
                if (control.DataContext is AssetObjectViewModel or AssetFolderViewModel)
                    return control.DataContext;

                if (control is ListBox or TreeView)
                    return null;
            }

            return null;
        }

        private bool _isFolderTreeUserSelecting;

        private void MarkFolderTreeUserSelecting()
        {
            _isFolderTreeUserSelecting = true;

            Dispatcher.UIThread.Post(() => _isFolderTreeUserSelecting = false, DispatcherPriority.Background);
        }

        private void OnContentBrowserFolderTreeKeyDown(object? sender, KeyEventArgs e)
        {
            if (e.Source is not TextBox)
            {
                MarkFolderTreeUserSelecting();
            }
        }

        private void OnContentBrowserFolderTreeSelectionChanged(object? sender, SelectionChangedEventArgs e)
        {
            if (!_isFolderTreeUserSelecting || (sender as TreeView)?.SelectedItem is not AssetFolderViewModel folder)
            {
                return;
            }

            MainWindowViewModel.Instance?.ContentBrowser.NavigateTo(folder);
        }

        private void OnContentBrowserPointerPressed(object? sender, PointerPressedEventArgs e)
        {
            CancelRenameClick();

            if (sender is TreeView)
            {
                MarkFolderTreeUserSelecting();
            }

            var point = e.GetCurrentPoint(sender as Visual);

            if (!point.Properties.IsLeftButtonPressed)
            {
                return;
            }

            object? item = FindBrowserItemInEventSource(e.Source);

            _browserDragCandidate = item is AssetFolderViewModel { IsRoot: true } ? null : item;
            _browserDragPressedArgs = e;
            _browserDragStartPoint = e.GetPosition(sender as Visual);
            _isDraggingBrowserItems = false;
            _pendingBrowserSingleSelect = null;

            // the tunnel handler runs before the list updates its selection, so this is what was selected before the click
            _renameClickCandidate = sender is ListBox { SelectedItems: { Count: 1 } previousSelection }
                && item != null
                && e.ClickCount == 1
                && e.KeyModifiers == KeyModifiers.None
                && previousSelection.Contains(item)
                && IsBrowserItemNameText(e.Source)
                    ? item
                    : null;

            if (sender is ListBox listBox
                && item != null
                && e.KeyModifiers == KeyModifiers.None
                && listBox.SelectedItems is { Count: > 1 } selectedItems
                && selectedItems.Contains(item))
            {
                _pendingBrowserSingleSelect = item;
                e.Handled = true;
            }
        }

        private void OnContentBrowserDragSourcePointerReleased(object? sender, PointerReleasedEventArgs e)
        {
            if (_renameClickCandidate != null && !_isDraggingBrowserItems)
            {
                StartRenameClickTimer(_renameClickCandidate);
            }

            _renameClickCandidate = null;

            if (_pendingBrowserSingleSelect != null && !_isDraggingBrowserItems && sender is ListBox listBox)
            {
                listBox.SelectedItems?.Clear();
                listBox.SelectedItem = _pendingBrowserSingleSelect;
            }

            _pendingBrowserSingleSelect = null;
        }

        private async void OnContentBrowserPointerMoved(object? sender, PointerEventArgs e)
        {
            if (_browserDragCandidate == null || _isDraggingBrowserItems)
                return;

            if (!e.GetCurrentPoint(sender as Visual).Properties.IsLeftButtonPressed)
            {
                _browserDragCandidate = null;
                _browserDragPressedArgs = null;
                return;
            }

            var pos = e.GetPosition(sender as Visual);
            var delta = pos - _browserDragStartPoint;

            if (Math.Abs(delta.X) < 5 && Math.Abs(delta.Y) < 5)
                return;

            if (_browserDragPressedArgs == null)
            {
                _browserDragCandidate = null;
                return;
            }

            _isDraggingBrowserItems = true;
            _pendingBrowserSingleSelect = null;

            object candidate = _browserDragCandidate;

            List<object> items = sender is ListBox listBox && listBox.SelectedItems is { } selectedItems && selectedItems.Contains(candidate)
                ? selectedItems.Cast<object>().ToList()
                : new List<object> { candidate };

            var dataItem = new DataTransferItem();
            dataItem.Set(ContentBrowserDragFormat, new ContentBrowserDragPayload { Items = items });

            // viewport and hierarchy drops take one asset, the one the drag started on
            if ((candidate as AssetObjectViewModel ?? items.OfType<AssetObjectViewModel>().FirstOrDefault()) is { } asset)
            {
                dataItem.Set(AssetDragFormat, $"{asset.Bucket?.BucketIndex ?? 0}|{asset.AssetDesc.Name}");

                if (asset.TypeName != null && (EntityAssetTypeNames.Contains(asset.TypeName) || EditorPluginService.Instance.IsEntityDropAssetClass(asset.TypeName)))
                {
                    dataItem.Set(EntityAssetDragFormat, asset.AssetDesc.Name.ToString());
                }
            }

            if (sender is Visual dragSource)
            {
                _browserDragPressedArgs.Source = dragSource;
            }
            
            var data = new DataTransfer();
            data.Add(dataItem);

            try
            {
                await DragDrop.DoDragDropAsync(_browserDragPressedArgs, data, DragDropEffects.Copy | DragDropEffects.Move);
            }
            catch (Exception ex) when (ex is System.Runtime.InteropServices.COMException)
            {
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Error, $"Content browser drag-and-drop failed: {ex}");
            }
            finally
            {
                _isDraggingBrowserItems = false;
                _browserDragCandidate = null;
                _browserDragPressedArgs = null;

                // DragLeave isn't guaranteed when the drag is cancelled, so don't leave a target highlighted
                (DataContext as MainWindowViewModel)?.ClearAssetDropTarget();
                SetBrowserDropTarget(null);
            }
        }

        private AssetFolderViewModel? GetBrowserDropFolder(object? sender, DragEventArgs e, bool isFileDrop)
        {
            for (var control = e.Source as Control; control != null; control = control.Parent as Control)
            {
                if (control.DataContext is AssetFolderViewModel folder)
                    return folder;

                if (control.DataContext is AssetObjectViewModel || ReferenceEquals(control, sender))
                    break;
            }

            if (isFileDrop && ReferenceEquals(sender, _contentBrowserAssetList))
            {
                return MainWindowViewModel.Instance?.ContentBrowser.CurrentFolder;
            }

            return null;
        }

        private static bool CanMoveInto(IReadOnlyList<object> items, AssetFolderViewModel target)
        {
            foreach (object item in items)
            {
                switch (item)
                {
                    case AssetFolderViewModel folder when !folder.IsRoot && !target.IsSameOrDescendantOf(folder) && !ReferenceEquals(folder.Parent, target):
                        return true;
                    case AssetObjectViewModel asset when asset.FolderPath != target.Path:
                        return true;
                }
            }

            return false;
        }

        private static List<string> GetDroppedImportableFiles(DragEventArgs e)
        {
            var paths = new List<string>();

            var items = e.DataTransfer.TryGetFiles();
            if (items == null)
                return paths;

            foreach (var item in items)
            {
                string? path = item.TryGetLocalPath();
                if (string.IsNullOrEmpty(path) || path.Contains('|'))
                    continue;

                if (ImportableFileExtensions.Any(ext => path.EndsWith(ext, StringComparison.OrdinalIgnoreCase)))
                {
                    paths.Add(path);
                }
            }

            return paths;
        }

        private void SetBrowserDropTarget(AssetFolderViewModel? folder)
        {
            if (ReferenceEquals(_browserDropTarget, folder))
                return;

            if (_browserDropTarget != null)
                _browserDropTarget.IsDropTarget = false;

            _browserDropTarget = folder;
            _browserDropHoverStart = DateTime.UtcNow;

            if (folder != null)
                folder.IsDropTarget = true;
        }

        private void OnContentBrowserDragOver(object? sender, DragEventArgs e)
        {
            bool isSimulating = MainWindowViewModel.Instance?.IsSimulating ?? false;

            e.DragEffects = DragDropEffects.None;
            e.Handled = true;

            AssetFolderViewModel? target = null;

            if (!isSimulating && e.DataTransfer.TryGetValue(ContentBrowserDragFormat) is { } payload)
            {
                target = GetBrowserDropFolder(sender, e, isFileDrop: false);

                if (target != null && CanMoveInto(payload.Items, target))
                {
                    e.DragEffects = DragDropEffects.Move;
                }
                else
                {
                    target = null;
                }
            }
            else if (!isSimulating && e.DataTransfer.Contains(DataFormat.File) && GetDroppedImportableFiles(e).Count > 0)
            {
                target = GetBrowserDropFolder(sender, e, isFileDrop: true);

                if (target != null)
                {
                    e.DragEffects = DragDropEffects.Copy;
                }
            }

            SetBrowserDropTarget(target);

            if (ReferenceEquals(sender, _contentBrowserFolderTree))
            {
                AutoExpandAndScrollFolderTree(e, target);
            }
        }

        private void AutoExpandAndScrollFolderTree(DragEventArgs e, AssetFolderViewModel? target)
        {
            if (target is { IsExpanded: false, Children.Count: > 0 } && (DateTime.UtcNow - _browserDropHoverStart).TotalMilliseconds > 600)
            {
                target.IsExpanded = true;
            }

            if (_contentBrowserFolderTree?.GetVisualDescendants().OfType<ScrollViewer>().FirstOrDefault() is not { } scrollViewer)
            {
                return;
            }

            const double edgeSize = 20.0;
            const double scrollStep = 8.0;

            double y = e.GetPosition(_contentBrowserFolderTree).Y;
            double height = _contentBrowserFolderTree.Bounds.Height;

            if (y < edgeSize)
            {
                scrollViewer.Offset = scrollViewer.Offset.WithY(Math.Max(0, scrollViewer.Offset.Y - scrollStep));
            }
            else if (y > height - edgeSize)
            {
                scrollViewer.Offset = scrollViewer.Offset.WithY(scrollViewer.Offset.Y + scrollStep);
            }
        }

        private void OnContentBrowserDragLeave(object? sender, DragEventArgs e)
        {
            SetBrowserDropTarget(null);
        }

        private void OnContentBrowserDrop(object? sender, DragEventArgs e)
        {
            ContentBrowserViewModel? contentBrowser = MainWindowViewModel.Instance?.ContentBrowser;

            SetBrowserDropTarget(null);

            if (contentBrowser == null || (MainWindowViewModel.Instance?.IsSimulating ?? false))
            {
                return;
            }

            if (e.DataTransfer.TryGetValue(ContentBrowserDragFormat) is { } payload)
            {
                if (GetBrowserDropFolder(sender, e, isFileDrop: false) is { } target && CanMoveInto(payload.Items, target))
                {
                    contentBrowser.MoveItems(payload.Items, target);
                    e.Handled = true;
                }

                return;
            }

            List<string> files = GetDroppedImportableFiles(e);

            if (files.Count > 0 && GetBrowserDropFolder(sender, e, isFileDrop: true) is { } importFolder)
            {
                contentBrowser.ImportFiles(files, importFolder);
                e.Handled = true;
            }
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
            _viewportDropTarget.AddHandler(DragDrop.DragLeaveEvent, OnViewportDragLeave);
            _viewportDropTarget.AddHandler(DragDrop.DropEvent, OnViewportDrop);
            return true;
        }

        private void OnViewportDragOver(object? sender, DragEventArgs e)
        {
            if (!e.DataTransfer.Contains(AssetDragFormat))
            {
                if (e.DataTransfer.Contains(DataFormat.File))
                {
                    e.DragEffects = GetDroppedModelFiles(e).Count > 0 ? DragDropEffects.Copy : DragDropEffects.None;
                    e.Handled = true;
                }

                return;
            }

            e.DragEffects = DragDropEffects.Copy;
            e.Handled = true;

            if (!e.DataTransfer.Contains(EntityAssetDragFormat)
                || !TryGetDraggedAsset(e, out uint bucketIndex, out Name assetName))
            {
                return;
            }

            var (nx, ny) = GetNormalizedViewportPosition(e);

            var vm = DataContext as MainWindowViewModel;
            vm?.UpdateViewportAssetDropTarget(bucketIndex, assetName, nx, ny);
        }

        private void OnViewportDragLeave(object? sender, DragEventArgs e)
        {
            var vm = DataContext as MainWindowViewModel;
            vm?.ClearAssetDropTarget();
        }

        private void OnViewportDrop(object? sender, DragEventArgs e)
        {
            var vm = DataContext as MainWindowViewModel;

            if (!TryGetDraggedAsset(e, out uint bucketIndex, out Name assetName))
            {
                var modelFiles = GetDroppedModelFiles(e);

                if (modelFiles.Count > 0)
                {
                    var (fx, fy) = GetNormalizedViewportPosition(e);
                    vm?.ImportModelsOnViewport(modelFiles, fx, fy);
                    e.Handled = true;
                }

                return;
            }

            var (nx, ny) = GetNormalizedViewportPosition(e);

            vm?.DropAssetOnViewport(bucketIndex, assetName, nx, ny, placeAsInstance: (e.KeyModifiers & KeyModifiers.Alt) != 0);

            e.Handled = true;
        }

        private (float X, float Y) GetNormalizedViewportPosition(DragEventArgs e)
        {
            if (_viewportDropTarget == null)
                return (0.5f, 0.5f);

            var pos = e.GetPosition(_viewportDropTarget);
            double nx = Math.Clamp(pos.X / _viewportDropTarget.Bounds.Width, 0.0, 1.0);
            double ny = Math.Clamp(pos.Y / _viewportDropTarget.Bounds.Height, 0.0, 1.0);

            return ((float)nx, (float)ny);
        }

        private static readonly string[] ModelFileExtensions = { ".obj", ".fbx", ".gltf", ".glb" };

        private static List<string> GetDroppedModelFiles(DragEventArgs e)
        {
            var paths = new List<string>();

            var items = e.DataTransfer.TryGetFiles();
            if (items == null)
                return paths;

            foreach (var item in items)
            {
                string? path = item.TryGetLocalPath();
                if (string.IsNullOrEmpty(path) || path.Contains('|'))
                    continue;

                if (ModelFileExtensions.Any(ext => path.EndsWith(ext, StringComparison.OrdinalIgnoreCase))
                    || path.EndsWith(".mesh.xml", StringComparison.OrdinalIgnoreCase))
                {
                    paths.Add(path);
                }
            }

            return paths;
        }

        private static bool TryGetDraggedAsset(DragEventArgs e, out uint bucketIndex, out Name assetName)
        {
            bucketIndex = 0;
            assetName = default;

            if (!e.DataTransfer.Contains(AssetDragFormat))
                return false;

            var assetData = e.DataTransfer.TryGetValue(AssetDragFormat);
            if (string.IsNullOrEmpty(assetData))
                return false;

            var parts = assetData.Split('|');
            if (parts.Length != 2 || !uint.TryParse(parts[0], out bucketIndex))
                return false;

            assetName = new Name(parts[1]);
            return true;
        }

        private void UpdateSceneTreeAssetDropTarget(DragEventArgs e, NodeViewModel? target)
        {
            if (target == _sceneTreeAssetDropTarget)
                return;

            _sceneTreeAssetDropTarget = target;

            var vm = DataContext as MainWindowViewModel;
            if (vm == null)
                return;

            if (target == null || !TryGetDraggedAsset(e, out uint bucketIndex, out Name assetName))
            {
                vm.ClearAssetDropTarget();
                return;
            }

            vm.UpdateSceneHierarchyAssetDropTarget(bucketIndex, assetName, target);
        }

        protected override void OnClosing(WindowClosingEventArgs e)
        {
            // The engine's message pump can dispatch WM_CLOSE from inside Hyp_MainThreadUpdate; shutting down there
            // frees the engine under the pump, so close again once the update has returned
            if (_mainThreadUpdateDepth > 0)
            {
                e.Cancel = true;
                _closeDeferred = true;

                return;
            }

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
                _mainThreadUpdateDepth++;

                try
                {
                    NativeBindings.Hyp_MainThreadUpdate();
                }
                finally
                {
                    _mainThreadUpdateDepth--;
                }
            }

            if (_closeDeferred && _mainThreadUpdateDepth == 0)
            {
                _closeDeferred = false;

                Close();
            }

            ConsoleService.Instance.ProcessLogQueue();

            var topLevel = GetTopLevel(this);
            topLevel?.RequestAnimationFrame(OnFrame);
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
                    (shift ? vm.SelectNone : vm.SelectAll).Execute(null);
                    e.Handled = true;
                    break;

                case Key.S:
                    if (!vm.IsSimulating)
                    {
                        (shift ? vm.SaveProjectAs : vm.SaveProject).Execute(null);
                        e.Handled = true;
                    }
                    break;

                case Key.N:
                    if (!vm.IsSimulating)
                    {
                        vm.NewProject.Execute(null);
                        e.Handled = true;
                    }
                    break;

                case Key.O:
                    if (!vm.IsSimulating)
                    {
                        vm.OpenProject.Execute(null);
                        e.Handled = true;
                    }
                    break;
            }
        }
    }
}
