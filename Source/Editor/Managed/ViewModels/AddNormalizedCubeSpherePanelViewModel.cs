using System;
using System.Threading;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;
using Hyperion.Editor.Services;

/// @TODO Instead of doing a whole panel, just do an overlay that shows inline on the top of the screen.

namespace Hyperion.Editor.ViewModels
{
    public class AddNormalizedCubeSpherePanelViewModel : EditorPanelViewModel
    {
        private const int MinDivisions = 1;
        private const int MaxDivisions = 32;

        private readonly EditorSubsystem _editorSubsystem;
        private readonly Action<bool> _onCompleted;

        private int _numDivisions = 8;
        private int _isUpdatingPreview;
        private bool _resolved;

        public int NumDivisions
        {
            get => _numDivisions;
            set
            {
                int clamped = Math.Clamp(value, MinDivisions, MaxDivisions);

                if (SetProperty(ref _numDivisions, clamped))
                {
                    UpdatePreview();
                }
            }
        }

        public int MinDivisionsValue => MinDivisions;
        public int MaxDivisionsValue => MaxDivisions;

        public ICommand ConfirmCommand { get; }
        public ICommand CancelCommand { get; }

        public AddNormalizedCubeSpherePanelViewModel(EditorSubsystem editorSubsystem, Action<bool> onCompleted)
            : base("Add Normalized Cube Sphere")
        {
            _editorSubsystem = editorSubsystem ?? throw new ArgumentNullException(nameof(editorSubsystem));
            _onCompleted = onCompleted ?? throw new ArgumentNullException(nameof(onCompleted));

            ConfirmCommand = new RelayCommand(OnConfirm);
            CancelCommand = new RelayCommand(OnCancel);

            OnClosed = OnCancel;

            UpdatePreview();
        }

        private void UpdatePreview()
        {
            if (Interlocked.CompareExchange(ref _isUpdatingPreview, 1, 0) == 1)
            {
                return;
            }

            uint divisions = (uint)_numDivisions;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    _editorSubsystem.UpdateNormalizedCubeSpherePreview(divisions);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Failed to update cube sphere preview: {ex.Message}");
                }
                finally
                {
                    Dispatcher.UIThread.Post(() => _isUpdatingPreview = 0);
                }
            });
        }

        private void OnConfirm()
        {
            if (_resolved)
                return;
            _resolved = true;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    _editorSubsystem.CommitMeshPreview();
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Failed to commit cube sphere: {ex.Message}");
                }
            });

            _onCompleted(true);
            PanelService.Instance.ClosePanel();
        }

        private void OnCancel()
        {
            if (_resolved)
                return;
            _resolved = true;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    _editorSubsystem.CancelMeshPreview();
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Failed to cancel cube sphere preview: {ex.Message}");
                }
            });

            _onCompleted(false);
            PanelService.Instance.ClosePanel();
        }
    }
}
