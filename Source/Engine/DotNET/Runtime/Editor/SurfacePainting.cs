using System;

namespace Hyperion
{
    [ClassBinding(Name = "EditorSurfacePainterState")]
    public abstract class EditorSurfacePainterState : ObjectBase
    {
        public bool IsEnabled => this.IsEnabled();

        public float Scale => this.GetScale();

        public float RotationDegrees => this.GetRotationDegrees();

        public bool RandomRotation => this.GetRandomRotation();

        public float Spacing => this.GetSpacing();

        public float EraseRadius => this.GetEraseRadius();

        public bool AlignToSurface => this.GetAlignToSurface();
    }

    [ClassBinding(Name = "EditorDecalPainterState")]
    public class EditorDecalPainterState : EditorSurfacePainterState
    {
        public Decal? ActiveDecal => this.GetActiveDecal();
    }

    [ClassBinding(Name = "EditorInstancePainterState")]
    public class EditorInstancePainterState : EditorSurfacePainterState
    {
        public Prefab? ActivePrefab => this.GetActivePrefab();

        public float FootprintRadius => this.GetFootprintRadius();

        public float SinkDepth => this.GetSinkDepth();
    }
}
