template <class T>
struct HypClassDecl;

class HypObjectBase;
class UIGridColumn;
class UIGridRow;
class UIGrid;
class UISpacer;
class UIPanel;
class UIMenuBarDropDirection;
class UIMenuItem;
class UIMenuBar;
class UIStage;
class UIDockableItem;
class UIDockableContainer;
class UIImage;
class UISubsystem;
class UIWindow;
class UIDataSource;
class UIElementFactoryBase;
class UIDataSourceBase;
class UIText;
class UIObject;
struct UIObjectSize;
struct UIObjectAspectRatio;
class UIObjectUpdateSizeFlags;
class UIObjectAlignment;
struct UIEventHandlerResult;
class UIObjectBorderFlags;
class UIObjectUpdateType;
class UIObjectFocusState;
enum ScrollAxis : uint8;
class UITab;
class UITabView;
class UIListViewItem;
class UIListView;
class UIListViewOrientation;
class UITextbox;
class UIButton;
enum ScriptCompileStatus : uint32;
enum ScriptLanguage : uint32;
struct ScriptData;
struct WeakName;
struct Name;
struct HashCode;
struct CommandLineArgumentDefinitions;
class CommandLineArguments;
class CommandLineArgumentType;
struct Vertex;
struct VertexAttributeSet;
struct Transform;
struct Triangle;
struct Quaternion;
struct BoundingSphere;
class Matrix4;
class Matrix3;
struct Ray;
struct Frustum;
class Color;
struct BoundingBox;
class Time;
struct Uuid;
class Error;
class Result;
struct TypeId;
class HypClassFlags;
class DynamicLibrary;
class Logger;
class LogChannel;
class AssetLoaderBase;
class TextureAsset;
class ScriptAsset;
enum AssetPackageFlags : uint32;
class AssetPackage;
class AssetRegistry;
class AssetReference;
class AssetObject;
enum AssetObjectFlags : uint32;
struct AssetPath;
struct MeshData;
class MeshAsset;
struct MeshDesc;
class AssetChangeType;
struct AssetLoaderDefinition;
class AssetManager;
class AssetCollector;
class WAVAudioLoader;
class OgreXMLSkeletonLoader;
class UILoader;
class PLYModelLoader;
class FBOMModelLoader;
class OBJModelLoader;
class FBXModelLoader;
class OgreXMLModelLoader;
class FontAtlasLoader;
class FontFaceLoader;
class TextureLoader;
class MTLMaterialLoader;
class JSONLoader;
struct OctantId;
class KeyCode;
struct KeyboardEvent;
class InputManager;
class MouseButtonState;
struct MouseEvent;
enum MouseButtonKey : uint32;
class InputHandlerBase;
class NullInputHandler;
class Game;
class StreamableBase;
struct StreamableKey;
struct StreamingCellNeighbor;
struct StreamingCellInfo;
class StreamingCell;
class StreamingCellState;
class StreamingManager;
class StreamingVolumeShape;
class StreamingVolumeBase;
class SDLAppContext;
class AppContextBase;
class SDLApplicationWindow;
class Win32ApplicationWindow;
class ApplicationWindow;
class Win32AppContext;
class AudioSource;
class AudioSourceFormat;
class AudioSourceState;
enum GlobalRenderBuffer : uint8;
enum GlobalRendererType : uint32;
class FramebufferBase;
enum RenderBucket : uint32;
class DescriptorSetElementType;
struct DescriptorTableDeclaration;
class DescriptorTableBase;
struct DescriptorSetOffsetMap;
struct DescriptorDeclaration;
struct DescriptorSetDeclaration;
class DescriptorSetBase;
class DescriptorSetDeclarationFlags;
enum DescriptorSlot : uint32;
struct DescriptorSetLayoutElement;
struct DescriptorTableOffsetMap;
class Material;
class MaterialTextureKey;
class MaterialGroup;
struct MaterialParameterValue;
class MaterialParameters;
enum MaterialParameterKey : uint64;
enum MaterialParameterType : uint32;
class MaterialTextures;
struct MaterialParameter;
struct BlendFunction;
enum Topology : uint32;
enum BlendModeFactor : uint32;
enum ImageUsage : uint32;
enum ResourceState : uint32;
enum TextureFormat : uint32;
enum GpuElemType : uint32;
enum StencilOp : uint8;
enum TextureWrapMode : uint32;
enum FaceCullMode : uint32;
struct StencilFunction;
struct TextureDesc;
enum StencilCompareOp : uint8;
enum ImageSupport : uint8;
enum FillMode : uint32;
enum DefaultImageFormat : uint8;
enum TextureType : uint32;
struct TextureData;
enum TextureFilterMode : uint32;
enum TextureBaseFormat : uint32;
struct SSGIConfig;
class ComputePipelineBase;
class TonemapPass;
class LightmapPass;
class RaytracingPassData;
class DeferredPassData;
class ReflectionsPass;
class DeferredPass;
class EnvGridPass;
class SwapchainBase;
class PassData;
struct RendererConfig;
class GaussianSplattingInstance;
class GaussianSplatting;
class AttachmentBase;
class GpuBufferType;
class GpuBufferBase;
enum ShaderModuleType : uint32;
class ShaderBase;
class UIPassData;
class GBuffer;
enum GBufferTargetName : uint32;
class PostFXPass;
class RenderGroup;
class DeviceBase;
class GraphicsPipelineBase;
class ParticleSpawner;
class ParticleSystem;
struct SSRRendererConfig;
class CommandBufferBase;
struct MeshInstanceData;
class SamplerBase;
enum MaterialAttributeFlags : uint32;
struct MaterialAttributes;
struct MeshAttributes;
class FrameBase;
class Texture;
class GpuImageViewBase;
class GpuImageBase;
struct HBAOConfig;
class HBAO;
enum RenderStatsCountType : uint32;
struct RenderStats;
struct RenderStatsCounts;
class FullScreenPass;
class Mesh;
enum MeshFlags : uint32;
class RenderSubsystem;
struct MeshRaytracingData;
struct EnvProbeSphericalHarmonics;
struct ShaderProperty;
struct DescriptorUsage;
struct CompiledShader;
struct ShaderDefinition;
struct CompiledShaderBatch;
class ShaderProperties;
struct DescriptorUsageSet;
class DescriptorUsageFlags;
struct DescriptorUsageType;
enum ShadowMapType : uint32;
enum ShadowMapFilter : uint32;
class ShadowPassData;
struct ShadowMapAtlas;
struct ShadowMapAtlasElement;
struct RaytracingReflectionsConfig;
class TLASBase;
class BLASBase;
class RaytracingPipelineBase;
class EnvGridPassData;
class Lightmapper_CpuPathTracing;
class Lightmapper_GpuPathTracing;
class LightmapperSubsystem;
class LightmapShadingType;
struct LightmapperConfig;
class Lightmapper;
class LightmapTraceMode;
class EnvProbePassData;
class VulkanDescriptorTable;
class VulkanDescriptorSet;
class VulkanSampler;
class VulkanSwapchain;
class VulkanGpuImage;
class VulkanGpuBuffer;
class VulkanDevice;
class VulkanShader;
class VulkanFence;
class VulkanDeviceQueueType;
class VulkanCommandBuffer;
class VulkanFramebuffer;
class VulkanFrame;
class VulkanGpuImageView;
class VulkanGraphicsPipeline;
class VulkanAttachment;
class VulkanRenderPass;
class VulkanComputePipeline;
class VulkanBLAS;
class VulkanAccelerationGeometry;
class VulkanTLAS;
class VulkanRaytracingPipeline;
class SkydomeRenderer;
class FontAtlas;
struct FontAtlasTextureSet;
struct DebugDrawerConfig;
struct PhysicsMaterial;
class PhysicsShapeType;
class RigidBody;
class SpherePhysicsShape;
class ConvexHullPhysicsShape;
class BoxPhysicsShape;
class PhysicsShape;
class PlanePhysicsShape;
enum EnvProbeType : uint32;
class EnvProbe;
class ReflectionProbe;
class SkyProbe;
struct GameState;
class GameStateMode;
class World;
struct VisibilityState;
struct VisibilityStateSnapshot;
class SystemBase;
class ComponentRWFlags;
struct ComponentInfo;
class Subsystem;
enum NodeFlags : uint32;
class NodeTagSet;
struct NodeTag;
class Node;
class DirectionalLight;
enum LightFlags : uint32;
class AreaRectLight;
enum LightType : uint32;
class SpotLight;
class PointLight;
class Light;
class View;
struct BVHNode;
class EntityManager;
class ScriptableSystem;
class Scene;
class SceneFlags;
class EnvGrid;
class LegacyEnvGrid;
class Entity;
class PerspectiveCameraController;
class CameraTrackController;
class FirstPersonCameraInputHandler;
class FirstPersonCameraControllerMode;
class FirstPersonCameraController;
class FollowCameraController;
class OrthoCameraController;
class CameraProjectionMode;
class CameraFlags;
class CameraController;
class Camera;
class NullCameraController;
class CameraStreamingVolume;
class AnimationTrack;
class Animation;
struct Keyframe;
class Skeleton;
class Bone;
struct LightmapElement;
struct LightmapVolumeAtlas;
class LightmapVolume;
enum LightmapTextureType : uint32;
struct MeshComponent;
class VisibilityStateFlags;
struct VisibilityStateComponent;
struct BoundingBoxComponent;
struct TransformComponent;
class ScriptComponentFlags;
struct ScriptComponent;
struct UIComponent;
struct AnimationComponent;
struct AnimationPlaybackState;
class AnimationLoopMode;
class AnimationPlaybackStatus;
struct SkyComponent;
enum AudioLoopMode : uint8;
struct AudioComponent;
class AudioComponentFlags;
enum AudioPlaybackStatus : uint8;
struct AudioPlaybackState;
struct LightmapVolumeComponent;
struct NodeLinkComponent;
struct RigidBodyComponent;
class RigidBodyComponentFlags;
struct ReflectionProbeComponent;
class WorldAABBUpdaterSystem;
class PhysicsSystem;
class SkySystem;
class VisibilityStateUpdaterSystem;
class AudioSystem;
class EntityRenderProxySystem_Mesh;
class EntityMeshDirtyStateSystem;
class ScriptSystem;
class LightmapSystem;
class AnimationSystem;
struct WorldGridLayerInfo;
class WorldGridLayer;
class WorldGrid;
class TerrainStreamingCell;
class TerrainWorldGridLayer;
class EngineStats;
class EngineStatGroup;
struct EngineStatEntry;
class EngineDriver;
class EditorActionStack;
class EditorActionStackState;
class EditorCameraInputHandler;
class EditorCameraControllerMode;
class EditorCameraController;
class EditorState;
class EditorProject;
class EditorActionBase;
class FunctionalEditorAction;
class EditorTaskBase;
class LongRunningEditorTask;
class TickableEditorTask;
class HyperionEditor;
class NullEditorManipulationWidget;
class TranslateEditorManipulationWidget;
class EditorSubsystem;
class GenerateLightmapsEditorTask;
class EditorManipulationWidgetBase;
class EditorPropertyPanelBase;
class TransformEditorPropertyPanel;
class TextureEditorDebugOverlay;
class TextEditorDebugOverlay;
class EditorDebugOverlayBase;
class ConsoleCommandBase;
class ConsoleUI;
class LogEntitiesCommand;

