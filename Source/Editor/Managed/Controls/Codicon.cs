using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Xml.Linq;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Documents;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Platform;

namespace Hyperion.Editor.Controls
{
    public class Codicon : Control
    {
        public static readonly StyledProperty<string> KindProperty =
            AvaloniaProperty.Register<Codicon, string>(nameof(Kind), string.Empty);

        public static readonly StyledProperty<double> SizeProperty =
            AvaloniaProperty.Register<Codicon, double>(nameof(Size), 16.0);

        public static readonly StyledProperty<IBrush?> ForegroundProperty =
            TextElement.ForegroundProperty.AddOwner<Codicon>();

        static Codicon()
        {
            AffectsRender<Codicon>(KindProperty, SizeProperty, ForegroundProperty);
            AffectsMeasure<Codicon>(KindProperty, SizeProperty);
        }

        public string Kind
        {
            get => GetValue(KindProperty);
            set => SetValue(KindProperty, value);
        }

        public double Size
        {
            get => GetValue(SizeProperty);
            set => SetValue(SizeProperty, value);
        }

        public IBrush? Foreground
        {
            get => GetValue(ForegroundProperty);
            set => SetValue(ForegroundProperty, value);
        }

        private sealed record IconShape(
            Geometry Geometry,
            double FillOpacity,
            bool HasStroke,
            double StrokeWidth,
            PenLineCap LineCap,
            PenLineJoin LineJoin);

        private static readonly Dictionary<string, IReadOnlyList<IconShape>?> ShapeCache = new();

        // baked by BuildEditorIconsCommandlet; white-on-transparent so they can be tinted by Foreground
        private static readonly Dictionary<string, IBrush?> GeneratedIconCache = new();

        protected override Size MeasureOverride(Size availableSize)
        {
            double width = double.IsNaN(Width) ? Size : Width;
            double height = double.IsNaN(Height) ? Size : Height;
            return new Size(width, height);
        }

        public override void Render(DrawingContext context)
        {
            string kind = Kind;
            if (string.IsNullOrEmpty(kind))
            {
                return;
            }

            IBrush? generatedIcon = ResolveGeneratedIcon(kind);
            if (generatedIcon is not null)
            {
                RenderGeneratedIcon(context, generatedIcon);
                return;
            }

            IReadOnlyList<IconShape>? shapes = ResolveShapes(kind);
            if (shapes is null || shapes.Count == 0)
            {
                return;
            }

            Rect bounds = default;
            bool hasBounds = false;

            foreach (IconShape shape in shapes)
            {
                Rect shapeBounds = shape.Geometry.Bounds;

                if (shapeBounds.Width <= 0 && shapeBounds.Height <= 0)
                {
                    continue;
                }

                bounds = hasBounds ? bounds.Union(shapeBounds) : shapeBounds;
                hasBounds = true;
            }

            if (!hasBounds || bounds.Width <= 0 || bounds.Height <= 0)
            {
                return;
            }

            double targetWidth = double.IsNaN(Width) ? Size : Width;
            double targetHeight = double.IsNaN(Height) ? Size : Height;

            double scale = Math.Min(targetWidth / bounds.Width, targetHeight / bounds.Height);
            if (!double.IsFinite(scale) || scale <= 0)
            {
                return;
            }

            double offsetX = ((Bounds.Width - bounds.Width * scale) * 0.5) - (bounds.X * scale);
            double offsetY = ((Bounds.Height - bounds.Height * scale) * 0.5) - (bounds.Y * scale);

            using (context.PushTransform(Matrix.CreateScale(scale, scale) * Matrix.CreateTranslation(offsetX, offsetY)))
            {
                foreach (IconShape shape in shapes)
                {
                    if (shape.FillOpacity > 0)
                    {
                        if (shape.FillOpacity < 1)
                        {
                            using (context.PushOpacity(shape.FillOpacity))
                            {
                                context.DrawGeometry(Foreground, null, shape.Geometry);
                            }
                        }
                        else
                        {
                            context.DrawGeometry(Foreground, null, shape.Geometry);
                        }
                    }

                    if (shape.HasStroke)
                    {
                        Pen pen = new Pen(Foreground, shape.StrokeWidth, null, shape.LineCap, shape.LineJoin);
                        context.DrawGeometry(null, pen, shape.Geometry);
                    }
                }
            }
        }

