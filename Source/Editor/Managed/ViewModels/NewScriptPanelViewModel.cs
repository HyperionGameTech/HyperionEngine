using System;
using System.Windows.Input;
using Hyperion.Editor.Services;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public readonly record struct ScriptLanguageEntry(
        string DisplayName,
        string LanguageArg,
        string Extension);

    public class NewScriptPanelViewModel : EditorPanelViewModel
    {
        // Invoked with (name, languageArg); called with empty strings on cancel.
        private readonly Action<string, string> _onCompleted;

        private string _scriptName = "NewScript";
        private ScriptLanguageEntry _selectedLanguage;

        private static readonly ScriptLanguageEntry StrataLanguage = new ScriptLanguageEntry("Strata", "strata", ".strata");
        private static readonly ScriptLanguageEntry CSharpLanguage = new ScriptLanguageEntry("C#", "csharp", ".cs");

        public bool IsStrataAvailable { get; } = EngineManager.IsStrataEnabled;

        public ScriptLanguageEntry[] Languages { get; }

        public string ScriptName
        {
            get => _scriptName;
            set => SetProperty(ref _scriptName, value);
        }

        public ScriptLanguageEntry SelectedLanguage
        {
            get => _selectedLanguage;
            set
            {
                if (SetProperty(ref _selectedLanguage, value))
                {
                    // Keep the radio buttons in sync with the selection.
                    OnPropertyChanged(nameof(IsStrataSelected));
                    OnPropertyChanged(nameof(IsCSharpSelected));
                }
            }
        }

        // Radio buttons bind to these; each setter drives SelectedLanguage.
        public bool IsStrataSelected
        {
            get => _selectedLanguage.LanguageArg == "strata";
            set { if (value && IsStrataAvailable) SelectedLanguage = StrataLanguage; }
        }

        public bool IsCSharpSelected
        {
            get => _selectedLanguage.LanguageArg == "csharp";
            set { if (value) SelectedLanguage = CSharpLanguage; }
        }

        public ICommand ConfirmCommand { get; }
        public ICommand CancelCommand { get; }

        public NewScriptPanelViewModel(Action<string, string> onCompleted)
            : base("New Script")
        {
            _onCompleted = onCompleted ?? throw new ArgumentNullException(nameof(onCompleted));

            Languages = IsStrataAvailable
                ? new[] { StrataLanguage, CSharpLanguage }
                : new[] { CSharpLanguage };

            _selectedLanguage = Languages[0];

            ConfirmCommand = new RelayCommand(OnConfirm);
            CancelCommand = new RelayCommand(OnCancel);
        }

        private void OnConfirm()
        {
            // Script names are identifiers; they must not contain spaces because the command parses args by splitting on spaces.
            string name = (ScriptName ?? string.Empty).Trim();

            if (string.IsNullOrWhiteSpace(name) || name.Contains(' '))
            {
                return;
            }

            _onCompleted(name, SelectedLanguage.LanguageArg);
            PanelService.Instance.RemovePanel(this);
        }

        private void OnCancel()
        {
            _onCompleted(string.Empty, string.Empty);
            PanelService.Instance.RemovePanel(this);
        }
    }
}