template <>
struct HypClassDecl<HypObjectBase>
{
    using Type = HypObjectBase;
};

template <>
struct HypClassDecl<UIGridColumn>
{
    using Type = UIGridColumn;
};

template <>
struct HypClassDecl<UIGridRow>
{
    using Type = UIGridRow;
};

template <>
struct HypClassDecl<UIGrid>
{
    using Type = UIGrid;
};

template <>
struct HypClassDecl<UISpacer>
{
    using Type = UISpacer;
};

template <>
struct HypClassDecl<UIPanel>
{
    using Type = UIPanel;
};

template <>
struct HypClassDecl<UIMenuBarDropDirection>
{
    using Type = UIMenuBarDropDirection;
};

template <>
struct HypClassDecl<UIMenuItem>
{
    using Type = UIMenuItem;
};

template <>
struct HypClassDecl<UIMenuBar>
{
    using Type = UIMenuBar;
};

template <>
struct HypClassDecl<UIStage>
{
    using Type = UIStage;
};

template <>
struct HypClassDecl<UIDockableItem>
{
    using Type = UIDockableItem;
};

template <>
struct HypClassDecl<UIDockableContainer>
{
    using Type = UIDockableContainer;
};

template <>
struct HypClassDecl<UIImage>
{
    using Type = UIImage;
};

template <>
struct HypClassDecl<UISubsystem>
{
    using Type = UISubsystem;
};

template <>
struct HypClassDecl<UIWindow>
{
    using Type = UIWindow;
};

template <>
struct HypClassDecl<UIDataSource>
{
    using Type = UIDataSource;
};

template <>
struct HypClassDecl<UIElementFactoryBase>
{
    using Type = UIElementFactoryBase;
};

template <>
struct HypClassDecl<UIDataSourceBase>
{
    using Type = UIDataSourceBase;
};

template <>
struct HypClassDecl<UIText>
{
    using Type = UIText;
};

template <>
struct HypClassDecl<UIObject>
{
    using Type = UIObject;
};

