using System;
using System.Globalization;
using System.Runtime.InteropServices;
using Avalonia.Media;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class ColorViewModel : InspectorPropertyViewModelBase
    {
        private string _r = string.Empty;
        private string _g = string.Empty;
        private string _b = string.Empty;
        private string _a = string.Empty;

        private IBrush _swatchBrush = Brushes.Transparent;

        public ColorViewModel(ObjectBase target, Property property, bool isReadOnly)
            : base(target, property, isReadOnly)
        {
        }

        public ColorViewModel(IntPtr classAddress, Func<IntPtr> targetAddressResolver, Property property, bool isReadOnly)
            : base(classAddress, targetAddressResolver, property, isReadOnly)
        {
        }

        public ColorViewModel(string label, TypeInfo typeInfo, Func<BoxedValue> getter, Action<BoxedValue> setter, bool isReadOnly)
            : base(label, typeInfo, getter, setter, isReadOnly)
        {
        }

        public string R
        {
            get => _r;
            set
            {
                if (IsApplyingModelValue)
                {
                    return;
                }

                _r = value;
            }
        }

        public string G
        {
            get => _g;
            set
            {
                if (IsApplyingModelValue)
                {
                    return;
                }

                _g = value;
            }
        }

        public string B
        {
            get => _b;
            set
            {
                if (IsApplyingModelValue)
                {
                    return;
                }

                _b = value;
            }
        }

        public string A
        {
            get => _a;
            set
            {
                if (IsApplyingModelValue)
                {
                    return;
                }

                _a = value;
            }
        }

        public IBrush SwatchBrush
        {
            get => _swatchBrush;
            private set => SetProperty(ref _swatchBrush, value);
        }

        public override void RefreshValue()
        {
            if (!BeginRefresh())
            {
                return;
            }

            _ = EngineManager.PostToSimThread(() =>
            {
                Color color;

                try
                {
                    color = ReadColorFromProperty();
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to read color for '{Label}': {ex.Message}");

                    EndRefresh();

                    return;
                }

                string r = FormatComponent(color.Red);
                string g = FormatComponent(color.Green);
                string b = FormatComponent(color.Blue);
                string a = FormatComponent(color.Alpha);

                Dispatcher.UIThread.Post(() =>
                {
                    try
                    {
                        ApplyModelValue(() =>
                        {
                            Value = $"({r}, {g}, {b}, {a})";

                            // Leave the text boxes alone while the user is typing in them.
                            if (!IsEditing)
                            {
                                _r = r;
                                _g = g;
                                _b = b;
                                _a = a;

                                OnPropertyChanged(nameof(R));
                                OnPropertyChanged(nameof(G));
                                OnPropertyChanged(nameof(B));
                                OnPropertyChanged(nameof(A));
                            }

                            // Avalonia UI objects (Brush/Color) must be created on the UI thread.
                            SwatchBrush = ToBrush(color);
                        });
                    }
                    finally
                    {
                        EndRefresh();
                    }
                });
            });
        }

        public override void CommitValue()
        {
            if (_isReadOnly || !IsTargetValid)
            {
                return;
            }

            string capturedR = _r;
            string capturedG = _g;
            string capturedB = _b;
            string capturedA = _a;

            _ = EngineManager.PostToSimThread(() =>
            {
                try
                {
                    // Refresh any cached copy of the containing value before reading, so the
                    // components we don't touch are carried over from the current value.
                    PreWriteCallback?.Invoke();

                    Color current = ReadColorFromProperty();

                    float r = TryParseComponent(capturedR, out float parsedR) ? parsedR : current.Red;
                    float g = TryParseComponent(capturedG, out float parsedG) ? parsedG : current.Green;
                    float b = TryParseComponent(capturedB, out float parsedB) ? parsedB : current.Blue;
                    float a = TryParseComponent(capturedA, out float parsedA) ? parsedA : current.Alpha;

                    using BoxedValue boxed = new BoxedValue(new Color(r, g, b, a));
                    CommitPropertyChange($"Set {Label}", boxed);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Error, $"Inspector failed to write color property '{Label}': {ex.Message}");

                    Dispatcher.UIThread.Post(RefreshValue);
                }
            });
        }

        private static IBrush ToBrush(Color color)
        {
            byte a = (byte)Math.Clamp(color.Alpha * 255.0f, 0.0f, 255.0f);
            byte r = (byte)Math.Clamp(color.Red * 255.0f, 0.0f, 255.0f);
            byte g = (byte)Math.Clamp(color.Green * 255.0f, 0.0f, 255.0f);
            byte b = (byte)Math.Clamp(color.Blue * 255.0f, 0.0f, 255.0f);

            return new SolidColorBrush(new Avalonia.Media.Color(a, r, g, b));
        }

        private static string FormatComponent(float value)
        {
            return value.ToString("F3", CultureInfo.InvariantCulture);
        }

        private static bool TryParseComponent(string text, out float result)
        {
            return float.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out result);
        }

        // Sim thread only.
        private Color ReadColorFromProperty()
        {
            using BoxedValue boxed = GetPropertyValue();

            IntPtr ptr = boxed.Pointer;

            if (ptr == IntPtr.Zero)
            {
                throw new InvalidOperationException($"Property '{Label}' returned null pointer");
            }

            return Marshal.PtrToStructure<Color>(ptr);
        }
    }
}
