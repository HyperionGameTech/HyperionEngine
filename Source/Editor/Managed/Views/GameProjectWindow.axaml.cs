using System;
using Avalonia.Controls;
using Avalonia.Input;
using Hyperion.Editor.ViewModels;

namespace Hyperion.Editor.Views
{
    public partial class GameProjectWindow : Window
    {
        public GameProjectWindow()
        {
            InitializeComponent();
        }

        protected override void OnDataContextChanged(EventArgs e)
        {
            base.OnDataContextChanged(e);

            if (DataContext is GameProjectWindowViewModel viewModel)
            {
                viewModel.CloseRequested += Close;
            }
        }

        protected override void OnKeyDown(KeyEventArgs e)
        {
            if (e.Key == Key.Escape)
            {
                Close();
                e.Handled = true;
            }

            base.OnKeyDown(e);
        }
    }
}
