using System;
using System.Collections.Generic;
using Hyperion;

namespace Hyperion.Editor.Services
{
    /// <summary>Outcome of an <see cref="AssetImportService.ImportWithDialog"/> request.</summary>
    public readonly struct AssetImportResult
    {
        public uint BucketIndex { get; }
        public Name AssetName { get; }

        /// <summary>Empty on success or cancellation.</summary>
        public string Error { get; }

        public bool Succeeded => string.IsNullOrEmpty(Error) && AssetName.Valid;
        public bool Cancelled => string.IsNullOrEmpty(Error) && !AssetName.Valid;

        public AssetImportResult(uint bucketIndex, Name assetName, string error)
        {
            BucketIndex = bucketIndex;
            AssetName = assetName;
            Error = error ?? string.Empty;
        }
    }

    /// <summary>
    /// Bridges EditorSubsystem.RequestAssetImport to managed callers: each request gets an id, and the
    /// single OnAssetImportCompleted subscription routes the result back to whoever asked.
    /// </summary>
    public static class AssetImportService
    {
        private static readonly object _lock = new();
        private static readonly Dictionary<uint, Action<AssetImportResult>> _pending = new();
        private static uint _nextRequestId = 1;

        private static EditorSubsystem? _boundSubsystem;
        private static DelegateHandler? _onCompletedHandler;

        /// <summary>
        /// Shows a file dialog, imports the chosen file into the asset registry and invokes
        /// <paramref name="onCompleted"/> with the result. Must be called on the sim thread; the callback
        /// may run on any thread.
        /// </summary>
        public static void ImportWithDialog(Action<AssetImportResult> onCompleted)
        {
            Request(onCompleted, (subsystem, requestId) => subsystem.RequestAssetImport(requestId));
        }

        public static void ImportFromPath(string filePath, Action<AssetImportResult> onCompleted)
        {
            Request(onCompleted, (subsystem, requestId) => subsystem.RequestAssetImportFromPath(requestId, filePath));
        }

        private static void Request(Action<AssetImportResult> onCompleted, Action<EditorSubsystem, uint> send)
        {
            EditorSubsystem? subsystem = EngineManager.EditorGame?.EditorSubsystem;

            if (subsystem == null)
            {
                onCompleted(new AssetImportResult(0, default, "The editor is not ready to import assets."));
                return;
            }

            uint requestId;

            lock (_lock)
            {
                EnsureBound(subsystem);

                requestId = _nextRequestId++;
                _pending[requestId] = onCompleted;
            }

            send(subsystem, requestId);
        }

        private static void EnsureBound(EditorSubsystem subsystem)
        {
            if (_onCompletedHandler != null && ReferenceEquals(_boundSubsystem, subsystem))
            {
                return;
            }

            _onCompletedHandler?.Remove();

            _boundSubsystem = subsystem;
            _onCompletedHandler = subsystem.GetOnAssetImportCompletedDelegate().Bind((uint requestId, uint bucketIndex, Name assetName, string error) =>
            {
                Action<AssetImportResult>? callback;

                lock (_lock)
                {
                    if (!_pending.Remove(requestId, out callback))
                    {
                        return;
                    }
                }

                callback(new AssetImportResult(bucketIndex, assetName, error));
            });
        }
    }
}
