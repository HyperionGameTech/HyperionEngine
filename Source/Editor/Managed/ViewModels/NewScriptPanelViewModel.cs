using System;
using System.Windows.Input;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.ViewModels
{
    public class ScriptLanguageEntry
    {
        public string DisplayName { get; }
        public string LanguageArg { get; }
        public string Extension { get; }

        public ScriptLanguageEntry(string displayName, string languageArg, string extension)
        {
            DisplayName = displayName;
            LanguageArg = languageArg;
            Extension = extension;
        }
    }

    public class NewScriptPanelViewModel : EditorPanelViewModel
    {
        // Invoked with (name, languageArg); called with empty strings on cancel.
        private readonly Action<string, string> _onCompleted;

        private string _scriptName = "NewScript";
        private ScriptLanguageEntry _selectedLanguage = null!;

        public ScriptLanguageEntry[] Languages { get; } =
        {
            new ScriptLanguageEntry("HypScript", "hypscript", ".hyp"),
            new ScriptLanguageEntry("Strata", "strata", ".strata"),
            new ScriptLanguageEntry("C#", "csharp", ".cs")
        };

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
                    OnPropertyChanged(nameof(IsHypScriptSelected));
                    OnPropertyChanged(nameof(IsStrataSelected));
                    OnPropertyChanged(nameof(IsCSharpSelected));
                }
            }
        }

        // Radio buttons bind to these; each setter drives SelectedLanguage.
        public bool IsHypScriptSelected
        {
            get => _selectedLanguage.LanguageArg == "hypscript";
            set { if (value) SelectedLanguage = Languages[0]; }
        }

        public bool IsStrataSelected
        {
            get => _selectedLanguage.LanguageArg == "strata";
            set { if (value) SelectedLanguage = Languages[1]; }
        }

        public bool IsCSharpSelected
        {
            get => _selectedLanguage.LanguageArg == "csharp";
            set { if (value) SelectedLanguage = Languages[2]; }
        }

        public ICommand ConfirmCommand { get; }
        public ICommand CancelCommand { get; }

        public NewScriptPanelViewModel(Action<string, string> onCompleted)
            : base("New Script")
        {
            _onCompleted = onCompleted ?? throw new ArgumentNullException(nameof(onCompleted));

            _selectedLanguage = Languages[0];

            ConfirmCommand = new RelayCommand(OnConfirm);
            CancelCommand = new RelayCommand(OnCancel);
        }

        private void OnConfirm()
        {
            // Script names are identifiers; they must not contain spaces because
            // the command parses args by splitting on spaces.
            string name = (ScriptName ?? string.Empty).Trim();

            if (string.IsNullOrWhiteSpace(name) || name.Contains(' '))
            {
                return;
            }

            _onCompleted(name, SelectedLanguage.LanguageArg);
            PanelService.Instance.ClosePanel();
        }

        private void OnCancel()
        {
            _onCompleted(string.Empty, string.Empty);
            PanelService.Instance.ClosePanel();
        }
    }
}
