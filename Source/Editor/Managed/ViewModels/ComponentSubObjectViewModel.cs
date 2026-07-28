using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class ComponentSubObjectViewModel : ViewModelBase
    {
        public string Label { get; }
        public ObjectBase Target { get; }
        public ObservableCollection<InspectorPropertyViewModelBase> Properties { get; } = new();
        public ObservableCollection<InspectorActionViewModel> Actions { get; } = new();

        private readonly Action? _preWriteCallback;
        private readonly Action? _postWriteCallback;
        private readonly Action? _valueChangedCallback;

        private bool _hasProperties;
        public bool HasProperties
        {
            get => _hasProperties;
            private set => SetProperty(ref _hasProperties, value);
        }

        private bool _hasActions;
        public bool HasActions
        {
            get => _hasActions;
            private set => SetProperty(ref _hasActions, value);
        }

        public ComponentSubObjectViewModel(
            string label,
            ObjectBase target,
            int depth = 0,
            Action? preWriteCallback = null,
            Action? postWriteCallback = null,
            Action? valueChangedCallback = null)
        {
            Label = label;
            Target = target ?? throw new ArgumentNullException(nameof(target));
            _preWriteCallback = preWriteCallback;
            _postWriteCallback = postWriteCallback;
            _valueChangedCallback = valueChangedCallback;

            PopulateProperties(depth);
            PopulateActions();
        }

        /// <summary>
        /// Re-reads every property of this object. A setter can change more than the value it was
        /// handed (clamping, packed flags, base-material fallbacks), so after any write the whole
        /// object has to be read back rather than trusting the value the UI just sent.
        /// </summary>
        public void RefreshProperties()
        {
            Dispatcher.UIThread.VerifyAccess();

            if (!Target.IsValid)
            {
                return;
            }

            foreach (InspectorPropertyViewModelBase vm in Properties)
            {
                vm.RefreshValue();
            }
        }

        private void PopulateProperties(int depth)
        {
            if (!Target.IsValid)
            {
                return;
            }

            Class cls = Target.Class;

            List<Property> properties = cls.Properties
                .Where(p =>
                {
                    ClassAttribute? attrEditor = p.GetAttribute("editor");

                    if (attrEditor != null && attrEditor.Value.GetBool() == false)
                    {
                        return false;
                    }

                    return true;
                })
                .OrderBy(p =>
                {
                    ClassAttribute? attrEditOrder = p.GetAttribute("editororder");

                    if (attrEditOrder != null)
                    {
                        return attrEditOrder.Value.GetInt();
                    }

                    return int.MaxValue;
                })
                .ThenBy(p => p.Name.ToString())
                .ToList();

            // Any write anywhere below this object refreshes the whole object, and also bubbles up
            // to whatever contains it.
            Action onValueChanged = () =>
            {
                RefreshProperties();
                _valueChangedCallback?.Invoke();
            };

            // Not every reflected setter marks its asset dirty (plain HYP_FIELD properties write
            // straight through), so record the edit here rather than relying on each class to.
            Action onPostWrite = () =>
            {
                _postWriteCallback?.Invoke();
                MarkTargetDirty();
            };

            foreach (Property property in properties)
            {
                try
                {
                    bool isReadOnly = false;
                    ClassAttribute? attrEditEnabled = property.GetAttribute("editenabled");

                    if (attrEditEnabled != null && attrEditEnabled.Value.GetBool() == false)
                    {
                        isReadOnly = true;
                    }

                    InspectorPropertyViewModelBase vm = InspectorViewModelFactory.Create(
                        Target, property, isReadOnly, depth, _preWriteCallback, onPostWrite, onValueChanged);

                    Properties.Add(vm);
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to create view model for sub-object property '{property.Name}': {ex.Message}");
                }
            }

            HasProperties = Properties.Count > 0;
        }

        // Same idea as Inspector actions.
        private void PopulateActions()
        {
            if (!Target.IsValid)
            {
                return;
            }

            Class cls = Target.Class;

            List<Method> actions = cls.Methods
                .Where(m => m.IsMemberFunction)
                .Where(m => m.GetAttribute("editoraction") != null)
                .Where(m => EvaluateEditCondition(cls, m.GetAttribute("editcondition"), m.Name.ToString()))
                .OrderBy(m =>
                {
                    ClassAttribute? attrEditOrder = m.GetAttribute("editororder");

                    if (attrEditOrder != null)
                    {
                        return attrEditOrder.Value.GetInt();
                    }

                    return int.MaxValue;
                })
                .ThenBy(m => m.Name.ToString())
                .ToList();

            foreach (Method method in actions)
            {
                try
                {
                    ClassAttribute? attrEditor = method.GetAttribute("editor");

                    if (attrEditor != null && attrEditor.Value.GetBool() == false)
                    {
                        continue;
                    }

                    string label = method.Name.ToString();
                    ClassAttribute? attrEditorAction = method.GetAttribute("editoraction");

                    if (attrEditorAction != null && attrEditorAction.Value.IsString)
                    {
                        label = attrEditorAction.Value.GetString();
                    }

                    bool isEnabled = true;
                    ClassAttribute? attrEditEnabled = method.GetAttribute("editenabled");

                    if (attrEditEnabled != null && attrEditEnabled.Value.IsBool && attrEditEnabled.Value.GetBool() == false)
                    {
                        isEnabled = false;
                    }

                    // An editor action mutates the object, so the panel has to re-read afterwards.
                    Actions.Add(new InspectorActionViewModel(Target, method, label, isEnabled, OnActionCompleted));
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to create view model for sub-object action '{method.Name}': {ex.Message}");
                }
            }

            HasActions = Actions.Count > 0;
        }

        private void OnActionCompleted()
        {
            RefreshProperties();
            _valueChangedCallback?.Invoke();
        }

        // Sim thread. Records that the asset has unsaved changes.
        private void MarkTargetDirty()
        {
            if (Target is not AssetObject assetObject || !assetObject.IsValid)
            {
                return;
            }

            try
            {
                assetObject.MarkDirty();
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Warning, $"Failed to mark asset '{Label}' dirty: {ex.Message}");
            }
        }

        private bool EvaluateEditCondition(Class cls, ClassAttribute? attrEditCondition, string memberName)
        {
            if (!Target.IsValid)
            {
                return false;
            }

            if (attrEditCondition == null)
            {
                return true;
            }

            if (attrEditCondition.Value.IsString)
            {
                string methodName = attrEditCondition.Value.GetString();
                Method? conditionMethod = cls.GetMethod(methodName);

                if (conditionMethod != null)
                {
                    using BoxedValue resultData = conditionMethod.Value.Invoke(Target);
                    object? result = resultData.GetValue();

                    if (result is bool boolResult)
                    {
                        return boolResult;
                    }

                    Logger.Log(LogLevel.Warning, $"Sub-object editcondition method '{methodName}' on member '{memberName}' did not return a bool");
                }
            }
            else if (attrEditCondition.Value.IsBool)
            {
                return attrEditCondition.Value.GetBool();
            }
            else
            {
                Logger.Log(LogLevel.Warning, $"Sub-object editcondition attribute on member '{memberName}' is not a valid type");
            }

            return true; // continue if no condition or invalid condition
        }
    }
}
