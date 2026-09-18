using System;
using System.Collections.Generic;
using System.IO;
using System.Threading.Tasks;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.Services
{
    /// <summary>
    /// Bridges the native asset thumbnail renderer to the content browser: queues render requests,
    /// listens for completions, and decodes the cached PNGs into Avalonia images.
    /// </summary>
    public sealed class ThumbnailService : IDisposable
    {
        public static ThumbnailService? Instance { get; private set; }

        private readonly EditorSubsystem _editorSubsystem;
        private DelegateHandler? _onThumbnailReadyHandler;

        /// <summary>
        /// Consumers of a thumbnail, keyed by "bucketIndex/assetName". These persist rather than being
        /// one-shot: the native side re-renders a thumbnail whenever its asset changes, and the tile
        /// showing it needs to pick up each new version.
        /// </summary>
        private readonly Dictionary<string, List<Action<IImage>>> _subscribers = new();

        /// <summary>Decoded thumbnails, keyed the same way. Bounded by the assets actually viewed.</summary>
        private readonly Dictionary<string, IImage> _cache = new();

        public ThumbnailService(EditorSubsystem editorSubsystem)
        {
            _editorSubsystem = editorSubsystem ?? throw new ArgumentNullException(nameof(editorSubsystem));

            _onThumbnailReadyHandler = _editorSubsystem.GetOnThumbnailReadyDelegate().Bind((uint bucketIndex, Name assetName) =>
            {
                Dispatcher.UIThread.Post(() => OnThumbnailReady(bucketIndex, assetName));
            });

            Instance = this;
        }

        private static string MakeKey(uint bucketIndex, string assetName) => $"{bucketIndex}/{assetName}";

        /// <summary>
        /// Requests the thumbnail for an asset. <paramref name="onReady"/> is invoked on the UI thread,
        /// either immediately from the in-memory cache or once the native side has rendered the image.
        /// </summary>
        public void Request(uint bucketIndex, Name assetName, Action<IImage> onReady)
        {
            Dispatcher.UIThread.VerifyAccess();

            string key = MakeKey(bucketIndex, assetName.ToString());

            if (_subscribers.TryGetValue(key, out List<Action<IImage>>? subscribers))
            {
                subscribers.Add(onReady);
            }
            else
            {
                _subscribers[key] = new List<Action<IImage>> { onReady };
            }

            if (_cache.TryGetValue(key, out IImage? cached))
            {
                onReady(cached);
                return;
            }

            _ = EngineManager.PostToSimThread(() => _editorSubsystem.RequestAssetThumbnail(bucketIndex, assetName));
        }

        /// <summary>
        /// Drops queued native requests that have not started yet. Called when the visible asset set
        /// changes so the renderer does not keep working through a bucket the user has left.
        /// </summary>
        public void CancelPending()
        {
            _ = EngineManager.PostToSimThread(() => _editorSubsystem.CancelPendingAssetThumbnails());
        }

        private void OnThumbnailReady(uint bucketIndex, Name assetName)
        {
            Dispatcher.UIThread.VerifyAccess();

            string name = assetName.ToString();
            string key = MakeKey(bucketIndex, name);

            if (!_subscribers.ContainsKey(key))
            {
                // Nothing is showing this any more - the bucket changed, or the tile went away.
                return;
            }

            _ = LoadThumbnailAsync(bucketIndex, assetName, key);
        }

        private async Task LoadThumbnailAsync(uint bucketIndex, Name assetName, string key)
        {
            string path = await EngineManager.PostToSimThread(
                () => _editorSubsystem.GetAssetThumbnailPath(bucketIndex, assetName));

            if (string.IsNullOrEmpty(path))
            {
                return;
            }

            // Decoding touches the disk, so keep it off the UI thread. The file is re-read every time
            // rather than served from the cache - this also fires when an asset changed and the native
            // side re-rendered it, so the bytes on disk are not the ones we decoded last time.
            Bitmap? bitmap = await Task.Run<Bitmap?>(() =>
            {
                try
                {
                    using FileStream stream = File.OpenRead(path);
                    return new Bitmap(stream);
                }
                catch (Exception e)
                {
                    Logger.Log(LogLevel.Warning, $"Failed to load thumbnail '{path}': {e.Message}");
                    return null;
                }
            });

            if (bitmap == null)
            {
                return;
            }

            _cache[key] = bitmap;

            if (_subscribers.TryGetValue(key, out List<Action<IImage>>? subscribers))
            {
                foreach (Action<IImage> subscriber in subscribers)
                {
                    subscriber(bitmap);
                }
            }
        }

        /// <summary>
        /// Drops the subscriptions held for the tiles currently on screen. Called when the displayed
        /// asset set is replaced - those view models are about to be discarded, and their callbacks would
        /// otherwise keep them alive and be invoked forever. Decoded images are kept.
        /// </summary>
        public void ClearSubscribers()
        {
            Dispatcher.UIThread.VerifyAccess();

            _subscribers.Clear();
        }

        /// <summary>Forgets every decoded image, e.g. when a project closes.</summary>
        public void Clear()
        {
            Dispatcher.UIThread.VerifyAccess();

            _cache.Clear();
            _subscribers.Clear();
        }

        public void Dispose()
        {
            _onThumbnailReadyHandler?.Remove();
            _onThumbnailReadyHandler?.Dispose();
            _onThumbnailReadyHandler = null;

            if (Instance == this)
            {
                Instance = null;
            }
        }
    }
}
