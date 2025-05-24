using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [HypClassBinding(Name="LightComponent")]
    [StructLayout(LayoutKind.Explicit, Size = 8)]
    public struct LightComponent : IComponent
    {
        [FieldOffset(0)]
        private Handle<Light> lightHandle;

        public void Dispose()
        {
            lightHandle.Dispose();
        }

        public Light? Light
        {
            get
            {
                return lightHandle.GetValue();
            }
            set
            {
                lightHandle.Dispose();

                if (value == null)
                {
                    lightHandle = Handle<Light>.Empty;
                    
                    return;
                }

                lightHandle = new Handle<Light>(value);
            }
        }
    }

    [HypClassBinding(IsDynamic = true)]
    public class LightComponentEditorPropertyPanel : EditorPropertyPanelBase
    {
        public LightComponentEditorPropertyPanel()
        {
        }

        ~LightComponentEditorPropertyPanel()
        {
            Logger.Log(LogType.Debug, "LightComponentEditorPropertyPanel destructor called");
        }

        public override void Build(object target)
        {
            UIObject? uiObject = null;
            
            try
            {
                using (Asset<UIObject> asset = AssetManager.Instance.Load<UIObject>("ui/property_panel/LightComponent.PropertyPanel.ui.xml"))
                {
                    if (!asset.IsValid)
                        throw new Exception("Failed to load UIObject! Asset result is not valid");

                    uiObject = asset.Value;

                    if (uiObject == null)
                        throw new Exception("Failed to load UIObject! Result was null");
                }
            }
            catch (Exception e)
            {
                Logger.Log(LogType.Error, "Failed to load LightComponent.PropertyPanel.ui.xml: " + e.Message);

                return;
            }

            this.AddChildUIObject(uiObject);
        }
    }
}