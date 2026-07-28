using System;
using System.Threading;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public class BoundingBoxPropertyViewModel : StructPropertyViewModel
    {
        private static readonly BoundingBox EmptyBoundingBox = new BoundingBox(
            new Vec3f(float.MaxValue, float.MaxValue, float.MaxValue),
            new Vec3f(float.MinValue, float.MinValue, float.MinValue));

        public ICommand ResetCommand { get; }

        public BoundingBoxPropertyViewModel(ObjectBase target, Property property, bool isReadOnly, int depth = 0)
            : base(target, property, isReadOnly, depth)
        {
            ResetCommand = new RelayCommand(OnReset, () => !_isReadOnly);
        }

        public BoundingBoxPropertyViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly, int depth = 0)
            : base(classAddress, targetAddressResolver, property, isReadOnly, depth)
        {
            ResetCommand = new RelayCommand(OnReset, () => !_isReadOnly);
        }

        public BoundingBoxPropertyViewModel(string label, TypeInfo typeInfo, Func<BoxedValue> getter, Action<BoxedValue> setter, bool isReadOnly, int depth = 0)
            : base(label, typeInfo, getter, setter, isReadOnly, depth)
        {
            ResetCommand = new RelayCommand(OnReset, () => !_isReadOnly);
        }

        private void OnReset()
        {
            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    using BoxedValue boxed = new BoxedValue(EmptyBoundingBox);
                    CommitPropertyChange($"Reset {Label}", boxed);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to reset bounding box property '{Label}': {ex.Message}");

                    Dispatcher.UIThread.Post(RefreshValue);
                }
            });
        }
    }
}
