using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [ClassBinding(Name = "OverlayBase")]
    public abstract class OverlayBase : ObjectBase
    {
        public OverlayBase()
        {
        }
    }

    [ClassBinding(Name = "TextureOverlay")]
    public class TextureOverlay : OverlayBase
    {
        public TextureOverlay()
        {
        }
    }

    [ClassBinding(Name = "TextOverlay")]
    public class TextOverlay : OverlayBase
    {
        public TextOverlay()
        {
        }
    }
}