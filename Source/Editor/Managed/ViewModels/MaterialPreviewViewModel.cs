using System;
using System.Runtime.InteropServices;
using Avalonia;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Services;

namespace Hyperion.Editor.ViewModels
{
    /// <summary>
    /// Live preview of a material, shown above its properties in the asset editing panel. The native
    /// renderer draws the material onto a sphere offscreen and we blit the result into a bitmap; moving
    /// the mouse across the image aims the key light rather than the camera.
    /// </summary>
    public sealed class MaterialPreviewViewModel : ViewModelBase, IDisposable
    {
        /// <summary>Matches MaterialPreviewRenderer::PreviewSize on the native side.</summary>
        private const int PreviewSize = 256;

        private readonly EditorSubsystem _editorSubsystem;
        private readonly uint _bucketIndex;
        private readonly Name _assetName;

        private DelegateHandler? _onPreviewUpdatedHandler;

        // An Image only redraws when its Source reference changes, so frames alternate between two
        // bitmaps rather than repeatedly rewriting one.
        private readonly WriteableBitmap?[] _bitmaps = new WriteableBitmap?[2];
        private int _bitmapIndex;

        private byte[]? _frameBuffer;
        private bool _isDisposed;

        public MaterialPreviewViewModel(EditorSubsystem editorSubsystem, uint bucketIndex, Name assetName)
        {
            _editorSubsystem = editorSubsystem ?? throw new ArgumentNullException(nameof(editorSubsystem));
            _bucketIndex = bucketIndex;
            _assetName = assetName;

            _onPreviewUpdatedHandler = _editorSubsystem.GetOnMaterialPreviewUpdatedDelegate().Bind(() =>
            {
                Dispatcher.UIThread.Post(PullLatestFrame);
            });

            _ = EngineManager.PostToSimThread(() => _editorSubsystem.BeginMaterialPreview(_bucketIndex, _assetName));
        }

        private Bitmap? _previewImage;

        /// <summary>The most recently rendered frame, or null until the first one arrives.</summary>
        public Bitmap? PreviewImage
        {
            get => _previewImage;
            private set
            {
                if (SetProperty(ref _previewImage, value))
                {
                    OnPropertyChanged(nameof(HasPreview));
                }
            }
        }

        public bool HasPreview => _previewImage != null;

        /// <summary>True when this preview is already showing the given asset.</summary>
        public bool Matches(uint bucketIndex, Name assetName)
        {
            return _bucketIndex == bucketIndex && _assetName == assetName;
        }

        /// <summary>
        /// Re-renders the preview. Called whenever a property in the panel is edited - the material's
        /// render proxy has already been updated by then, so the next frame picks the change up.
        /// </summary>
        public void Invalidate()
        {
            if (_isDisposed)
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() => _editorSubsystem.InvalidateMaterialPreview());
        }

        /// <summary>
        /// Aims the key light from a pointer position over the preview, both normalised to 0..1.
        /// Dragging left/right swings the light around the sphere, up/down raises and lowers it.
        /// </summary>
        public void SetLightFromNormalizedPosition(double x, double y)
        {
            if (_isDisposed)
            {
                return;
            }

            float yaw = (float)((Math.Clamp(x, 0.0, 1.0) - 0.5) * 2.0 * Math.PI);
            float pitch = (float)((0.5 - Math.Clamp(y, 0.0, 1.0)) * Math.PI);

            _ = EngineManager.PostToSimThread(() => _editorSubsystem.SetMaterialPreviewLightAngles(yaw, pitch));
        }

        private void PullLatestFrame()
        {
            Dispatcher.UIThread.VerifyAccess();

            if (_isDisposed)
            {
                return;
            }

            _bitmapIndex ^= 1;

            WriteableBitmap bitmap = _bitmaps[_bitmapIndex] ??= new WriteableBitmap(
                new PixelSize(PreviewSize, PreviewSize),
                new Vector(96, 96),
                PixelFormat.Rgba8888,
                AlphaFormat.Unpremul);

            const int rowBytes = PreviewSize * 4;

            _frameBuffer ??= new byte[rowBytes * PreviewSize];

            ulong written;
            uint width;
            uint height;

            unsafe
            {
                fixed (byte* pixels = _frameBuffer)
                {
                    written = EditorSubsystem_CopyMaterialPreviewFrame(
                        _editorSubsystem.NativeAddress,
                        (IntPtr)pixels,
                        (ulong)_frameBuffer.Length,
                        out width,
                        out height);
                }
            }

            if (written == 0 || width != PreviewSize || height != PreviewSize)
            {
                return;
            }

            using (ILockedFramebuffer framebuffer = bitmap.Lock())
            {
                // The native frame is tightly packed; the locked bitmap may carry stride padding, so the
                // rows are copied one at a time rather than as a single block.
                for (int y = 0; y < PreviewSize; y++)
                {
                    Marshal.Copy(
                        _frameBuffer,
                        y * rowBytes,
                        framebuffer.Address + (y * framebuffer.RowBytes),
                        rowBytes);
                }
            }

            PreviewImage = bitmap;
        }

        public void Dispose()
        {
            if (_isDisposed)
            {
                return;
            }

            _isDisposed = true;

            _onPreviewUpdatedHandler?.Remove();
            _onPreviewUpdatedHandler?.Dispose();
            _onPreviewUpdatedHandler = null;

            _ = EngineManager.PostToSimThread(() => _editorSubsystem.EndMaterialPreview());

            PreviewImage = null;

            for (int i = 0; i < _bitmaps.Length; i++)
            {
                _bitmaps[i]?.Dispose();
                _bitmaps[i] = null;
            }
        }

        [DllImport("hyperion", EntryPoint = "EditorSubsystem_CopyMaterialPreviewFrame")]
        private static extern ulong EditorSubsystem_CopyMaterialPreviewFrame(
            IntPtr subsystem,
            IntPtr dest,
            ulong destSize,
            out uint outWidth,
            out uint outHeight);
    }
}
