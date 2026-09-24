using System;
using Avalonia.Controls;
using Avalonia.Input;
using Hyperion.Editor.ViewModels;

namespace Hyperion.Editor.Views
{
    public partial class StartScreenWindow : Window
    {
        public StartScreenWindow()
        {
            InitializeComponent();
        }

        protected override void OnKeyDown(KeyEventArgs e)
        {
            if (e.Key == Key.Escape && DataContext is StartScreenViewModel { IsBusy: false } viewModel)
            {
                viewModel.NewProjectCommand.Execute(null);
                e.Handled = true;
            }

            base.OnKeyDown(e);
        }

        protected override void OnClosed(EventArgs e)
        {
            (DataContext as IDisposable)?.Dispose();

            base.OnClosed(e);
        }
    }
}
