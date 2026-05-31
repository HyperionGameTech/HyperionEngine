using System;
using System.Collections.ObjectModel;
using System.Collections.Generic;
using System.Windows.Input;
using Hyperion.Editor.Services;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public class ConsoleViewModel : ViewModelBase
    {
        public ReadOnlyObservableCollection<LogEntry> Logs => ConsoleService.Instance.Logs;

        private string _commandText;
        public string CommandText
        {
            get => _commandText;
            set
            {
                if (SetProperty(ref _commandText, value))
                {
                    UpdateCompletions();
                }
            }
        }

        private IReadOnlyList<string> _completions = Array.Empty<string>();
        public IReadOnlyList<string> Completions
        {
            get => _completions;
            private set => SetProperty(ref _completions, value);
        }

        private int _selectedCompletionIndex = -1;
        public int SelectedCompletionIndex
        {
            get => _selectedCompletionIndex;
            set
            {
                if (SetProperty(ref _selectedCompletionIndex, value))
                {
                    OnPropertyChanged(nameof(SelectedCompletion));
                }
            }
        }

        public string SelectedCompletion
        {
            get
            {
                if (Completions == null || Completions.Count == 0 || SelectedCompletionIndex < 0 || SelectedCompletionIndex >= Completions.Count)
                    return string.Empty;

                return Completions[SelectedCompletionIndex];
            }
        }

        public bool HasCompletions => Completions != null && Completions.Count > 0;

        public ICommand ExecuteCommand { get; }
        public ICommand ClearCommand { get; }

        public ConsoleViewModel()
        {
            _commandText = string.Empty;
            ExecuteCommand = new RelayCommand(Execute, () => !string.IsNullOrWhiteSpace(CommandText));
            ClearCommand = new RelayCommand(Clear);
        }

        private void UpdateCompletions()
        {
            string prefix = GetCurrentWord();
            Completions = ConsoleService.Instance.GetCompletions(prefix);
            SelectedCompletionIndex = Completions.Count > 0 ? 0 : -1;
            OnPropertyChanged(nameof(HasCompletions));
        }

        private string GetCurrentWord()
        {
            if (string.IsNullOrEmpty(CommandText))
                return string.Empty;

            int lastSpace = CommandText.LastIndexOf(' ');
            return lastSpace >= 0 ? CommandText[(lastSpace + 1)..] : CommandText;
        }

        public void SelectCompletion()
        {
            string completion = SelectedCompletion;
            if (string.IsNullOrEmpty(completion))
                return;

            int lastSpace = CommandText.LastIndexOf(' ');
            if (lastSpace >= 0)
            {
                CommandText = CommandText[..(lastSpace + 1)] + completion;
            }
            else
            {
                CommandText = completion;
            }

            ClearCompletions();
        }

        public void ClearCompletions()
        {
            Completions = Array.Empty<string>();
            SelectedCompletionIndex = -1;
            OnPropertyChanged(nameof(HasCompletions));
        }

        private void Execute()
        {
            if (string.IsNullOrWhiteSpace(CommandText)) return;

            string[] args = CommandText.Split(' ', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries);

            if (args.Length != 0)
            {
                try
                {
                    ConsoleService.Instance.ExecuteCommand(args);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Error, $"Failed to execute command: {ex.Message}");
                }
                finally
                {
                    CommandText = string.Empty;
                    Completions = Array.Empty<string>();
                    SelectedCompletionIndex = -1;
                    OnPropertyChanged(nameof(HasCompletions));
                }
            }
        }

        private void Clear()
        {
            ConsoleService.Instance.ClearLogs();
        }
    }
}
