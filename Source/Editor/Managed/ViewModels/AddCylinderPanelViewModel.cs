using System;
using System.Threading;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.ViewModels
{
    public class AddCylinderPanelViewModel : EditorPanelViewModel
    {
        private const double MinSize = 0.05;
        private const double MaxRadius = 10.0;
        private const double MaxHeight = 20.0;
        private const int MinSegments = 3;
        private const int MaxSegments = 128;

        private readonly EditorSubsystem _editorSubsystem;
        private readonly Action<bool> _onCompleted;

        private double _radius = 0.5;
        private double _height = 1.0;
        private int _numSegments = 32;
        private int _isUpdatingPreview;
        private bool _previewDirty;
        private bool _resolved;

        public double Radius
        {
            get => _radius;
            set
            {
                if (SetProperty(ref _radius, Math.Clamp(value, MinSize, MaxRadius)))
                {
                    OnPropertyChanged(nameof(RadiusText));
                    UpdatePreview();
                }
            }
        }

        public string RadiusText => $"{_radius:0.0#} m";

        public double Height
        {
            get => _height;
            set
            {
                if (SetProperty(ref _height, Math.Clamp(value, MinSize, MaxHeight)))
                {
                    OnPropertyChanged(nameof(HeightText));
                    UpdatePreview();
                }
            }
        }

        public string HeightText => $"{_height:0.0#} m";

        public int NumSegments
        {
            get => _numSegments;
            set
            {
                if (SetProperty(ref _numSegments, Math.Clamp(value, MinSegments, MaxSegments)))
                {
                    UpdatePreview();
                }
            }
        }

        public double MinSizeValue => MinSize;
        public double MaxRadiusValue => MaxRadius;
        public double MaxHeightValue => MaxHeight;
        public int MinSegmentsValue => MinSegments;
        public int MaxSegmentsValue => MaxSegments;

        public ICommand ConfirmCommand { get; }
        public ICommand CancelCommand { get; }

        public AddCylinderPanelViewModel(EditorSubsystem? editorSubsystem, Action<bool>? onCompleted)
            : base("Add Cylinder")
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
                _previewDirty = true;
                return;
            }

            _previewDirty = false;

            float radius = (float)_radius;
            float height = (float)_height;
            uint segments = (uint)_numSegments;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    _editorSubsystem.UpdateCylinderPreview(radius, height, segments);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Failed to update cylinder preview: {ex.Message}");
                }
                finally
                {
                    Dispatcher.UIThread.Post(() =>
                    {
                        _isUpdatingPreview = 0;

                        if (_previewDirty && !_resolved)
                        {
                            UpdatePreview();
                        }
                    });
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
                    Logger.Log(LogLevel.Warning, $"Failed to commit cylinder: {ex.Message}");
                }
            });

            _onCompleted(true);
            PanelService.Instance.RemovePanel(this);
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
                    Logger.Log(LogLevel.Warning, $"Failed to cancel cylinder preview: {ex.Message}");
                }
            });

            _onCompleted(false);
            PanelService.Instance.RemovePanel(this);
        }
    }
}