template <>
struct HypClassDecl<UIObjectSize>
{
    using Type = UIObjectSize;
};

template <>
struct HypClassDecl<UIObjectAspectRatio>
{
    using Type = UIObjectAspectRatio;
};

template <>
struct HypClassDecl<UIObjectUpdateSizeFlags>
{
    using Type = UIObjectUpdateSizeFlags;
};

template <>
struct HypClassDecl<UIObjectAlignment>
{
    using Type = UIObjectAlignment;
};

template <>
struct HypClassDecl<UIEventHandlerResult>
{
    using Type = UIEventHandlerResult;
};

template <>
struct HypClassDecl<UIObjectBorderFlags>
{
    using Type = UIObjectBorderFlags;
};

template <>
struct HypClassDecl<UIObjectUpdateType>
{
    using Type = UIObjectUpdateType;
};

template <>
struct HypClassDecl<UIObjectFocusState>
{
    using Type = UIObjectFocusState;
};

template <>
struct HypClassDecl<ScrollAxis>
{
    using Type = ScrollAxis;
};

template <>
struct HypClassDecl<UITab>
{
    using Type = UITab;
};

template <>
struct HypClassDecl<UITabView>
{
    using Type = UITabView;
};

template <>
struct HypClassDecl<UIListViewItem>
{
    using Type = UIListViewItem;
};

template <>
struct HypClassDecl<UIListView>
{
    using Type = UIListView;
};

template <>
struct HypClassDecl<UIListViewOrientation>
{
    using Type = UIListViewOrientation;
};

template <>
struct HypClassDecl<UITextbox>
{
    using Type = UITextbox;
};

template <>
struct HypClassDecl<UIButton>
{
    using Type = UIButton;
};

template <>
struct HypClassDecl<ScriptCompileStatus>
{
    using Type = ScriptCompileStatus;
};

template <>
struct HypClassDecl<ScriptLanguage>
{
    using Type = ScriptLanguage;
};

template <>
struct HypClassDecl<ScriptData>
{
    using Type = ScriptData;
};

template <>
struct HypClassDecl<WeakName>
{
    using Type = WeakName;
};

template <>
struct HypClassDecl<Name>
{
    using Type = Name;
};

template <>
struct HypClassDecl<HashCode>
{
    using Type = HashCode;
};

template <>
struct HypClassDecl<CommandLineArgumentDefinitions>
{
    using Type = CommandLineArgumentDefinitions;
};

template <>
struct HypClassDecl<CommandLineArguments>
{
    using Type = CommandLineArguments;
};

template <>
struct HypClassDecl<CommandLineArgumentType>
{
    using Type = CommandLineArgumentType;
};

template <>
struct HypClassDecl<Vertex>
{
    using Type = Vertex;
};

template <>
struct HypClassDecl<VertexAttributeSet>
{
    using Type = VertexAttributeSet;
};

template <>
struct HypClassDecl<Transform>
{
    using Type = Transform;
};

template <>
struct HypClassDecl<Triangle>
{
    using Type = Triangle;
};

template <>
struct HypClassDecl<Quaternion>
{
    using Type = Quaternion;
};

template <>
struct HypClassDecl<BoundingSphere>
{
    using Type = BoundingSphere;
};

template <>
struct HypClassDecl<Matrix4>
{
    using Type = Matrix4;
};

template <>
struct HypClassDecl<Matrix3>
{
    using Type = Matrix3;
};

template <>
struct HypClassDecl<Ray>
{
    using Type = Ray;
};

template <>
struct HypClassDecl<Frustum>
{
    using Type = Frustum;
};

template <>
struct HypClassDecl<Color>
{
    using Type = Color;
};

template <>
struct HypClassDecl<BoundingBox>
{
    using Type = BoundingBox;
};

template <>
struct HypClassDecl<Time>
{
    using Type = Time;
};

template <>
struct HypClassDecl<Uuid>
{
    using Type = Uuid;
};

template <>
struct HypClassDecl<Error>
{
    using Type = Error;
};

template <>
struct HypClassDecl<Result>
{
    using Type = Result;
};

template <>
struct HypClassDecl<TypeId>
{
    using Type = TypeId;
};

template <>
struct HypClassDecl<HypClassFlags>
{
    using Type = HypClassFlags;
};

template <>
struct HypClassDecl<DynamicLibrary>
{
    using Type = DynamicLibrary;
};

template <>
struct HypClassDecl<Logger>
{
    using Type = Logger;
};

template <>
struct HypClassDecl<LogChannel>
{
    using Type = LogChannel;
};

template <>
struct HypClassDecl<AssetLoaderBase>
{
    using Type = AssetLoaderBase;
};

template <>
struct HypClassDecl<TextureAsset>
{
    using Type = TextureAsset;
};

template <>
struct HypClassDecl<ScriptAsset>
{
    using Type = ScriptAsset;
};

template <>
struct HypClassDecl<AssetPackageFlags>
{
    using Type = AssetPackageFlags;
};

template <>
struct HypClassDecl<AssetPackage>
{
    using Type = AssetPackage;
};

template <>
struct HypClassDecl<AssetRegistry>
{
    using Type = AssetRegistry;
};

template <>
struct HypClassDecl<AssetReference>
{
    using Type = AssetReference;
};

template <>
struct HypClassDecl<AssetObject>
{
    using Type = AssetObject;
};

template <>
struct HypClassDecl<AssetObjectFlags>
{
    using Type = AssetObjectFlags;
};

template <>
struct HypClassDecl<AssetPath>
{
    using Type = AssetPath;
};

template <>
struct HypClassDecl<MeshData>
{
    using Type = MeshData;
};

template <>
struct HypClassDecl<MeshAsset>
{
    using Type = MeshAsset;
};

template <>
struct HypClassDecl<MeshDesc>
{
    using Type = MeshDesc;
};

template <>
struct HypClassDecl<AssetChangeType>
{
    using Type = AssetChangeType;
};

template <>
struct HypClassDecl<AssetLoaderDefinition>
{
    using Type = AssetLoaderDefinition;
};

template <>
struct HypClassDecl<AssetManager>
{
    using Type = AssetManager;
};

template <>
struct HypClassDecl<AssetCollector>
{
    using Type = AssetCollector;
};

template <>
struct HypClassDecl<WAVAudioLoader>
{
    using Type = WAVAudioLoader;
};

template <>
struct HypClassDecl<OgreXMLSkeletonLoader>
{
    using Type = OgreXMLSkeletonLoader;
};

template <>
struct HypClassDecl<UILoader>
{
    using Type = UILoader;
};

template <>
struct HypClassDecl<PLYModelLoader>
{
    using Type = PLYModelLoader;
};

template <>
struct HypClassDecl<FBOMModelLoader>
{
    using Type = FBOMModelLoader;
};

