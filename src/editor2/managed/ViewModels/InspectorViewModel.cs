using System;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using Hyperion;

namespace Hyperion.Editor.ViewModels
{
    public class InspectorViewModel : ViewModelBase
    {
        public ObservableCollection<InspectorPropertyViewModelBase> Properties { get; } = new ObservableCollection<InspectorPropertyViewModelBase>();
        public ObservableCollection<InspectorActionViewModel> Actions { get; } = new ObservableCollection<InspectorActionViewModel>();

        private bool _hasActions;
        public bool HasActions
        {
            get => _hasActions;
            private set => SetProperty(ref _hasActions, value);
        }

        private Node? _selectedNode;
        public Node? SelectedNode
        {
            get => _selectedNode;
            private set => SetProperty(ref _selectedNode, value);
        }

        public void SetSelectedNode(Node? node)
        {
            SelectedNode = node;
            RefreshProperties();
        }

        private void RefreshProperties()
        {
            Properties.Clear();
            Actions.Clear();
            HasActions = false;

            if (SelectedNode == null || !SelectedNode.IsValid)
            {
                return;
            }

            Class nodeClass = SelectedNode.Class;

            Logger.Log(LogType.Debug, $"Inspector refreshing properties for node '{SelectedNode.Name}' of class '{nodeClass.Name}'");

            // sort by editorder attribute (if present), then by name
            List<Property> properties = nodeClass.Properties
                .Where(p =>
                {
                    ClassAttribute? attrEditCondition = p.GetAttribute("editcondition");

                    return EvaluateEditCondition(nodeClass, attrEditCondition, p.Name.ToString());
                })
                .OrderBy(p =>
                {
                    ClassAttribute? attrEditOrder = p.GetAttribute("editorder");

                    if (attrEditOrder != null)
                    {
                        return attrEditOrder.Value.GetInt();
                    }

                    return int.MaxValue;
                })
                .ThenBy(p => p.Name.ToString())
                .ToList();

            foreach (Property property in properties)
            {
                try
                {
                    // skip non-editor properties
                    ClassAttribute? attrEditHide = property.GetAttribute("edithide");

                    if (attrEditHide != null && attrEditHide.Value.GetBool() == true)
                    {
                        continue;
                    }

                    bool isReadOnly = false;
                    ClassAttribute? attrEditEnabled = property.GetAttribute("editenabled");

                    if (attrEditEnabled != null && attrEditEnabled.Value.GetBool() == false)
                    {
                        isReadOnly = true;
                    }
                    Properties.Add(InspectorViewModelFactory.Create(SelectedNode, property, isReadOnly));
                }
                catch (Exception ex)
                {
                    Logger.Log(LogType.Warn, $"Inspector failed to create view model for property '{property.Name}': {ex.Message}");
                }
            }

            // collect actions (methods with editaction attribute)
            List<Method> actions = nodeClass.Methods
                .Where(m => m.IsMemberFunction)
                .Where(m => m.GetAttribute("editaction") != null)
                .Where(m => EvaluateEditCondition(nodeClass, m.GetAttribute("editcondition"), m.Name.ToString()))
                .OrderBy(m =>
                {
                    ClassAttribute? attrEditOrder = m.GetAttribute("editorder");

                    if (attrEditOrder != null)
                    {
                        return attrEditOrder.Value.GetInt();
                    }

                    return int.MaxValue;
                })
                .ThenBy(m => m.Name.ToString())
                .ToList();

            Logger.Log(LogType.Debug, $"Inspector found {actions.Count} actions for node '{SelectedNode.Name}'");

            foreach (Method method in actions)
            {
                try
                {
                    ClassAttribute? attrEditHide = method.GetAttribute("edithide");

                    if (attrEditHide != null && attrEditHide.Value.IsBool && attrEditHide.Value.GetBool())
                    {
                        continue;
                    }

                    string label = method.Name.ToString();
                    ClassAttribute? attrEditAction = method.GetAttribute("editaction");

                    if (attrEditAction != null && attrEditAction.Value.IsString)
                    {
                        label = attrEditAction.Value.GetString();
                    }

                    bool isEnabled = true;
                    ClassAttribute? attrEditEnabled = method.GetAttribute("editenabled");

                    if (attrEditEnabled != null && attrEditEnabled.Value.IsBool && attrEditEnabled.Value.GetBool() == false)
                    {
                        isEnabled = false;
                    }

                    Actions.Add(new InspectorActionViewModel(SelectedNode, method, label, isEnabled));
                }
                catch (Exception ex)
                {
                    Logger.Log(LogType.Warn, $"Inspector failed to create view model for action '{method.Name}': {ex.Message}");
                }
            }

            HasActions = Actions.Count > 0;
        }

        private bool EvaluateEditCondition(Class nodeClass, ClassAttribute? attrEditCondition, string memberName)
        {
            if (SelectedNode == null || !SelectedNode.IsValid)
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
                Method? conditionMethod = nodeClass.GetMethod(methodName);

                if (conditionMethod != null)
                {
                    using HypData resultData = conditionMethod.Value.Invoke(SelectedNode);
                    object? result = resultData.GetValue();

                    if (result is bool boolResult)
                    {
                        return boolResult;
                    }

                    Logger.Log(LogType.Warn, $"Inspector editcondition method '{methodName}' on member '{memberName}' did not return a bool");
                }
            }
            else if (attrEditCondition.Value.IsBool)
            {
                return attrEditCondition.Value.GetBool();
            }
            else
            {
                Logger.Log(LogType.Warn, $"Inspector editcondition attribute on member '{memberName}' is not a valid type");
            }

            return true; // continue if no condition or invalid condition
        }
    }
}