        private void RenderGeneratedIcon(DrawingContext context, IBrush iconMask)
        {
            double targetWidth = double.IsNaN(Width) ? Size : Width;
            double targetHeight = double.IsNaN(Height) ? Size : Height;
            double extent = Math.Min(targetWidth, targetHeight);

            Rect iconRect = new Rect(
                (Bounds.Width - extent) * 0.5,
                (Bounds.Height - extent) * 0.5,
                extent,
                extent);

            using (context.PushRenderOptions(new RenderOptions { BitmapInterpolationMode = BitmapInterpolationMode.HighQuality }))
            using (context.PushOpacityMask(iconMask, iconRect))
            {
                context.FillRectangle(Foreground ?? Brushes.White, iconRect);
            }
        }

        private static IBrush? ResolveGeneratedIcon(string kind)
        {
            lock (GeneratedIconCache)
            {
                string fileName = NormalizeKind(kind);
                if (GeneratedIconCache.TryGetValue(fileName, out IBrush? cached))
                {
                    return cached;
                }

                IBrush? iconMask = LoadGeneratedIcon(fileName);
                GeneratedIconCache[fileName] = iconMask;
                return iconMask;
            }
        }

        private static IBrush? LoadGeneratedIcon(string fileName)
        {
            try
            {
                Uri uri = new($"avares://Hyperion.Editor/Assets/Icons/Generated/{fileName}.png");
                if (!AssetLoader.Exists(uri))
                {
                    return null;
                }

                using Stream stream = AssetLoader.Open(uri);
                Bitmap bitmap = new Bitmap(stream);

                return new ImageBrush(bitmap) { Stretch = Stretch.Uniform }.ToImmutable();
            }
            catch
            {
                return null;
            }
        }

        private static IReadOnlyList<IconShape>? ResolveShapes(string kind)
        {
            lock (ShapeCache)
            {
                string fileName = NormalizeKind(kind);
                if (ShapeCache.TryGetValue(fileName, out IReadOnlyList<IconShape>? cached))
                {
                    return cached;
                }

                IReadOnlyList<IconShape>? shapes = LoadShapes(fileName);
                ShapeCache[fileName] = shapes;
                return shapes;
            }
        }

        private static string NormalizeKind(string kind)
        {
            if (kind.Contains('-'))
            {
                return kind.ToLowerInvariant();
            }

            StringBuilder sb = new(kind.Length + 8);
            for (int i = 0; i < kind.Length; i++)
            {
                char c = kind[i];
                if (char.IsUpper(c) && i > 0)
                {
                    sb.Append('-');
                }

                sb.Append(char.ToLowerInvariant(c));
            }

            return sb.ToString();
        }

        private static IReadOnlyList<IconShape>? LoadShapes(string fileName)
        {
            try
            {
                Uri uri = new($"avares://Hyperion.Editor/Assets/Icons/{fileName}.svg");
                if (!AssetLoader.Exists(uri))
                {
                    return null;
                }

                using Stream stream = AssetLoader.Open(uri);
                XDocument document = XDocument.Load(stream);
                if (document.Root is null)
                {
                    return null;
                }

                List<IconShape> shapes = new();

                foreach (XElement element in document.Root.Descendants())
                {
                    string pathData = element.Name.LocalName switch
                    {
                        "path" => element.Attribute("d")?.Value ?? string.Empty,
                        "polygon" => PointsToPathData(element.Attribute("points")?.Value, close: true),
                        "polyline" => PointsToPathData(element.Attribute("points")?.Value, close: false),
                        "line" => LineToPathData(element),
                        "rect" => RectToPathData(element),
                        "circle" => CircleToPathData(element),
                        "ellipse" => EllipseToPathData(element),
                        _ => string.Empty,
                    };

                    if (string.IsNullOrWhiteSpace(pathData))
                    {
                        continue;
                    }

                    string? fillAttr = element.Attribute("fill")?.Value;
                    double fillOpacity = ParseDouble(element.Attribute("fill-opacity")?.Value)
                        ?? ParseDouble(element.Attribute("opacity")?.Value)
                        ?? 1.0;

                    if (fillAttr == "none")
                    {
                        fillOpacity = 0.0;
                    }

                    string? strokeAttr = element.Attribute("stroke")?.Value;
                    bool hasStroke = !string.IsNullOrEmpty(strokeAttr) && strokeAttr != "none";
                    double strokeWidth = ParseDouble(element.Attribute("stroke-width")?.Value) ?? 1.0;
                    PenLineCap lineCap = ParseCap(element.Attribute("stroke-linecap")?.Value);
                    PenLineJoin lineJoin = ParseJoin(element.Attribute("stroke-linejoin")?.Value);

                    shapes.Add(new IconShape(
                        StreamGeometry.Parse(pathData),
                        fillOpacity,
                        hasStroke,
                        strokeWidth,
                        lineCap,
                        lineJoin));
                }

                return shapes;
            }
            catch
            {
                return null;
            }
        }

        private static readonly char[] PointSeparators = { ' ', ',', '\t', '\n', '\r' };