template <>
struct HypClassDecl<OBJModelLoader>
{
    using Type = OBJModelLoader;
};

template <>
struct HypClassDecl<FBXModelLoader>
{
    using Type = FBXModelLoader;
};

template <>
struct HypClassDecl<OgreXMLModelLoader>
{
    using Type = OgreXMLModelLoader;
};

template <>
struct HypClassDecl<FontAtlasLoader>
{
    using Type = FontAtlasLoader;
};

template <>
struct HypClassDecl<FontFaceLoader>
{
    using Type = FontFaceLoader;
};

template <>
struct HypClassDecl<TextureLoader>
{
    using Type = TextureLoader;
};

template <>
struct HypClassDecl<MTLMaterialLoader>
{
    using Type = MTLMaterialLoader;
};

template <>
struct HypClassDecl<JSONLoader>
{
    using Type = JSONLoader;
};

template <>
struct HypClassDecl<OctantId>
{
    using Type = OctantId;
};

template <>
struct HypClassDecl<KeyCode>
{
    using Type = KeyCode;
};

template <>
struct HypClassDecl<KeyboardEvent>
{
    using Type = KeyboardEvent;
};

template <>
struct HypClassDecl<InputManager>
{
    using Type = InputManager;
};

template <>
struct HypClassDecl<MouseButtonState>
{
    using Type = MouseButtonState;
};

template <>
struct HypClassDecl<MouseEvent>
{
    using Type = MouseEvent;
};

template <>
struct HypClassDecl<MouseButtonKey>
{
    using Type = MouseButtonKey;
};

template <>
struct HypClassDecl<InputHandlerBase>
{
    using Type = InputHandlerBase;
};

template <>
struct HypClassDecl<NullInputHandler>
{
    using Type = NullInputHandler;
};

template <>
struct HypClassDecl<Game>
{
    using Type = Game;
};

template <>
struct HypClassDecl<StreamableBase>
{
    using Type = StreamableBase;
};

template <>
struct HypClassDecl<StreamableKey>
{
    using Type = StreamableKey;
};

template <>
struct HypClassDecl<StreamingCellNeighbor>
{
    using Type = StreamingCellNeighbor;
};

template <>
struct HypClassDecl<StreamingCellInfo>
{
    using Type = StreamingCellInfo;
};

template <>
struct HypClassDecl<StreamingCell>
{
    using Type = StreamingCell;
};

template <>
struct HypClassDecl<StreamingCellState>
{
    using Type = StreamingCellState;
};

template <>
struct HypClassDecl<StreamingManager>
{
    using Type = StreamingManager;
};

template <>
struct HypClassDecl<StreamingVolumeShape>
{
    using Type = StreamingVolumeShape;
};

template <>
struct HypClassDecl<StreamingVolumeBase>
{
    using Type = StreamingVolumeBase;
};

template <>
struct HypClassDecl<SDLAppContext>
{
    using Type = SDLAppContext;
};

template <>
struct HypClassDecl<AppContextBase>
{
    using Type = AppContextBase;
};

template <>
struct HypClassDecl<SDLApplicationWindow>
{
    using Type = SDLApplicationWindow;
};

template <>
struct HypClassDecl<Win32ApplicationWindow>
{
    using Type = Win32ApplicationWindow;
};

template <>
struct HypClassDecl<ApplicationWindow>
{
    using Type = ApplicationWindow;
};

template <>
struct HypClassDecl<Win32AppContext>
{
    using Type = Win32AppContext;
};

template <>
struct HypClassDecl<AudioSource>
{
    using Type = AudioSource;
};

template <>
struct HypClassDecl<AudioSourceFormat>
{
    using Type = AudioSourceFormat;
};

template <>
struct HypClassDecl<AudioSourceState>
{
    using Type = AudioSourceState;
};

template <>
struct HypClassDecl<GlobalRenderBuffer>
{
    using Type = GlobalRenderBuffer;
};

template <>
struct HypClassDecl<GlobalRendererType>
{
    using Type = GlobalRendererType;
};

template <>
struct HypClassDecl<FramebufferBase>
{
    using Type = FramebufferBase;
};

template <>
struct HypClassDecl<RenderBucket>
{
    using Type = RenderBucket;
};

template <>
struct HypClassDecl<DescriptorSetElementType>
{
    using Type = DescriptorSetElementType;
};

template <>
struct HypClassDecl<DescriptorTableDeclaration>
{
    using Type = DescriptorTableDeclaration;
};

template <>
struct HypClassDecl<DescriptorTableBase>
{
    using Type = DescriptorTableBase;
};

template <>
struct HypClassDecl<DescriptorSetOffsetMap>
{
    using Type = DescriptorSetOffsetMap;
};

template <>
struct HypClassDecl<DescriptorDeclaration>
{
    using Type = DescriptorDeclaration;
};

template <>
struct HypClassDecl<DescriptorSetDeclaration>
{
    using Type = DescriptorSetDeclaration;
};

template <>
struct HypClassDecl<DescriptorSetBase>
{
    using Type = DescriptorSetBase;
};

template <>
struct HypClassDecl<DescriptorSetDeclarationFlags>
{
    using Type = DescriptorSetDeclarationFlags;
};

template <>
struct HypClassDecl<DescriptorSlot>
{
    using Type = DescriptorSlot;
};

template <>
struct HypClassDecl<DescriptorSetLayoutElement>
{
    using Type = DescriptorSetLayoutElement;
};

template <>
struct HypClassDecl<DescriptorTableOffsetMap>
{
    using Type = DescriptorTableOffsetMap;
};

template <>
struct HypClassDecl<Material>
{
    using Type = Material;
};

template <>
struct HypClassDecl<MaterialTextureKey>
{
    using Type = MaterialTextureKey;
};

template <>
struct HypClassDecl<MaterialGroup>
{
    using Type = MaterialGroup;
};

template <>
struct HypClassDecl<MaterialParameterValue>
{
    using Type = MaterialParameterValue;
};

template <>
struct HypClassDecl<MaterialParameters>
{
    using Type = MaterialParameters;
};

template <>
struct HypClassDecl<MaterialParameterKey>
{
    using Type = MaterialParameterKey;
};

template <>
struct HypClassDecl<MaterialParameterType>
{
    using Type = MaterialParameterType;
};

template <>
struct HypClassDecl<MaterialTextures>
{
    using Type = MaterialTextures;
};

template <>
struct HypClassDecl<MaterialParameter>
{
    using Type = MaterialParameter;
};

template <>
struct HypClassDecl<BlendFunction>
{
    using Type = BlendFunction;
};

template <>
struct HypClassDecl<Topology>
{
    using Type = Topology;
};

template <>
struct HypClassDecl<BlendModeFactor>
{
    using Type = BlendModeFactor;
};

