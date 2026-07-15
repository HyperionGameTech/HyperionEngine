using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Hyperion.Editor.ViewModels;

namespace Hyperion.Editor.Views.Inspector
{
    public partial class SubclassPropertyEditor : UserControl
    {
        public SubclassPropertyEditor()
        {
            InitializeComponent();

            PART_PickerBox.AsyncPopulator = (search, token) =>
            {
                if (DataContext is not ObjectPropertyViewModel vm)
                    return Task.FromResult<IEnumerable<object>>(Array.Empty<object>());

                string filter = search ?? string.Empty;

                var results = vm.AvailableSubclasses
                    .Where(name => filter.Length == 0 || name.Contains(filter, StringComparison.OrdinalIgnoreCase))
                    .Cast<object>()
                    .ToList();

                return Task.FromResult<IEnumerable<object>>(results);
            };

            PART_PickerToggle.Click += OnPickerToggleClicked;
            PART_PickerBox.LostFocus += OnPickerBoxLostFocus;
            PART_PickerBox.SelectionChanged += OnPickerSelectionChanged;
        }

        private void OnPickerToggleClicked(object? sender, RoutedEventArgs e)
        {
            if (DataContext is ObjectPropertyViewModel vm)
                vm.SubclassFilter = string.Empty;

            PART_PickerBox.IsDropDownOpen = true;
            PART_PickerBox.Focus();
        }

        private void OnPickerBoxLostFocus(object? sender, RoutedEventArgs e)
        {
            if (DataContext is ObjectPropertyViewModel vm)
                vm.SubclassFilter = string.Empty;
        }

        private void OnPickerSelectionChanged(object? sender, SelectionChangedEventArgs e)
        {
            if (e.AddedItems.Count == 0 || DataContext is not ObjectPropertyViewModel vm)
                return;

            if (e.AddedItems[0] is string className)
            {
                vm.SelectedSubclass = className;
            }
        }
    }
}
