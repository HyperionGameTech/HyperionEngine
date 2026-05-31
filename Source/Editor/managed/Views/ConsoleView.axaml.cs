using Avalonia.Controls;
using Avalonia.Markup.Xaml;
using Avalonia.Threading;
using Avalonia;
using Avalonia.Input;
using Hyperion.Editor.ViewModels;

namespace Hyperion.Editor
{
    public partial class ConsoleView : UserControl
    {
        private TextBox? _commandTextBox;
        private ListBox? _completionListBox;
        private ConsoleViewModel? _viewModel;

        public ConsoleView()
        {
            InitializeComponent();
        }

        private void InitializeComponent()
        {
            AvaloniaXamlLoader.Load(this);
            _commandTextBox = this.FindControl<TextBox>("CommandTextBox");
            _completionListBox = this.FindControl<ListBox>("CompletionListBox");

            if (_commandTextBox != null)
            {
                _commandTextBox.KeyDown += OnCommandTextBoxKeyDown;
            }

            DataContextChanged += OnDataContextChanged;
        }

        private void OnDataContextChanged(object? sender, EventArgs e)
        {
            _viewModel = DataContext as ConsoleViewModel;
        }

        private void OnCommandTextBoxKeyDown(object? sender, KeyEventArgs e)
        {
            if (_commandTextBox == null)
                return;

            _viewModel = this.DataContext as ConsoleViewModel;
            if (_viewModel == null)
                return;

            switch (e.Key)
            {
                case Key.Tab:
                    if (_viewModel.HasCompletions)
                    {
                        _viewModel.SelectCompletion();
                        _commandTextBox.CaretIndex = _commandTextBox.Text?.Length ?? 0;
                        e.Handled = true;
                    }
                    break;

                case Key.Up:
                    if (_viewModel.HasCompletions)
                    {
                        if (_viewModel.SelectedCompletionIndex > 0)
                        {
                            _viewModel.SelectedCompletionIndex--;
                        }
                        else
                        {
                            _viewModel.SelectedCompletionIndex = _viewModel.Completions.Count - 1;
                        }
                        e.Handled = true;
                    }
                    break;

                case Key.Down:
                    if (_viewModel.HasCompletions)
                    {
                        if (_viewModel.SelectedCompletionIndex < _viewModel.Completions.Count - 1)
                        {
                            _viewModel.SelectedCompletionIndex++;
                        }
                        else
                        {
                            _viewModel.SelectedCompletionIndex = 0;
                        }
                        e.Handled = true;
                    }
                    break;

                case Key.Escape:
                    if (_viewModel.HasCompletions)
                    {
                        _viewModel.ClearCompletions();
                        e.Handled = true;
                    }
                    break;

                case Key.Enter:
                    if (_viewModel.HasCompletions)
                    {
                        _viewModel.SelectCompletion();
                        _commandTextBox.CaretIndex = _commandTextBox.Text?.Length ?? 0;
                        e.Handled = true;
                    }
                    else if (!string.IsNullOrWhiteSpace(_viewModel.CommandText))
                    {
                        _viewModel.ExecuteCommand.Execute(null);
                        e.Handled = true;
                    }
                    break;
            }
        }

        protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e)
        {
            base.OnAttachedToVisualTree(e);
            Dispatcher.UIThread.Post(() => _commandTextBox?.Focus(), DispatcherPriority.Loaded);
        }
    }
}