template <>
struct HypClassDecl<ImageUsage>
{
    using Type = ImageUsage;
};

template <>
struct HypClassDecl<ResourceState>
{
    using Type = ResourceState;
};

template <>
struct HypClassDecl<TextureFormat>
{
    using Type = TextureFormat;
};

template <>
struct HypClassDecl<GpuElemType>
{
    using Type = GpuElemType;
};

template <>
struct HypClassDecl<StencilOp>
{
    using Type = StencilOp;
};

template <>
struct HypClassDecl<TextureWrapMode>
{
    using Type = TextureWrapMode;
};

template <>
struct HypClassDecl<FaceCullMode>
{
    using Type = FaceCullMode;
};

template <>
struct HypClassDecl<StencilFunction>
{
    using Type = StencilFunction;
};

template <>
struct HypClassDecl<TextureDesc>
{
    using Type = TextureDesc;
};

template <>
struct HypClassDecl<StencilCompareOp>
{
    using Type = StencilCompareOp;
};

template <>
struct HypClassDecl<ImageSupport>
{
    using Type = ImageSupport;
};

template <>
struct HypClassDecl<FillMode>
{
    using Type = FillMode;
};

template <>
struct HypClassDecl<DefaultImageFormat>
{
    using Type = DefaultImageFormat;
};

template <>
struct HypClassDecl<TextureType>
{
    using Type = TextureType;
};

template <>
struct HypClassDecl<TextureData>
{
    using Type = TextureData;
};

template <>
struct HypClassDecl<TextureFilterMode>
{
    using Type = TextureFilterMode;
};

template <>
struct HypClassDecl<TextureBaseFormat>
{
    using Type = TextureBaseFormat;
};

template <>
struct HypClassDecl<SSGIConfig>
{
    using Type = SSGIConfig;
};

template <>
struct HypClassDecl<ComputePipelineBase>
{
    using Type = ComputePipelineBase;
};

template <>
struct HypClassDecl<TonemapPass>
{
    using Type = TonemapPass;
};

template <>
struct HypClassDecl<LightmapPass>
{
    using Type = LightmapPass;
};

template <>
struct HypClassDecl<RaytracingPassData>
{
    using Type = RaytracingPassData;
};

template <>
struct HypClassDecl<DeferredPassData>
{
    using Type = DeferredPassData;
};

template <>
struct HypClassDecl<ReflectionsPass>
{
    using Type = ReflectionsPass;
};

template <>
struct HypClassDecl<DeferredPass>
{
    using Type = DeferredPass;
};

template <>
struct HypClassDecl<EnvGridPass>
{
    using Type = EnvGridPass;
};

template <>
struct HypClassDecl<SwapchainBase>
{
    using Type = SwapchainBase;
};

template <>
struct HypClassDecl<PassData>
{
    using Type = PassData;
};

template <>
struct HypClassDecl<RendererConfig>
{
    using Type = RendererConfig;
};

template <>
struct HypClassDecl<GaussianSplattingInstance>
{
    using Type = GaussianSplattingInstance;
};

template <>
struct HypClassDecl<GaussianSplatting>
{
    using Type = GaussianSplatting;
};

template <>
struct HypClassDecl<AttachmentBase>
{
    using Type = AttachmentBase;
};

template <>
struct HypClassDecl<GpuBufferType>
{
    using Type = GpuBufferType;
};

template <>
struct HypClassDecl<GpuBufferBase>
{
    using Type = GpuBufferBase;
};

template <>
struct HypClassDecl<ShaderModuleType>
{
    using Type = ShaderModuleType;
};

template <>
struct HypClassDecl<ShaderBase>
{
    using Type = ShaderBase;
};

template <>
struct HypClassDecl<UIPassData>
{
    using Type = UIPassData;
};

template <>
struct HypClassDecl<GBuffer>
{
    using Type = GBuffer;
};

template <>
struct HypClassDecl<GBufferTargetName>
{
    using Type = GBufferTargetName;
};

template <>
struct HypClassDecl<PostFXPass>
{
    using Type = PostFXPass;
};

template <>
struct HypClassDecl<RenderGroup>
{
    using Type = RenderGroup;
};

template <>
struct HypClassDecl<DeviceBase>
{
    using Type = DeviceBase;
};

template <>
struct HypClassDecl<GraphicsPipelineBase>
{
    using Type = GraphicsPipelineBase;
};

template <>
struct HypClassDecl<ParticleSpawner>
{
    using Type = ParticleSpawner;
};

template <>
struct HypClassDecl<ParticleSystem>
{
    using Type = ParticleSystem;
};

template <>
struct HypClassDecl<SSRRendererConfig>
{
    using Type = SSRRendererConfig;
};

template <>
struct HypClassDecl<CommandBufferBase>
{
    using Type = CommandBufferBase;
};

template <>
struct HypClassDecl<MeshInstanceData>
{
    using Type = MeshInstanceData;
};

template <>
struct HypClassDecl<SamplerBase>
{
    using Type = SamplerBase;
};

template <>
struct HypClassDecl<MaterialAttributeFlags>
{
    using Type = MaterialAttributeFlags;
};

template <>
struct HypClassDecl<MaterialAttributes>
{
    using Type = MaterialAttributes;
};

template <>
struct HypClassDecl<MeshAttributes>
{
    using Type = MeshAttributes;
};

template <>
struct HypClassDecl<FrameBase>
{
    using Type = FrameBase;
};

template <>
struct HypClassDecl<Texture>
{
    using Type = Texture;
};

template <>
struct HypClassDecl<GpuImageViewBase>
{
    using Type = GpuImageViewBase;
};

template <>
struct HypClassDecl<GpuImageBase>
{
    using Type = GpuImageBase;
};

template <>
struct HypClassDecl<HBAOConfig>
{
    using Type = HBAOConfig;
};

template <>
struct HypClassDecl<HBAO>
{
    using Type = HBAO;
};

template <>
struct HypClassDecl<RenderStatsCountType>
{
    using Type = RenderStatsCountType;
};

template <>
struct HypClassDecl<RenderStats>
{
    using Type = RenderStats;
};

template <>
struct HypClassDecl<RenderStatsCounts>
{
    using Type = RenderStatsCounts;
};

template <>
struct HypClassDecl<FullScreenPass>
{
    using Type = FullScreenPass;
};

template <>
struct HypClassDecl<Mesh>
{
    using Type = Mesh;
};

template <>
struct HypClassDecl<MeshFlags>
{
    using Type = MeshFlags;
};

template <>
struct HypClassDecl<RenderSubsystem>
{
    using Type = RenderSubsystem;
};

template <>
struct HypClassDecl<MeshRaytracingData>
{
    using Type = MeshRaytracingData;
};

