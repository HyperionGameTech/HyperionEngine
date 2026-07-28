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
        public ICommand RemoveComponentCommand { get; }

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
            RemoveComponentCommand = new RelayCommand<object>(RemoveComponent, CanRemoveComponent);
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

            // sort by editororder attribute (if present), then by name
            List<Property> properties = nodeClass.Properties
                .Where(p =>
                {
                    ClassAttribute? attrEditCondition = p.GetAttribute("editcondition");

                    return EvaluateEditCondition(nodeClass, attrEditCondition, p.Name.ToString());
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
                        continue; // skip Components property -- they're handled separately
                    }

                    // skip non-editor properties
                    ClassAttribute? attrEditor = property.GetAttribute("editor");

                    if (attrEditor != null && attrEditor.Value.GetBool() == false)
                    {
                        continue;
                    }

                    bool isReadOnly = false;
                    ClassAttribute? attrEditEnabled = property.GetAttribute("editenabled");

                    if (attrEditEnabled != null && attrEditEnabled.Value.GetBool() == false)
                    {
                        isReadOnly = true;
                    }

                    Properties.Add(InspectorViewModelFactory.Create(
                        SelectedNode, property, isReadOnly, 0, null, null, OnPropertyValueChanged));

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

            // collect actions (methods with editoraction attribute)
            foreach (InspectorActionViewModel actionVm in InspectorActionsHelper.GetActions(SelectedNode, OnPropertyValueChanged))
            {
                Actions.Add(actionVm);
            }

            Logger.Log(LogLevel.Debug, $"Inspector found {Actions.Count} actions for node '{SelectedNode.Name}'");

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

            foreach (InspectorActionViewModel actionVm in InspectorActionsHelper.GetActions(SelectedNode, OnPropertyValueChanged))
            {
                Actions.Add(actionVm);
            }

            HasActions = Actions.Count > 0;
        }

        /// <summary>
        /// Re-reads every property shown for the selected node. A setter can change more than the
        /// value it was given (clamped ranges, packed flag bits, side effects on other members), so
        /// after any write the whole object is read back instead of trusting what the UI sent.
        /// </summary>
        private void OnPropertyValueChanged()
        {
            Dispatcher.UIThread.VerifyAccess();

            if (SelectedNode == null || !SelectedNode.IsValid)
            {
                return;
            }

            foreach (InspectorPropertyViewModelBase propertyVm in Properties)
            {
                propertyVm.RefreshValue();
            }

            foreach (InspectorComponentViewModelBase componentVm in Components)
            {
                componentVm.RefreshProperties();
            }
        }

        private void OnScenePropertyValueChanged()
        {
            Dispatcher.UIThread.VerifyAccess();

            if (CurrentScene == null || !CurrentScene.IsValid)
            {
                return;
            }

            foreach (InspectorPropertyViewModelBase propertyVm in SceneProperties)
            {
                propertyVm.RefreshValue();
            }
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

                    SceneProperties.Add(InspectorViewModelFactory.Create(
                        CurrentScene, property, isReadOnly, 0, null, null, OnScenePropertyValueChanged));
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

        private bool CanRemoveComponent(object? parameter)
        {
            return parameter is InspectorComponentViewModelBase && SelectedNode is Entity;
        }

        private void RemoveComponent(object? parameter)
        {
            if (parameter is not InspectorComponentViewModelBase componentVm)
                return;

            if (SelectedNode is not Entity entity || entity.EntityManager == null)
                return;

            MessageBox.Info("Remove Component", $"Are you sure you want to remove the {componentVm.Label} component from {entity.Name}?")
                .Button("Remove", () => _ = RemoveComponentConfirmed(componentVm, entity))
                .Button("Cancel", () => { })
                .Show();
        }

        private async Task RemoveComponentConfirmed(InspectorComponentViewModelBase componentVm, Entity entity)
        {
            try
            {
                await EngineManager.PostToSimThread(() =>
                {
                    EntityManager? mgr = entity.EntityManager;

                    if (mgr == null)
                    {
                        Logger.Log(LogLevel.Warning, "Inspector failed to get EntityManager while removing component");
                        return;
                    }

                    try
                    {
                        mgr.RemoveComponent(entity, componentVm.TypeId);
                    }
                    catch (Exception ex)
                    {
                        Logger.Log(LogLevel.Warning, $"Inspector failed to remove component '{componentVm.Label}': {ex.Message}");
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
