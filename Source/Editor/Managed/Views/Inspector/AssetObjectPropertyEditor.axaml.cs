using System;
using System.ComponentModel;
using System.IO;
using System.Threading.Tasks;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Input.Platform;
using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Services;
using Hyperion.Editor.ViewModels;

namespace Hyperion.Editor.Views.Inspector
{
    public partial class AssetObjectPropertyEditor : UserControl
    {
        private const int MaxPickerResults = 10;

        public AssetObjectPropertyEditor()
        {
            InitializeComponent();

            PART_CopyMenuItem.Click += OnCopyClicked;
            PART_PasteMenuItem.Click += OnPasteClicked;

            DataContextChanged += OnDataContextChanged;

            // The AutoCompleteBox queries matching assets on demand (top N),
            // debounced + cancelled internally by Avalonia. No client-side cache.
            PART_PickerBox.AsyncPopulator = (search, token) =>
            {
                if (DataContext is ObjectPropertyViewModel vm)
                {
                    return vm.QueryMatchingAssetsAsync(search, MaxPickerResults);
                }

                return Task.FromResult<IEnumerable<object>>(Array.Empty<object>());
            };

            PART_PickerToggle.Click += OnPickerToggleClicked;
            PART_PickerBox.LostFocus += OnPickerBoxLostFocus;
            PART_PickerBox.SelectionChanged += OnPickerSelectionChanged;

            SubscribeViewModel(DataContext as ObjectPropertyViewModel);
            UpdateCopyEnabled();
        }

        private ObjectPropertyViewModel? _subscribedVm;
        private AssetObjectEditPanelViewModel? _editPanel;

        private void OnDataContextChanged(object? sender, EventArgs e)
        {
            SubscribeViewModel(DataContext as ObjectPropertyViewModel);
            CloseEditPanel();
            UpdateCopyEnabled();

            if (_subscribedVm != null)
            {
                SyncEditPanel(_subscribedVm);
            }
        }

        private void SubscribeViewModel(ObjectPropertyViewModel? vm)
        {
            if (_subscribedVm != null)
            {
                _subscribedVm.PropertyChanged -= OnViewModelPropertyChanged;
                _subscribedVm = null;
            }

            if (vm != null)
            {
                _subscribedVm = vm;
                vm.PropertyChanged += OnViewModelPropertyChanged;
            }
        }

        private void OnViewModelPropertyChanged(object? sender, PropertyChangedEventArgs e)
        {
            if (DataContext is not ObjectPropertyViewModel vm)
            {
                return;
            }

            if (e.PropertyName == nameof(ObjectPropertyViewModel.AssetPathDisplay)
                || e.PropertyName == nameof(ObjectPropertyViewModel.HasSubObject)
                || e.PropertyName == nameof(ObjectPropertyViewModel.SubObject))
            {
                UpdateCopyEnabled();
            }

            if (e.PropertyName == nameof(ObjectPropertyViewModel.IsEditorExpanded)
                || e.PropertyName == nameof(ObjectPropertyViewModel.HasSubObject)
                || e.PropertyName == nameof(ObjectPropertyViewModel.SubObject))
            {
                SyncEditPanel(vm);
            }
        }

        private void SyncEditPanel(ObjectPropertyViewModel vm)
        {
            if (vm.IsEditorExpanded && vm.HasSubObject && vm.SubObject != null)
            {
                if (vm.SubObject.Target is ScriptAsset)
                {
                    // Scripts have no property panel - open the file instead and untoggle.
                    vm.IsEditorExpanded = false;
                    OpenScriptFile(vm.SubObject.Target);
                    return;
                }

                if (_editPanel != null)
                {
                    return;
                }

                var panel = new AssetObjectEditPanelViewModel(vm, onClosed: () =>
                {
                    _editPanel = null;
                    vm.IsEditorExpanded = false;
                });
                _editPanel = panel;
                PanelService.Instance.OpenPanel(panel);
            }
            else if (!vm.IsEditorExpanded)
            {
                CloseEditPanel();
            }
        }

        private void CloseEditPanel()
        {
            if (_editPanel == null)
            {
                return;
            }

            AssetObjectEditPanelViewModel panel = _editPanel;
            _editPanel = null;
            PanelService.Instance.RemovePanel(panel);
        }

        private static void OpenScriptFile(ObjectBase target)
        {
            ObjectBase capturedTarget = target;

            _ = EngineManager.PostToSimThread(() =>
            {
                if (capturedTarget is not ScriptAsset scriptAsset || !scriptAsset.IsValid)
                {
                    return;
                }

                ScriptDesc scriptDesc = scriptAsset.ScriptDesc;
                string scriptPath = Path.Combine(AssetManager.Instance.AssetRegistry.GetRootPath(), scriptDesc.Path);

                Dispatcher.UIThread.Post(() => CodeEditorService.OpenFile(scriptPath));
            });
        }

        private void UpdateCopyEnabled()
        {
            PART_CopyMenuItem.IsEnabled = (DataContext as ObjectPropertyViewModel)?.GetCopyText() != null;
        }

        private async void OnCopyClicked(object? sender, RoutedEventArgs e)
        {
            if (DataContext is not ObjectPropertyViewModel vm || vm.GetCopyText() is not string text)
            {
                return;
            }

            var clipboard = TopLevel.GetTopLevel(this)?.Clipboard;

            if (clipboard != null)
            {
                await clipboard.SetTextAsync(text);
            }
        }

        private async void OnPasteClicked(object? sender, RoutedEventArgs e)
        {
            if (DataContext is not ObjectPropertyViewModel vm)
            {
                return;
            }

            var clipboard = TopLevel.GetTopLevel(this)?.Clipboard;

            if (clipboard == null)
            {
                return;
            }

            string? text = await clipboard.TryGetTextAsync();
            vm.PasteFromText(text);
        }

        private void OnPickerToggleClicked(object? sender, RoutedEventArgs e)
        {
            // Browse mode: clear the current name so the drop-down lists every
            // type-compatible asset (limited to the top N) instead of just the
            // current selection. It is restored on lost focus if nothing is picked.
            if (DataContext is ObjectPropertyViewModel vm)
            {
                vm.PickerFilter = string.Empty;
            }

            PART_PickerBox.IsDropDownOpen = true;
            PART_PickerBox.Focus();
        }

        private void OnPickerBoxLostFocus(object? sender, FocusChangedEventArgs e)
        {
            if (DataContext is ObjectPropertyViewModel vm)
            {
                vm.ResetFilterToSelection();
            }
        }

        private void OnPickerSelectionChanged(object? sender, SelectionChangedEventArgs e)
        {
            if (e.AddedItems.Count == 0 || DataContext is not ObjectPropertyViewModel vm)
            {
                return;
            }

            if (e.AddedItems[0] is AssetPickerItemViewModel item)
            {
                vm.CommitPickerItem(item);
            }
        }
    }
}