template <>
struct HypClassDecl<EnvProbeSphericalHarmonics>
{
    using Type = EnvProbeSphericalHarmonics;
};

template <>
struct HypClassDecl<ShaderProperty>
{
    using Type = ShaderProperty;
};

template <>
struct HypClassDecl<DescriptorUsage>
{
    using Type = DescriptorUsage;
};

template <>
struct HypClassDecl<CompiledShader>
{
    using Type = CompiledShader;
};

template <>
struct HypClassDecl<ShaderDefinition>
{
    using Type = ShaderDefinition;
};

template <>
struct HypClassDecl<CompiledShaderBatch>
{
    using Type = CompiledShaderBatch;
};

template <>
struct HypClassDecl<ShaderProperties>
{
    using Type = ShaderProperties;
};

template <>
struct HypClassDecl<DescriptorUsageSet>
{
    using Type = DescriptorUsageSet;
};

template <>
struct HypClassDecl<DescriptorUsageFlags>
{
    using Type = DescriptorUsageFlags;
};

template <>
struct HypClassDecl<DescriptorUsageType>
{
    using Type = DescriptorUsageType;
};

template <>
struct HypClassDecl<ShadowMapType>
{
    using Type = ShadowMapType;
};

template <>
struct HypClassDecl<ShadowMapFilter>
{
    using Type = ShadowMapFilter;
};

template <>
struct HypClassDecl<ShadowPassData>
{
    using Type = ShadowPassData;
};

template <>
struct HypClassDecl<ShadowMapAtlas>
{
    using Type = ShadowMapAtlas;
};

template <>
struct HypClassDecl<ShadowMapAtlasElement>
{
    using Type = ShadowMapAtlasElement;
};

template <>
struct HypClassDecl<RaytracingReflectionsConfig>
{
    using Type = RaytracingReflectionsConfig;
};

template <>
struct HypClassDecl<TLASBase>
{
    using Type = TLASBase;
};

template <>
struct HypClassDecl<BLASBase>
{
    using Type = BLASBase;
};

template <>
struct HypClassDecl<RaytracingPipelineBase>
{
    using Type = RaytracingPipelineBase;
};

template <>
struct HypClassDecl<EnvGridPassData>
{
    using Type = EnvGridPassData;
};

template <>
struct HypClassDecl<Lightmapper_CpuPathTracing>
{
    using Type = Lightmapper_CpuPathTracing;
};

template <>
struct HypClassDecl<Lightmapper_GpuPathTracing>
{
    using Type = Lightmapper_GpuPathTracing;
};

template <>
struct HypClassDecl<LightmapperSubsystem>
{
    using Type = LightmapperSubsystem;
};

template <>
struct HypClassDecl<LightmapShadingType>
{
    using Type = LightmapShadingType;
};

template <>
struct HypClassDecl<LightmapperConfig>
{
    using Type = LightmapperConfig;
};

template <>
struct HypClassDecl<Lightmapper>
{
    using Type = Lightmapper;
};

template <>
struct HypClassDecl<LightmapTraceMode>
{
    using Type = LightmapTraceMode;
};

template <>
struct HypClassDecl<EnvProbePassData>
{
    using Type = EnvProbePassData;
};

template <>
struct HypClassDecl<VulkanDescriptorTable>
{
    using Type = VulkanDescriptorTable;
};

template <>
struct HypClassDecl<VulkanDescriptorSet>
{
    using Type = VulkanDescriptorSet;
};

template <>
struct HypClassDecl<VulkanSampler>
{
    using Type = VulkanSampler;
};

template <>
struct HypClassDecl<VulkanSwapchain>
{
    using Type = VulkanSwapchain;
};

template <>
struct HypClassDecl<VulkanGpuImage>
{
    using Type = VulkanGpuImage;
};

template <>
struct HypClassDecl<VulkanGpuBuffer>
{
    using Type = VulkanGpuBuffer;
};

template <>
struct HypClassDecl<VulkanDevice>
{
    using Type = VulkanDevice;
};

template <>
struct HypClassDecl<VulkanShader>
{
    using Type = VulkanShader;
};

template <>
struct HypClassDecl<VulkanFence>
{
    using Type = VulkanFence;
};

template <>
struct HypClassDecl<VulkanDeviceQueueType>
{
    using Type = VulkanDeviceQueueType;
};

template <>
struct HypClassDecl<VulkanCommandBuffer>
{
    using Type = VulkanCommandBuffer;
};

template <>
struct HypClassDecl<VulkanFramebuffer>
{
    using Type = VulkanFramebuffer;
};

template <>
struct HypClassDecl<VulkanFrame>
{
    using Type = VulkanFrame;
};

template <>
struct HypClassDecl<VulkanGpuImageView>
{
    using Type = VulkanGpuImageView;
};

template <>
struct HypClassDecl<VulkanGraphicsPipeline>
{
    using Type = VulkanGraphicsPipeline;
};

template <>
struct HypClassDecl<VulkanAttachment>
{
    using Type = VulkanAttachment;
};

template <>
struct HypClassDecl<VulkanRenderPass>
{
    using Type = VulkanRenderPass;
};

template <>
struct HypClassDecl<VulkanComputePipeline>
{
    using Type = VulkanComputePipeline;
};

template <>
struct HypClassDecl<VulkanBLAS>
{
    using Type = VulkanBLAS;
};

template <>
struct HypClassDecl<VulkanAccelerationGeometry>
{
    using Type = VulkanAccelerationGeometry;
};

template <>
struct HypClassDecl<VulkanTLAS>
{
    using Type = VulkanTLAS;
};

template <>
struct HypClassDecl<VulkanRaytracingPipeline>
{
    using Type = VulkanRaytracingPipeline;
};

template <>
struct HypClassDecl<SkydomeRenderer>
{
    using Type = SkydomeRenderer;
};

template <>
struct HypClassDecl<FontAtlas>
{
    using Type = FontAtlas;
};

template <>
struct HypClassDecl<FontAtlasTextureSet>
{
    using Type = FontAtlasTextureSet;
};

template <>
struct HypClassDecl<DebugDrawerConfig>
{
    using Type = DebugDrawerConfig;
};

template <>
struct HypClassDecl<PhysicsMaterial>
{
    using Type = PhysicsMaterial;
};

template <>
struct HypClassDecl<PhysicsShapeType>
{
    using Type = PhysicsShapeType;
};

template <>
struct HypClassDecl<RigidBody>
{
    using Type = RigidBody;
};

template <>
struct HypClassDecl<SpherePhysicsShape>
{
    using Type = SpherePhysicsShape;
};

template <>
struct HypClassDecl<ConvexHullPhysicsShape>
{
    using Type = ConvexHullPhysicsShape;
};

