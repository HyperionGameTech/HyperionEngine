using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Reflection;
using System.Threading.Tasks;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;
using Hyperion.Editor.Commands;

namespace Hyperion.Editor.ViewModels
{
    public class InspectorViewModel : ViewModelBase
    {
        public ObservableCollection<InspectorPropertyViewModelBase> Properties { get; } = new ObservableCollection<InspectorPropertyViewModelBase>();
        public ObservableCollection<InspectorActionViewModel> Actions { get; } = new ObservableCollection<InspectorActionViewModel>();
        public ObservableCollection<InspectorComponentViewModelBase> Components { get; } = new ObservableCollection<InspectorComponentViewModelBase>();
        public ObservableCollection<AddComponentOptionViewModel> AddableComponents { get; } = new ObservableCollection<AddComponentOptionViewModel>();

        public ICommand AddComponentCommand { get; }

        private bool _hasActions;
        public bool HasActions
        {
            get => _hasActions;
            private set => SetProperty(ref _hasActions, value);
        }

        private bool _hasComponents;
        public bool HasComponents
        {
            get => _hasComponents;
            private set => SetProperty(ref _hasComponents, value);
        }

        private bool _hasAddableComponents;
        public bool HasAddableComponents
        {
            get => _hasAddableComponents;
            private set => SetProperty(ref _hasAddableComponents, value);
        }

        private bool _isEntity;
        public bool IsEntity
        {
            get => _isEntity;
            private set => SetProperty(ref _isEntity, value);
        }

        private bool _isRootNode;
        public bool IsRootNode
        {
            get => _isRootNode;
            private set => SetProperty(ref _isRootNode, value);
        }

        private bool _hasSceneProperties;
        public bool HasSceneProperties
        {
            get => _hasSceneProperties;
            private set => SetProperty(ref _hasSceneProperties, value);
        }

        public ObservableCollection<InspectorPropertyViewModelBase> SceneProperties { get; } = new ObservableCollection<InspectorPropertyViewModelBase>();

        private AttachedScriptViewModel? _attachedScript;
        public AttachedScriptViewModel? AttachedScript
        {
            get => _attachedScript;
            private set => SetProperty(ref _attachedScript, value);
        }

        private bool _hasAttachedScript;
        public bool HasAttachedScript
        {
            get => _hasAttachedScript;
            private set => SetProperty(ref _hasAttachedScript, value);
        }

        private Node? _selectedNode;
        public Node? SelectedNode
        {
            get => _selectedNode;
            private set => SetProperty(ref _selectedNode, value);
        }

        private DelegateHandler? _transformUpdatedHandler;

        private Scene? _currentScene;
        public Scene? CurrentScene
        {
            get => _currentScene;
            private set => SetProperty(ref _currentScene, value);
        }

        public InspectorViewModel()
        {
            AddComponentCommand = new AsyncRelayCommand(AddComponentAsync, CanAddComponent);
        }

        ~InspectorViewModel()
        {
            _transformUpdatedHandler?.Remove();
        }

        public void SetSelectedNode(Node? node, Scene? scene = null, bool isRootNode = false)
        {
            Dispatcher.UIThread.VerifyAccess();

            // Unbind from previous node's TransformUpdated delegate
            _transformUpdatedHandler?.Remove();
            _transformUpdatedHandler = null;

            SelectedNode = node;
            CurrentScene = scene;
            IsRootNode = isRootNode;

            // Bind to the new node's TransformUpdated delegate
            if (SelectedNode != null)
            {
                _transformUpdatedHandler = SelectedNode.GetTransformUpdatedDelegate().Bind((Node updatedNode) =>
                {
                    Dispatcher.UIThread.Post(() =>
                    {
                        RefreshTransformProperties();
                    });
                });
            }

            RefreshProperties();
        }

