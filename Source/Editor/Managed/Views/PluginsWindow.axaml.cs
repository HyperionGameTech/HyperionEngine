using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Hyperion.Editor.ViewModels;

namespace Hyperion.Editor.Views
{
    public partial class PluginsWindow : Window
    {
        public PluginsWindow()
        {
            InitializeComponent();

            AddHandler(KeyDownEvent, OnWindowKeyDown, RoutingStrategies.Bubble);
        }

        private void OnWindowKeyDown(object? sender, KeyEventArgs e)
        {
            if (e.Key == Key.Escape)
            {
                Close();
            }
        }

        private async void OnApplyClick(object? sender, RoutedEventArgs e)
        {
            if (DataContext is not PluginsViewModel viewModel)
            {
                return;
            }

            await viewModel.ApplyAsync();

            Close();
        }

        private void OnCloseClick(object? sender, RoutedEventArgs e)
        {
            Close();
        }
    }
}
