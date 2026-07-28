using System;
using System.Globalization;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class TransformViewModel : InspectorPropertyViewModelBase
    {
        private readonly Vec3fViewModel _translation;
        private readonly Vec3fViewModel _rotationEuler;
        private readonly Vec3fViewModel _scale;

        private bool _isExpanded = true;

        public TransformViewModel(ObjectBase target, Property property, bool isReadOnly)
            : base(target, property, isReadOnly)
        {
            _translation = new Vec3fViewModel(target, property, isReadOnly, ReadTranslation, WriteTranslation);
            _rotationEuler = new Vec3fViewModel(target, property, isReadOnly, ReadRotationEuler, WriteRotationEuler);
            _scale = new Vec3fViewModel(target, property, isReadOnly, ReadScale, WriteScale);
        }

        public TransformViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly)
            : base(classAddress, targetAddressResolver, property, isReadOnly)
        {
            _translation = new Vec3fViewModel(classAddress, targetAddressResolver, property, isReadOnly, ReadTranslation, WriteTranslation);
            _rotationEuler = new Vec3fViewModel(classAddress, targetAddressResolver, property, isReadOnly, ReadRotationEuler, WriteRotationEuler);
            _scale = new Vec3fViewModel(classAddress, targetAddressResolver, property, isReadOnly, ReadScale, WriteScale);
        }

        // The three vector editors read and write through this view model's own property, so they
        // share its container hooks instead of talking to the containing struct/object themselves.
        // PostWriteCallback stays on this view model - SetPropertyValue here already invokes it.
        public override Action? PreWriteCallback
        {
            get => base.PreWriteCallback;
            set
            {
                base.PreWriteCallback = value;

                _translation.PreWriteCallback = value;
                _rotationEuler.PreWriteCallback = value;
                _scale.PreWriteCallback = value;
            }
        }

        public override Action? ValueChangedCallback
        {
            get => base.ValueChangedCallback;
            set
            {
                base.ValueChangedCallback = value;

                _translation.ValueChangedCallback = value;
                _rotationEuler.ValueChangedCallback = value;
                _scale.ValueChangedCallback = value;
            }
        }

        public Vec3fViewModel Translation => _translation;
        public Vec3fViewModel Rotation => _rotationEuler;
        public Vec3fViewModel Scale => _scale;

        public bool IsExpanded
        {
            get => _isExpanded;
            set => SetProperty(ref _isExpanded, value);
        }

        public override void RefreshValue()
        {
            if (!BeginRefresh())
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                Transform transform;

                try
                {
                    transform = ReadTransform();
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to read property '{Label}': {ex.Message}");

                    EndRefresh();

                    return;
                }

                Dispatcher.UIThread.Post(() =>
                {
                    try
                    {
                        ApplyModelValue(() =>
                        {
                            Value = $"T:{FormatVec3(transform.Translation)} R:{FormatQuat(transform.Rotation)} S:{FormatVec3(transform.Scale)}";
                        });

                        _translation.RefreshValue();
                        _rotationEuler.RefreshValue();
                        _scale.RefreshValue();
                    }
                    finally
                    {
                        EndRefresh();
                    }
                });
            });
        }

        // Sim thread only.
        private Transform ReadTransform()
        {
            using BoxedValue boxed = GetPropertyValue();
            object? raw = boxed.GetValue();

            return raw is Transform t ? t : Transform.Identity;
        }

        // Sim thread only.
        private void WriteTransform(Transform transform)
        {
            using BoxedValue boxed = new BoxedValue(transform);
            CommitPropertyChange($"Set {Label}", boxed);
        }

        private Vec3f ReadTranslation() => ReadTransform().Translation;
        private Vec3f ReadScale() => ReadTransform().Scale;
        private Vec3f ReadRotationEuler()
        {
            Quat4f q = ReadTransform().Rotation;
            return QuaternionToEulerDegrees(q);
        }

        private void WriteTranslation(Vec3f value)
        {
            Transform t = ReadTransform();
            t.Translation = value;
            WriteTransform(t);
        }

        private void WriteScale(Vec3f value)
        {
            Transform t = ReadTransform();
            t.Scale = value;
            WriteTransform(t);
        }

        private void WriteRotationEuler(Vec3f eulerDegrees)
        {
            Transform t = ReadTransform();
            t.Rotation = EulerDegreesToQuaternion(eulerDegrees);
            WriteTransform(t);
        }

        private static string FormatVec3(Vec3f v)
        {
            const string Format = "F3";
            return $"({v.x.ToString(Format, CultureInfo.InvariantCulture)}, {v.y.ToString(Format, CultureInfo.InvariantCulture)}, {v.z.ToString(Format, CultureInfo.InvariantCulture)})";
        }

        private static string FormatQuat(Quat4f q)
        {
            const string Format = "F3";
            return $"({q.X.ToString(Format, CultureInfo.InvariantCulture)}, {q.Y.ToString(Format, CultureInfo.InvariantCulture)}, {q.Z.ToString(Format, CultureInfo.InvariantCulture)}, {q.W.ToString("F3", CultureInfo.InvariantCulture)})";
        }

        private static Vec3f QuaternionToEulerDegrees(Quat4f q)
        {
            float sinr_cosp = 2f * (q.W * q.X + q.Y * q.Z);
            float cosr_cosp = 1f - 2f * (q.X * q.X + q.Y * q.Y);
            float roll = MathF.Atan2(sinr_cosp, cosr_cosp);

            float sinp = 2f * (q.W * q.Y - q.Z * q.X);
            float pitch = MathF.Abs(sinp) >= 1f ? MathF.CopySign(MathF.PI / 2f, sinp) : MathF.Asin(sinp);

            float siny_cosp = 2f * (q.W * q.Z + q.X * q.Y);
            float cosy_cosp = 1f - 2f * (q.Y * q.Y + q.Z * q.Z);
            float yaw = MathF.Atan2(siny_cosp, cosy_cosp);

            const float rad2deg = 180f / MathF.PI;
            return new Vec3f(roll * rad2deg, pitch * rad2deg, yaw * rad2deg);
        }

        private static Quat4f EulerDegreesToQuaternion(Vec3f eulerDegrees)
        {
            const float DegToRad = MathF.PI / 180f;

            float roll = eulerDegrees.x * DegToRad;
            float pitch = eulerDegrees.y * DegToRad;
            float yaw = eulerDegrees.z * DegToRad;

            float cy = MathF.Cos(yaw * 0.5f);
            float sy = MathF.Sin(yaw * 0.5f);
            float cp = MathF.Cos(pitch * 0.5f);
            float sp = MathF.Sin(pitch * 0.5f);
            float cr = MathF.Cos(roll * 0.5f);
            float sr = MathF.Sin(roll * 0.5f);

            Quat4f q = new Quat4f
            {
                W = cr * cp * cy + sr * sp * sy,
                X = sr * cp * cy - cr * sp * sy,
                Y = cr * sp * cy + sr * cp * sy,
                Z = cr * cp * sy - sr * sp * cy
            };

            return q;
        }
    }
}