        private static string PointsToPathData(string? points, bool close)
        {
            if (string.IsNullOrWhiteSpace(points))
            {
                return string.Empty;
            }

            string[] tokens = points.Split(PointSeparators, StringSplitOptions.RemoveEmptyEntries);
            if (tokens.Length < 4)
            {
                return string.Empty;
            }

            StringBuilder sb = new();
            for (int i = 0; i + 1 < tokens.Length; i += 2)
            {
                sb.Append(i == 0 ? 'M' : ' ');
                sb.Append(i == 0 ? string.Empty : "L");
                sb.Append(TryFormatNumber(tokens[i]));
                sb.Append(' ');
                sb.Append(TryFormatNumber(tokens[i + 1]));
            }

            if (close)
            {
                sb.Append(" Z");
            }

            return sb.ToString();
        }

        private static string LineToPathData(XElement element)
        {
            string? x1 = element.Attribute("x1")?.Value;
            string? y1 = element.Attribute("y1")?.Value;
            string? x2 = element.Attribute("x2")?.Value;
            string? y2 = element.Attribute("y2")?.Value;

            if (x1 is null || y1 is null || x2 is null || y2 is null)
            {
                return string.Empty;
            }

            return $"M{TryFormatNumber(x1)} {TryFormatNumber(y1)} L{TryFormatNumber(x2)} {TryFormatNumber(y2)}";
        }

        private static string RectToPathData(XElement element)
        {
            double x = ParseDouble(element.Attribute("x")?.Value) ?? 0;
            double y = ParseDouble(element.Attribute("y")?.Value) ?? 0;
            double width = ParseDouble(element.Attribute("width")?.Value) ?? 0;
            double height = ParseDouble(element.Attribute("height")?.Value) ?? 0;

            if (width <= 0 || height <= 0)
            {
                return string.Empty;
            }

            return $"M{FormatNumber(x)} {FormatNumber(y)} L{FormatNumber(x + width)} {FormatNumber(y)}"
                 + $" L{FormatNumber(x + width)} {FormatNumber(y + height)} L{FormatNumber(x)} {FormatNumber(y + height)} Z";
        }

        private static string CircleToPathData(XElement element)
        {
            double cx = ParseDouble(element.Attribute("cx")?.Value) ?? 0;
            double cy = ParseDouble(element.Attribute("cy")?.Value) ?? 0;
            double r = ParseDouble(element.Attribute("r")?.Value) ?? 0;

            if (r <= 0)
            {
                return string.Empty;
            }

            return EllipsePath(cx, cy, r, r);
        }

        private static string EllipseToPathData(XElement element)
        {
            double cx = ParseDouble(element.Attribute("cx")?.Value) ?? 0;
            double cy = ParseDouble(element.Attribute("cy")?.Value) ?? 0;
            double rx = ParseDouble(element.Attribute("rx")?.Value) ?? 0;
            double ry = ParseDouble(element.Attribute("ry")?.Value) ?? 0;

            if (rx <= 0 || ry <= 0)
            {
                return string.Empty;
            }

            return EllipsePath(cx, cy, rx, ry);
        }

        private static string EllipsePath(double cx, double cy, double rx, double ry)
        {
            return $"M{FormatNumber(cx - rx)} {FormatNumber(cy)} "
                 + $"A{FormatNumber(rx)} {FormatNumber(ry)} 0 1 0 {FormatNumber(cx + rx)} {FormatNumber(cy)} "
                 + $"A{FormatNumber(rx)} {FormatNumber(ry)} 0 1 0 {FormatNumber(cx - rx)} {FormatNumber(cy)} Z";
        }

        private static string FormatNumber(double value)
        {
            return value.ToString("R", CultureInfo.InvariantCulture);
        }

        private static string TryFormatNumber(string token)
        {
            return double.TryParse(token, NumberStyles.Float, CultureInfo.InvariantCulture, out double value)
                ? value.ToString("R", CultureInfo.InvariantCulture)
                : "0";
        }

        private static double? ParseDouble(string? value)
        {
            if (value is null)
            {
                return null;
            }

            return double.TryParse(value, NumberStyles.Float, CultureInfo.InvariantCulture, out double result)
                ? result
                : null;
        }

        private static PenLineCap ParseCap(string? value) => value switch
        {
            "round" => PenLineCap.Round,
            "square" => PenLineCap.Square,
            _ => PenLineCap.Flat,
        };

        private static PenLineJoin ParseJoin(string? value) => value switch
        {
            "round" => PenLineJoin.Round,
            "bevel" => PenLineJoin.Bevel,
            _ => PenLineJoin.Miter,
        };
    }
}
