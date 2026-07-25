using System.Collections.Generic;
using System.Linq;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public static class InspectorActionsHelper
    {
        public static List<InspectorActionViewModel> GetActions(ObjectBase? target)
        {
            List<InspectorActionViewModel> result = new List<InspectorActionViewModel>();

            if (target == null || !target.IsValid)
            {
                return result;
            }

            Class targetClass = target.Class;

            List<Method> actions = targetClass.Methods
                .Where(m => m.IsMemberFunction)
                .Where(m => m.GetAttribute("editoraction") != null)
                .Where(m => EvaluateEditCondition(target, targetClass, m.GetAttribute("editcondition"), m.Name.ToString()))
                .OrderBy(m =>
                {
                    ClassAttribute? attrEditOrder = m.GetAttribute("editororder");

                    return attrEditOrder != null ? attrEditOrder.Value.GetInt() : int.MaxValue;
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

                    result.Add(new InspectorActionViewModel(target, method, label, isEnabled));
                }
                catch (System.Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Failed to create view model for action '{method.Name}': {ex.Message}");
                }
            }

            return result;
        }

        private static bool EvaluateEditCondition(ObjectBase target, Class targetClass, ClassAttribute? attrEditCondition, string memberName)
        {
            if (!target.IsValid)
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
                Method? conditionMethod = targetClass.GetMethod(methodName);

                if (conditionMethod != null)
                {
                    using BoxedValue resultData = conditionMethod.Value.Invoke(target);
                    object? result = resultData.GetValue();

                    if (result is bool boolResult)
                    {
                        return boolResult;
                    }

                    Logger.Log(LogLevel.Warning, $"editcondition method '{methodName}' on member '{memberName}' did not return a bool");
                }
            }
            else if (attrEditCondition.Value.IsBool)
            {
                return attrEditCondition.Value.GetBool();
            }
            else
            {
                Logger.Log(LogLevel.Warning, $"editcondition attribute on member '{memberName}' is not a valid type");
            }

            return true; // continue if no condition or invalid condition
        }
    }
}