        private void RefreshProperties()
        {
            Dispatcher.UIThread.VerifyAccess();

            Properties.Clear();
            Actions.Clear();
            Components.Clear();
            AddableComponents.Clear();
            SceneProperties.Clear();

            AttachedScript = null;
            HasAttachedScript = false;

            HasActions = false;
            HasComponents = false;
            HasAddableComponents = false;
            HasSceneProperties = false;

            if (SelectedNode == null || !SelectedNode.IsValid)
            {
                return;
            }

            // If this is the root node, show scene properties
            if (IsRootNode && CurrentScene != null && CurrentScene.IsValid)
            {
                RefreshSceneProperties();
            }

            Class nodeClass = SelectedNode.Class;

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

            bool hasAddedMobility = false;

            var addMobility = () =>
            {
                try
                {
                    var mobilityVm = new MobilityPropertyViewModel(SelectedNode, Class.GetClass<Node>().GetProperty("NodeFlags") ?? throw new Exception("Failed to get NodeFlags property"));
                    mobilityVm.RefreshValue();
                    
                    Properties.Add(mobilityVm);

                    hasAddedMobility = true;
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to create mobility selector: {ex.Message}");
                }
            };

            foreach (Property property in properties)
            {
                try
                {
                    if (property.Name == "Components")
                    {
                        continue; // skip Components property -- now handled separately
                    }

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

                    if (Properties[Properties.Count - 1] is FlagsPropertyViewModel flagsVm)
                    {
                        flagsVm.ValueCommitted += RefreshActions;

                        // Insert Mobility selector right after the Flags property
                        if (!hasAddedMobility)
                        {
                            addMobility();
                        }
                    }
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to create view model for property '{property.Name}': {ex.Message}");
                }
            }

            if (!hasAddedMobility)
            {
                addMobility();
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

            Logger.Log(LogLevel.Debug, $"Inspector found {actions.Count} actions for node '{SelectedNode.Name}'");

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
                    Logger.Log(LogLevel.Warning, $"Inspector failed to create view model for action '{method.Name}': {ex.Message}");
                }
            }

            HasActions = Actions.Count > 0;

            // collect components
            if (SelectedNode is Entity entity)
            {
                IsEntity = true;

                AttachedScript = new AttachedScriptViewModel(entity);
                HasAttachedScript = true;

                _ = EngineManager.PostToSimThread(() =>
                {
                    EntityManager? mgr = entity.EntityManager;
                    if (mgr == null)
                    {
                        Logger.Log(LogLevel.Warning, $"Inspector failed to get EntityManager for entity '{entity.Name}'");

                        return;
                    }

                    List<TypeId> componentTypeIds = mgr.GetComponentTypeIds(entity).ToList();

                    Dispatcher.UIThread.Post(() =>
                    {
                        Components.Clear();

                        foreach (TypeId typeId in componentTypeIds)
                        {
                            InspectorComponentViewModelBase? componentVm = null;

                            ComponentTypeDescriptor? descriptor = s_registeredComponents.Value
                                .FirstOrDefault(d => d.TypeId == typeId);

                            if (descriptor != null)
                                componentVm = descriptor.CreateViewModel(entity);
                            else
                                Logger.Log(LogLevel.Debug, $"Inspector has no view model for component type '{typeId}'");

                            if (componentVm != null && componentVm.IsEditorVisible)
                            {
                                Components.Add(componentVm);
                                componentVm.PopulateProperties();
                            }
                        }

                        HasComponents = Components.Count > 0;

                        UpdateAddableComponents(componentTypeIds);
                    });
                });
            }
            else
            {
                IsEntity = false;
                AttachedScript = null;
                HasAttachedScript = false;
                AddableComponents.Clear();
                HasAddableComponents = false;
            }
        }

