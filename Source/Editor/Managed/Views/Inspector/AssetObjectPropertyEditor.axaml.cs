using System;
using System.Threading.Tasks;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;
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

            PART_EditButton.Click += OnEditClicked;

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

        private void OnPickerBoxLostFocus(object? sender, RoutedEventArgs e)
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

        private void OnEditClicked(object? sender, RoutedEventArgs e)
        {
            if (DataContext is not ObjectPropertyViewModel vm || !vm.HasSubObject || vm.SubObject == null)
            {
                return;
            }

            var panel = new AssetObjectEditPanelViewModel(vm);
            PanelService.Instance.OpenPanel(panel);
        }
    }
}