template <>
struct HypClassDecl<BoxPhysicsShape>
{
    using Type = BoxPhysicsShape;
};

template <>
struct HypClassDecl<PhysicsShape>
{
    using Type = PhysicsShape;
};

template <>
struct HypClassDecl<PlanePhysicsShape>
{
    using Type = PlanePhysicsShape;
};

template <>
struct HypClassDecl<EnvProbeType>
{
    using Type = EnvProbeType;
};

template <>
struct HypClassDecl<EnvProbe>
{
    using Type = EnvProbe;
};

template <>
struct HypClassDecl<ReflectionProbe>
{
    using Type = ReflectionProbe;
};

template <>
struct HypClassDecl<SkyProbe>
{
    using Type = SkyProbe;
};

template <>
struct HypClassDecl<GameState>
{
    using Type = GameState;
};

template <>
struct HypClassDecl<GameStateMode>
{
    using Type = GameStateMode;
};

template <>
struct HypClassDecl<World>
{
    using Type = World;
};

template <>
struct HypClassDecl<VisibilityState>
{
    using Type = VisibilityState;
};

template <>
struct HypClassDecl<VisibilityStateSnapshot>
{
    using Type = VisibilityStateSnapshot;
};

template <>
struct HypClassDecl<SystemBase>
{
    using Type = SystemBase;
};

template <>
struct HypClassDecl<ComponentRWFlags>
{
    using Type = ComponentRWFlags;
};

template <>
struct HypClassDecl<ComponentInfo>
{
    using Type = ComponentInfo;
};

template <>
struct HypClassDecl<Subsystem>
{
    using Type = Subsystem;
};

template <>
struct HypClassDecl<NodeFlags>
{
    using Type = NodeFlags;
};

template <>
struct HypClassDecl<NodeTagSet>
{
    using Type = NodeTagSet;
};

template <>
struct HypClassDecl<NodeTag>
{
    using Type = NodeTag;
};

template <>
struct HypClassDecl<Node>
{
    using Type = Node;
};

template <>
struct HypClassDecl<DirectionalLight>
{
    using Type = DirectionalLight;
};

template <>
struct HypClassDecl<LightFlags>
{
    using Type = LightFlags;
};

template <>
struct HypClassDecl<AreaRectLight>
{
    using Type = AreaRectLight;
};

template <>
struct HypClassDecl<LightType>
{
    using Type = LightType;
};

template <>
struct HypClassDecl<SpotLight>
{
    using Type = SpotLight;
};

template <>
struct HypClassDecl<PointLight>
{
    using Type = PointLight;
};

template <>
struct HypClassDecl<Light>
{
    using Type = Light;
};

template <>
struct HypClassDecl<View>
{
    using Type = View;
};

template <>
struct HypClassDecl<BVHNode>
{
    using Type = BVHNode;
};

template <>
struct HypClassDecl<EntityManager>
{
    using Type = EntityManager;
};

template <>
struct HypClassDecl<ScriptableSystem>
{
    using Type = ScriptableSystem;
};

template <>
struct HypClassDecl<Scene>
{
    using Type = Scene;
};

template <>
struct HypClassDecl<SceneFlags>
{
    using Type = SceneFlags;
};

template <>
struct HypClassDecl<EnvGrid>
{
    using Type = EnvGrid;
};

template <>
struct HypClassDecl<LegacyEnvGrid>
{
    using Type = LegacyEnvGrid;
};

template <>
struct HypClassDecl<Entity>
{
    using Type = Entity;
};

template <>
struct HypClassDecl<PerspectiveCameraController>
{
    using Type = PerspectiveCameraController;
};

template <>
struct HypClassDecl<CameraTrackController>
{
    using Type = CameraTrackController;
};

template <>
struct HypClassDecl<FirstPersonCameraInputHandler>
{
    using Type = FirstPersonCameraInputHandler;
};

template <>
struct HypClassDecl<FirstPersonCameraControllerMode>
{
    using Type = FirstPersonCameraControllerMode;
};

template <>
struct HypClassDecl<FirstPersonCameraController>
{
    using Type = FirstPersonCameraController;
};

template <>
struct HypClassDecl<FollowCameraController>
{
    using Type = FollowCameraController;
};

template <>
struct HypClassDecl<OrthoCameraController>
{
    using Type = OrthoCameraController;
};

template <>
struct HypClassDecl<CameraProjectionMode>
{
    using Type = CameraProjectionMode;
};

template <>
struct HypClassDecl<CameraFlags>
{
    using Type = CameraFlags;
};

template <>
struct HypClassDecl<CameraController>
{
    using Type = CameraController;
};

template <>
struct HypClassDecl<Camera>
{
    using Type = Camera;
};

template <>
struct HypClassDecl<NullCameraController>
{
    using Type = NullCameraController;
};

template <>
struct HypClassDecl<CameraStreamingVolume>
{
    using Type = CameraStreamingVolume;
};

template <>
struct HypClassDecl<AnimationTrack>
{
    using Type = AnimationTrack;
};

template <>
struct HypClassDecl<Animation>
{
    using Type = Animation;
};

template <>
struct HypClassDecl<Keyframe>
{
    using Type = Keyframe;
};

template <>
struct HypClassDecl<Skeleton>
{
    using Type = Skeleton;
};

template <>
struct HypClassDecl<Bone>
{
    using Type = Bone;
};

template <>
struct HypClassDecl<LightmapElement>
{
    using Type = LightmapElement;
};

template <>
struct HypClassDecl<LightmapVolumeAtlas>
{
    using Type = LightmapVolumeAtlas;
};

template <>
struct HypClassDecl<LightmapVolume>
{
    using Type = LightmapVolume;
};

template <>
struct HypClassDecl<LightmapTextureType>
{
    using Type = LightmapTextureType;
};

template <>
struct HypClassDecl<MeshComponent>
{
    using Type = MeshComponent;
};

template <>
struct HypClassDecl<VisibilityStateFlags>
{
    using Type = VisibilityStateFlags;
};

template <>
struct HypClassDecl<VisibilityStateComponent>
{
    using Type = VisibilityStateComponent;
};

template <>
struct HypClassDecl<BoundingBoxComponent>
{
    using Type = BoundingBoxComponent;
};

template <>
struct HypClassDecl<TransformComponent>
{
    using Type = TransformComponent;
};

template <>
struct HypClassDecl<ScriptComponentFlags>
{
    using Type = ScriptComponentFlags;
};

template <>
struct HypClassDecl<ScriptComponent>
{
    using Type = ScriptComponent;
};

template <>
struct HypClassDecl<UIComponent>
{
    using Type = UIComponent;
};

template <>
struct HypClassDecl<AnimationComponent>
{
    using Type = AnimationComponent;
};