        private void RefreshActions()
        {
            Dispatcher.UIThread.VerifyAccess();

            if (SelectedNode == null || !SelectedNode.IsValid)
                return;

            Actions.Clear();

            Class nodeClass = SelectedNode.Class;

            List<Method> actions = nodeClass.Methods
                .Where(m => m.IsMemberFunction)
                .Where(m => m.GetAttribute("editaction") != null)
                .Where(m => EvaluateEditCondition(nodeClass, m.GetAttribute("editcondition"), m.Name.ToString()))
                .OrderBy(m =>
                {
                    ClassAttribute? attrEditOrder = m.GetAttribute("editorder");
                    return attrEditOrder != null ? attrEditOrder.Value.GetInt() : int.MaxValue;
                })
                .ThenBy(m => m.Name.ToString())
                .ToList();

            foreach (Method method in actions)
            {
                try
                {
                    ClassAttribute? attrEditHide = method.GetAttribute("edithide");
                    if (attrEditHide != null && attrEditHide.Value.IsBool && attrEditHide.Value.GetBool())
                        continue;

                    string label = method.Name.ToString();
                    ClassAttribute? attrEditAction = method.GetAttribute("editaction");
                    if (attrEditAction != null && attrEditAction.Value.IsString)
                        label = attrEditAction.Value.GetString();

                    bool isEnabled = true;
                    ClassAttribute? attrEditEnabled = method.GetAttribute("editenabled");
                    if (attrEditEnabled != null && attrEditEnabled.Value.IsBool && attrEditEnabled.Value.GetBool() == false)
                        isEnabled = false;

                    Actions.Add(new InspectorActionViewModel(SelectedNode, method, label, isEnabled));
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to create view model for action '{method.Name}': {ex.Message}");
                }
            }

            HasActions = Actions.Count > 0;
        }

        private void RefreshTransformProperties()
        {
            Dispatcher.UIThread.VerifyAccess();

            foreach (InspectorPropertyViewModelBase propertyVm in Properties)
            {
                // Refresh transform-related properties
                if (propertyVm is TransformViewModel transformVm)
                {
                    transformVm.RefreshValue();
                }
            }
        }

