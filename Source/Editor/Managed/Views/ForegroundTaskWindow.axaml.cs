using System;
using System.ComponentModel;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Threading;
using Hyperion.Editor.ViewModels;

namespace Hyperion.Editor.Views
{
    public partial class ForegroundTaskWindow : Window
    {
        private const double MarginRight = 16;
        private const double MarginBottom = 32;

        private Window? _owner;

        public ForegroundTaskWindow()
        {
            InitializeComponent();

            SizeChanged += (_, _) => RepositionRelativeToOwner();
        }

        public void AttachTo(Window owner)
        {
            _owner = owner;
            Owner = owner;

            owner.PositionChanged += (_, _) => RepositionRelativeToOwner();
            owner.SizeChanged += (_, _) => RepositionRelativeToOwner();

            if (DataContext is ForegroundTaskViewModel viewModel)
            {
                viewModel.PropertyChanged += OnViewModelPropertyChanged;

                if (viewModel.IsVisible)
                {
                    RepositionRelativeToOwner();
                    Show();
                }
            }
        }

        private void OnViewModelPropertyChanged(object? sender, PropertyChangedEventArgs e)
        {
            if (e.PropertyName != nameof(ForegroundTaskViewModel.IsVisible) || DataContext is not ForegroundTaskViewModel viewModel)
            {
                return;
            }

            Dispatcher.UIThread.Post(() =>
            {
                if (viewModel.IsVisible)
                {
                    RepositionRelativeToOwner();

                    if (!IsVisible)
                    {
                        Show();
                    }
                }
                else if (IsVisible)
                {
                    Hide();
                }
            });
        }

        private void RepositionRelativeToOwner()
        {
            if (_owner == null || Width <= 0 || Height <= 0)
            {
                return;
            }

            PixelPoint ownerBottomRight = _owner.PointToScreen(new Point(_owner.Bounds.Width, _owner.Bounds.Height));

            Position = new PixelPoint(
                (int)Math.Round(ownerBottomRight.X - Width - MarginRight),
                (int)Math.Round(ownerBottomRight.Y - Height - MarginBottom));
        }
    }
}
