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
            if (Interlocked.CompareExchange(ref _isRefreshing, 1, 0) == 1)
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    Transform transform;
                    using (BoxedValue boxed = GetPropertyValue())
                    {
                        object? raw = boxed.GetValue();
                        transform = raw is Transform t ? t : Transform.Identity;
                    }

                    Dispatcher.UIThread.Post(() =>
                    {
                        _isRefreshing = 0;

                        Value = $"T:{FormatVec3(transform.Translation)} R:{FormatQuat(transform.Rotation)} S:{FormatVec3(transform.Scale)}";

                        _translation.RefreshValue();
                        _rotationEuler.RefreshValue();
                        _scale.RefreshValue();
                    });
                }
                catch (Exception ex)
                {
                    _isRefreshing = 0;

                    Logger.Log(LogLevel.Warning, $"Inspector failed to read property '{_property.Name}': {ex.Message}");
                }
            });
        }

        private Transform ReadTransform()
        {
            Task<Transform> task = EngineManager.PostToSimThread<Transform>(() =>
            {
                using BoxedValue boxed = GetPropertyValue();
                object? raw = boxed.GetValue();

                Logger.Log(LogLevel.Debug, $"ReadTransform got raw value of type: {(raw != null ? raw.GetType().Name : "null")}");

                if (raw is Transform t)
                {
                    return t;
                }
                
                return Transform.Identity;
            });
            
            return task.Result;
        }

        private void WriteTransform(Transform transform)
        {
            if (Interlocked.CompareExchange(ref _isRefreshing, 1, 0) == 1)
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    using BoxedValue boxed = new BoxedValue(transform);
                    SetPropertyValue(boxed);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to write property '{_property.Name}': {ex.Message}");
                }
                finally
                {
                    _isRefreshing = 0;
                }
            });
        }

        private Vec3f ReadTranslation() => ReadTransform().Translation;
        private Vec3f ReadScale() => ReadTransform().Scale;
        private Vec3f ReadRotationEuler()
        {
            Quaternion q = ReadTransform().Rotation;
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
            return $"({v.x.ToString("F3", CultureInfo.InvariantCulture)}, {v.y.ToString("F3", CultureInfo.InvariantCulture)}, {v.z.ToString("F3", CultureInfo.InvariantCulture)})";
        }

        private static string FormatQuat(Quaternion q)
        {
            return $"({q.X.ToString("F3", CultureInfo.InvariantCulture)}, {q.Y.ToString("F3", CultureInfo.InvariantCulture)}, {q.Z.ToString("F3", CultureInfo.InvariantCulture)}, {q.W.ToString("F3", CultureInfo.InvariantCulture)})";
        }

        private static Vec3f QuaternionToEulerDegrees(Quaternion q)
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

        private static Quaternion EulerDegreesToQuaternion(Vec3f eulerDegrees)
        {
            const float deg2rad = MathF.PI / 180f;
            float roll = eulerDegrees.x * deg2rad;
            float pitch = eulerDegrees.y * deg2rad;
            float yaw = eulerDegrees.z * deg2rad;

            float cy = MathF.Cos(yaw * 0.5f);
            float sy = MathF.Sin(yaw * 0.5f);
            float cp = MathF.Cos(pitch * 0.5f);
            float sp = MathF.Sin(pitch * 0.5f);
            float cr = MathF.Cos(roll * 0.5f);
            float sr = MathF.Sin(roll * 0.5f);

            Quaternion q = new Quaternion
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