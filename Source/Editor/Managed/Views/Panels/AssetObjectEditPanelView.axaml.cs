using Avalonia.Controls;
using Avalonia.Input;
using Hyperion.Editor.ViewModels;

namespace Hyperion.Editor.Views.Panels
{
    public partial class AssetObjectEditPanelView : UserControl
    {
        public AssetObjectEditPanelView()
        {
            InitializeComponent();
        }

        private void OnPreviewPointerPressed(object? sender, PointerPressedEventArgs e)
        {
            if (sender is not Control surface)
            {
                return;
            }

            if (!e.GetCurrentPoint(surface).Properties.IsLeftButtonPressed)
            {
                return;
            }

            // Captured so a drag that leaves the preview keeps steering the light until the button goes up.
            e.Pointer.Capture(surface);

            AimLight(surface, e);
        }

        private void OnPreviewPointerMoved(object? sender, PointerEventArgs e)
        {
            if (sender is not Control surface)
            {
                return;
            }

            if (!e.GetCurrentPoint(surface).Properties.IsLeftButtonPressed)
            {
                return;
            }

            AimLight(surface, e);
        }

        private void OnPreviewPointerReleased(object? sender, PointerReleasedEventArgs e)
        {
            e.Pointer.Capture(null);
        }

        private void AimLight(Control surface, PointerEventArgs e)
        {
            if (DataContext is not AssetObjectEditPanelViewModel viewModel || viewModel.Preview == null)
            {
                return;
            }

            double width = surface.Bounds.Width;
            double height = surface.Bounds.Height;

            if (width <= 0 || height <= 0)
            {
                return;
            }

            Avalonia.Point position = e.GetPosition(surface);

            // The preview is square and centred in the surface, so aiming is normalised against the
            // image rather than the whole panel.
            double size = System.Math.Min(width, height);
            double originX = (width - size) * 0.5;
            double originY = (height - size) * 0.5;

            viewModel.Preview.SetLightFromNormalizedPosition((position.X - originX) / size, (position.Y - originY) / size);
        }
    }
}