        private void RefreshSceneProperties()
        {
            if (CurrentScene == null || !CurrentScene.IsValid)
            {
                return;
            }

            Class sceneClass = CurrentScene.Class;

            List<Property> sceneProps = sceneClass.Properties
                .Where(p =>
                {
                    if (p.Name.ToString() == "Root")
                    {
                        return false;
                    }

                    ClassAttribute? attrEditHide = p.GetAttribute("edithide");
                    if (attrEditHide != null && attrEditHide.Value.GetBool() == true)
                    {
                        return false;
                    }

                    return true;
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

            foreach (Property property in sceneProps)
            {
                try
                {
                    bool isReadOnly = false;
                    ClassAttribute? attrEditEnabled = property.GetAttribute("editenabled");

                    if (attrEditEnabled != null && attrEditEnabled.Value.GetBool() == false)
                    {
                        isReadOnly = true;
                    }

                    SceneProperties.Add(InspectorViewModelFactory.Create(CurrentScene, property, isReadOnly));
                }
                catch (Exception ex)
                {
                    Logger.Log(LogLevel.Warning, $"Inspector failed to create view model for scene property '{property.Name}': {ex.Message}");
                }
            }

            HasSceneProperties = SceneProperties.Count > 0;
        }

        private void UpdateAddableComponents(IEnumerable<TypeId> existingComponentTypes)
        {
            HashSet<TypeId> existingTypes = [.. existingComponentTypes];

            AddableComponents.Clear();

            foreach ((string label, TypeId typeId) in GetSupportedComponentTypes())
            {
                bool canAdd = !existingTypes.Contains(typeId);
                AddableComponents.Add(new AddComponentOptionViewModel(label, typeId, canAdd));
            }

            HasAddableComponents = AddableComponents.Any(option => option.IsEnabled);

            if (AddComponentCommand is AsyncRelayCommand relayCommand)
            {
                relayCommand.RaiseCanExecuteChanged();
            }
        }

        private static IEnumerable<(string Label, TypeId TypeId)> GetSupportedComponentTypes()
            => s_registeredComponents.Value
                .Where(d => d.IsEditorEnabled)
                .Select(d => (d.Label, d.TypeId));

        private sealed record ComponentTypeDescriptor(
            string Label,
            Func<TypeId> GetTypeId,
            Func<Entity, InspectorComponentViewModelBase> CreateViewModel,
            Action<EntityManager, Entity> AddComponent)
        {
            // Evaluated on first access rather than at static init time
            public TypeId TypeId => GetTypeId();
            public bool IsEditorEnabled => Class.TryGetClass(TypeId)?.GetAttribute("editor")?.GetBool() ?? true;
        }

        private static ComponentTypeDescriptor? BuildDescriptor(Type componentType)
        {
            try
            {
                Class? cls = Class.TryGetClass(componentType);

                if (cls == null || !cls.Value.IsValid)
                    return null;

                Class componentClass = cls.Value;
                ClassAttribute? attrLabel = componentClass.GetAttribute("label");
                string label = attrLabel.HasValue ? attrLabel.Value.GetString() : componentClass.Name.ToString();

                Type vmType = typeof(InspectorComponentViewModel<>).MakeGenericType(componentType);

                return new ComponentTypeDescriptor(
                    label,
                    () => componentClass.TypeId,
                    entity => (InspectorComponentViewModelBase)Activator.CreateInstance(vmType, entity)!,
                    (mgr, entity) => mgr.AddDefaultComponent(entity, componentClass));
            }
            catch (Exception ex)
            {
                Logger.Log(LogLevel.Warning, $"Inspector failed to build descriptor for '{componentType.Name}': {ex.Message}");
                return null;
            }
        }

        private static readonly Lazy<ComponentTypeDescriptor[]> s_registeredComponents = new(() =>
            AppDomain.CurrentDomain.GetAssemblies()
                .SelectMany(a =>
                {
                    try { return a.GetTypes(); }
                    catch { return Array.Empty<Type>(); }
                })
                .Where(t => t.IsValueType && t.GetInterfaces().Contains(typeof(IComponent)))
                .Select(BuildDescriptor)
                .OfType<ComponentTypeDescriptor>()
                .ToArray());

        private bool CanAddComponent(object? parameter)
        {
            return parameter is AddComponentOptionViewModel option && option.IsEnabled && SelectedNode is Entity;
        }

        private async Task AddComponentAsync(object? parameter)
        {
            if (parameter is not AddComponentOptionViewModel option)
            {
                return;
            }

            if (SelectedNode is not Entity entity || entity.EntityManager == null)
            {
                return;
            }

            try
            {
                await EngineManager.PostToSimThread(() =>
                {
                    EntityManager? mgr = entity.EntityManager;

                    if (mgr == null)
                    {
                        Logger.Log(LogLevel.Warning, "Inspector failed to get EntityManager while adding component");

                        return;
                    }

                    try
                    {
                        ComponentTypeDescriptor? descriptor = s_registeredComponents.Value
                            .FirstOrDefault(d => d.TypeId == option.TypeId);

                        if (descriptor != null)
                            descriptor.AddComponent(mgr, entity);
                        else
                            Logger.Log(LogLevel.Warning, $"Inspector cannot add unsupported component type '{option.Label}'");
                    }
                    catch (Exception ex)
                    {
                        Logger.Log(LogLevel.Warning, $"Inspector failed to add component '{option.Label}': {ex.Message}");
                    }
                });
            }
            finally
            {
                Dispatcher.UIThread.Post(RefreshProperties);
            }
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
                    using BoxedValue resultData = conditionMethod.Value.Invoke(SelectedNode);
                    object? result = resultData.GetValue();

                    if (result is bool boolResult)
                    {
                        return boolResult;
                    }

                    Logger.Log(LogLevel.Warning, $"Inspector editcondition method '{methodName}' on member '{memberName}' did not return a bool");
                }
            }
            else if (attrEditCondition.Value.IsBool)
            {
                return attrEditCondition.Value.GetBool();
            }
            else
            {
                Logger.Log(LogLevel.Warning, $"Inspector editcondition attribute on member '{memberName}' is not a valid type");
            }

            return true; // continue if no condition or invalid condition
        }
    }

    public class AddComponentOptionViewModel : ViewModelBase
    {
        private bool _isEnabled;

        public AddComponentOptionViewModel(string label, TypeId typeId, bool isEnabled)
        {
            Label = label;
            TypeId = typeId;
            _isEnabled = isEnabled;
        }

        public string Label { get; }
        public TypeId TypeId { get; }

        public bool IsEnabled
        {
            get => _isEnabled;
            set => SetProperty(ref _isEnabled, value);
        }
    }
}
