using System;

namespace Hyperion
{
    [ClassBinding(Name = "EditorDecalPainterState")]
    public class EditorDecalPainterState : ObjectBase
    {
        public bool IsEnabled => this.IsEnabled();

        public Decal? ActiveDecal => this.GetActiveDecal();

        public float Scale => this.GetScale();

        public float RotationDegrees => this.GetRotationDegrees();

        public bool RandomRotation => this.GetRandomRotation();

        public float Spacing => this.GetSpacing();

        public float EraseRadius => this.GetEraseRadius();

        public bool AlignToSurface => this.GetAlignToSurface();
    }
}