template <>
struct HypClassDecl<AnimationPlaybackState>
{
    using Type = AnimationPlaybackState;
};

template <>
struct HypClassDecl<AnimationLoopMode>
{
    using Type = AnimationLoopMode;
};

template <>
struct HypClassDecl<AnimationPlaybackStatus>
{
    using Type = AnimationPlaybackStatus;
};

template <>
struct HypClassDecl<SkyComponent>
{
    using Type = SkyComponent;
};

template <>
struct HypClassDecl<AudioLoopMode>
{
    using Type = AudioLoopMode;
};

template <>
struct HypClassDecl<AudioComponent>
{
    using Type = AudioComponent;
};

template <>
struct HypClassDecl<AudioComponentFlags>
{
    using Type = AudioComponentFlags;
};

template <>
struct HypClassDecl<AudioPlaybackStatus>
{
    using Type = AudioPlaybackStatus;
};

template <>
struct HypClassDecl<AudioPlaybackState>
{
    using Type = AudioPlaybackState;
};

template <>
struct HypClassDecl<LightmapVolumeComponent>
{
    using Type = LightmapVolumeComponent;
};

template <>
struct HypClassDecl<NodeLinkComponent>
{
    using Type = NodeLinkComponent;
};

template <>
struct HypClassDecl<RigidBodyComponent>
{
    using Type = RigidBodyComponent;
};

template <>
struct HypClassDecl<RigidBodyComponentFlags>
{
    using Type = RigidBodyComponentFlags;
};

template <>
struct HypClassDecl<ReflectionProbeComponent>
{
    using Type = ReflectionProbeComponent;
};

template <>
struct HypClassDecl<WorldAABBUpdaterSystem>
{
    using Type = WorldAABBUpdaterSystem;
};

template <>
struct HypClassDecl<PhysicsSystem>
{
    using Type = PhysicsSystem;
};

template <>
struct HypClassDecl<SkySystem>
{
    using Type = SkySystem;
};

template <>
struct HypClassDecl<VisibilityStateUpdaterSystem>
{
    using Type = VisibilityStateUpdaterSystem;
};

template <>
struct HypClassDecl<AudioSystem>
{
    using Type = AudioSystem;
};

template <>
struct HypClassDecl<EntityRenderProxySystem_Mesh>
{
    using Type = EntityRenderProxySystem_Mesh;
};

template <>
struct HypClassDecl<EntityMeshDirtyStateSystem>
{
    using Type = EntityMeshDirtyStateSystem;
};

template <>
struct HypClassDecl<ScriptSystem>
{
    using Type = ScriptSystem;
};

template <>
struct HypClassDecl<LightmapSystem>
{
    using Type = LightmapSystem;
};

template <>
struct HypClassDecl<AnimationSystem>
{
    using Type = AnimationSystem;
};

template <>
struct HypClassDecl<WorldGridLayerInfo>
{
    using Type = WorldGridLayerInfo;
};

template <>
struct HypClassDecl<WorldGridLayer>
{
    using Type = WorldGridLayer;
};

template <>
struct HypClassDecl<WorldGrid>
{
    using Type = WorldGrid;
};

template <>
struct HypClassDecl<TerrainStreamingCell>
{
    using Type = TerrainStreamingCell;
};

template <>
struct HypClassDecl<TerrainWorldGridLayer>
{
    using Type = TerrainWorldGridLayer;
};

template <>
struct HypClassDecl<EngineStats>
{
    using Type = EngineStats;
};

template <>
struct HypClassDecl<EngineStatGroup>
{
    using Type = EngineStatGroup;
};

template <>
struct HypClassDecl<EngineStatEntry>
{
    using Type = EngineStatEntry;
};

template <>
struct HypClassDecl<EngineDriver>
{
    using Type = EngineDriver;
};

template <>
struct HypClassDecl<EditorActionStack>
{
    using Type = EditorActionStack;
};

template <>
struct HypClassDecl<EditorActionStackState>
{
    using Type = EditorActionStackState;
};

template <>
struct HypClassDecl<EditorCameraInputHandler>
{
    using Type = EditorCameraInputHandler;
};

template <>
struct HypClassDecl<EditorCameraControllerMode>
{
    using Type = EditorCameraControllerMode;
};

template <>
struct HypClassDecl<EditorCameraController>
{
    using Type = EditorCameraController;
};

template <>
struct HypClassDecl<EditorState>
{
    using Type = EditorState;
};

template <>
struct HypClassDecl<EditorProject>
{
    using Type = EditorProject;
};

template <>
struct HypClassDecl<EditorActionBase>
{
    using Type = EditorActionBase;
};

template <>
struct HypClassDecl<FunctionalEditorAction>
{
    using Type = FunctionalEditorAction;
};

template <>
struct HypClassDecl<EditorTaskBase>
{
    using Type = EditorTaskBase;
};

template <>
struct HypClassDecl<LongRunningEditorTask>
{
    using Type = LongRunningEditorTask;
};

template <>
struct HypClassDecl<TickableEditorTask>
{
    using Type = TickableEditorTask;
};

template <>
struct HypClassDecl<HyperionEditor>
{
    using Type = HyperionEditor;
};

template <>
struct HypClassDecl<NullEditorManipulationWidget>
{
    using Type = NullEditorManipulationWidget;
};

template <>
struct HypClassDecl<TranslateEditorManipulationWidget>
{
    using Type = TranslateEditorManipulationWidget;
};

template <>
struct HypClassDecl<EditorSubsystem>
{
    using Type = EditorSubsystem;
};

template <>
struct HypClassDecl<GenerateLightmapsEditorTask>
{
    using Type = GenerateLightmapsEditorTask;
};

template <>
struct HypClassDecl<EditorManipulationWidgetBase>
{
    using Type = EditorManipulationWidgetBase;
};

template <>
struct HypClassDecl<EditorPropertyPanelBase>
{
    using Type = EditorPropertyPanelBase;
};

template <>
struct HypClassDecl<TransformEditorPropertyPanel>
{
    using Type = TransformEditorPropertyPanel;
};

template <>
struct HypClassDecl<TextureEditorDebugOverlay>
{
    using Type = TextureEditorDebugOverlay;
};

template <>
struct HypClassDecl<TextEditorDebugOverlay>
{
    using Type = TextEditorDebugOverlay;
};

template <>
struct HypClassDecl<EditorDebugOverlayBase>
{
    using Type = EditorDebugOverlayBase;
};

template <>
struct HypClassDecl<ConsoleCommandBase>
{
    using Type = ConsoleCommandBase;
};

template <>
struct HypClassDecl<ConsoleUI>
{
    using Type = ConsoleUI;
};

template <>
struct HypClassDecl<LogEntitiesCommand>
{
    using Type = LogEntitiesCommand;
};

