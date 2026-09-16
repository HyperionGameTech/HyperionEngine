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

            viewModel.Preview.SetLightFromNormalizedPosition(position.X / width, position.Y / height);
        }
    }
}
