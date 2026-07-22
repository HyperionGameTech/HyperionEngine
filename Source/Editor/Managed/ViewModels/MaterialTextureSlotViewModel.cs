using System;
using System.Collections.Generic;

namespace Hyperion.Editor.ViewModels
{
    public class MaterialTextureSlotViewModel : ViewModelBase
    {
        public InspectorPropertyViewModelBase TextureEditor { get; }
        public InspectorPropertyViewModelBase? InlineControl { get; }

        public bool HasInlineControl => InlineControl != null;

        public IEnumerable<InspectorPropertyViewModelBase> TextureEditorItems { get; }
        public IEnumerable<InspectorPropertyViewModelBase> InlineControlItems { get; }

        public MaterialTextureSlotViewModel(InspectorPropertyViewModelBase textureEditor, InspectorPropertyViewModelBase? inlineControl)
        {
            TextureEditor = textureEditor ?? throw new ArgumentNullException(nameof(textureEditor));
            InlineControl = inlineControl;

            TextureEditorItems = new[] { TextureEditor };
            InlineControlItems = InlineControl != null
                ? new[] { InlineControl }
                : Array.Empty<InspectorPropertyViewModelBase>();
        }

        public void RefreshValue()
        {
            TextureEditor.RefreshValue();
            InlineControl?.RefreshValue();
        }
    }
}
