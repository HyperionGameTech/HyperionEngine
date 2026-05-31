using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;
using Hyperion.Editor.ViewModels;

namespace Hyperion.Editor.Views.Inspector
{
    public partial class AssetObjectPropertyEditor : UserControl
    {
        public AssetObjectPropertyEditor()
        {
            InitializeComponent();
            PART_EditButton.Click += OnEditClicked;
        }

        private async void OnEditClicked(object? sender, RoutedEventArgs e)
        {
            if (DataContext is not ObjectPropertyViewModel vm || !vm.HasSubObject || vm.SubObject == null)
            {
                return;
            }

            var owner = TopLevel.GetTopLevel(this) as Window;

            if (owner == null)
            {
                return;
            }

            var dialog = new AssetObjectEditWindow(vm.Label, vm.AssetPathDisplay, vm.SubObject);
            await dialog.ShowDialog(owner);
        }
    }
}
