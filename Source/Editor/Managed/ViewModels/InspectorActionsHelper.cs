using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public static class InspectorActionsHelper
    {
        public static Task<List<InspectorActionViewModel>> GetActionsAsync(ObjectBase? target, System.Action? onCompleted = null)
        {
            if (target == null || !target.IsValid)
            {
                return Task.FromResult(new List<InspectorActionViewModel>());
            }

            return EngineManager.PostToSimThread(() => GetActionsOnSimThread(target, onCompleted));
        }

        public static Task<List<InspectorActionViewModel>> GetActionsAsync(ObjectBase? target, Class instanceClass, System.Action? onCompleted = null)
        {
            if (target == null || !target.IsValid || !instanceClass.IsValid)
            {
                return Task.FromResult(new List<InspectorActionViewModel>());
            }

            return EngineManager.PostToSimThread(() => GetActionsOnSimThread(target, instanceClass, onCompleted));
        }

        public static List<InspectorActionViewModel> GetActionsOnSimThread(ObjectBase? target, System.Action? onCompleted = null)
        {
            if (target == null || !target.IsValid)
            {
                return new List<InspectorActionViewModel>();
            }

            return GetActionsCore(target, new List<Class> { target.Class }, onCompleted);
        }

        public static List<InspectorActionViewModel> GetActionsOnSimThread(ObjectBase? target, Class instanceClass, System.Action? onCompleted = null)
        {
            if (target == null || !target.IsValid || !instanceClass.IsValid)
            {
                return new List<InspectorActionViewModel>();
            }

            var chain = new List<Class>();

            for (Class? current = instanceClass; current.HasValue; current = current.Value.GetParent())
            {
                chain.Add(current.Value);
            }

            chain.Reverse();

            return GetActionsCore(target, chain, onCompleted);
        }

        private static List<InspectorActionViewModel> GetActionsCore(ObjectBase target, List<Class> classes, System.Action? onCompleted)
        {
            var result = new List<InspectorActionViewModel>();

            var methods = new List<Method>();
            var indexByName = new Dictionary<string, int>();

            foreach (Class targetClass in classes)
            {
                foreach (Method method in targetClass.Methods.Where(m => m.IsMemberFunction))
                {
                    string name = method.Name.ToString();

                    if (indexByName.TryGetValue(name, out int index))
                    {
                        methods[index] = method;
                    }
                    else
                    {
                        indexByName[name] = methods.Count;
                        methods.Add(method);
                    }
                }
            }

            var actions = methods
                .Where(m => m.GetAttribute("editoraction") != null)
                .Where(m => EvaluateEditCondition(target, classes, m.GetAttribute("editcondition"), m.Name.ToString()))
                .OrderBy(m =>
                {
                    ClassAttribute? attrEditOrder = m.GetAttribute("editororder");

                    return attrEditOrder != null ? attrEditOrder.Value.GetInt() : int.MaxValue;
                });

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

                    result.Add(new InspectorActionViewModel(target, method, label, isEnabled, onCompleted));
                }
                catch (System.Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Failed to create view model for action '{method.Name}': {ex.Message}");
                }
            }

            return result;
        }

        private static bool EvaluateEditCondition(ObjectBase target, List<Class> classes, ClassAttribute? attrEditCondition, string memberName)
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

                Method? conditionMethod = null;

                foreach (Class targetClass in classes)
                {
                    conditionMethod = targetClass.GetMethod(methodName);

                    if (conditionMethod != null)
                    {
                        break;
                    }
                }

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
