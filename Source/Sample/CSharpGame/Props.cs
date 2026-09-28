using System;
using System.Collections.Generic;
using Hyperion;

namespace Hyperion.Samples
{
    public enum PropShape
    {
        Box,
        Sphere
    }

    public sealed class Props
    {
        private readonly Node _root;

        private readonly Mesh _cubeMesh;
        private readonly Mesh _sphereMesh;

        private readonly BoxPhysicsShape _boxShape;
        private readonly SpherePhysicsShape _sphereShape;

        private readonly Dictionary<(Color, float), Material> _materials = new Dictionary<(Color, float), Material>();

        public Props(Node root)
        {
            _root = root;

            _cubeMesh = MeshBuilder.Cube();
            _cubeMesh.Name = "PropCubeMesh";

            _sphereMesh = MeshBuilder.Sphere(12);
            _sphereMesh.Name = "PropSphereMesh";

            _boxShape = new BoxPhysicsShape();
            _boxShape.Name = "PropBoxShape";
            _boxShape.SetAABB(_cubeMesh.GetAABB());

            _sphereShape = new SpherePhysicsShape();
            _sphereShape.Name = "PropSphereShape";
            _sphereShape.SetSphere(new BoundingSphere(new Vec3f(0.0f, 0.0f, 0.0f), 1.0f));
        }

        public Entity Spawn(string name, PropShape shape, Vec3f position, Vec3f size, Color color, float mass = 0.0f, Quat4f? rotation = null, Vec3f? velocity = null, float roughness = 0.6f)
        {
            Mesh mesh = shape == PropShape.Box ? _cubeMesh : _sphereMesh;
            PhysicsShape physicsShape = shape == PropShape.Box ? _boxShape : _sphereShape;

            Entity entity = new Entity();
            entity.Name = name;
            entity.IsDynamic = mass > 0.0f;
            entity.LocalScale = shape == PropShape.Box ? size : new Vec3f(size.X);
            entity.LocalTranslation = position;

            if (rotation.HasValue)
            {
                entity.LocalRotation = rotation.Value;
            }

            _root.AddChild(entity);

            MeshComponent meshComponent = new MeshComponent();
            meshComponent.Mesh = mesh;
            meshComponent.Material = GetMaterial(color, roughness);
            meshComponent.EnableAutoInstancing = true;

            entity.AddComponent(ref meshComponent);

            entity.LocalBounds = mesh.GetAABB();

            RigidBodyComponent rigidBodyComponent = new RigidBodyComponent
            {
                PhysicsMaterial = new PhysicsMaterial { Mass = mass, Friction = 0.6f, Restitution = shape == PropShape.Sphere ? 0.35f : 0.05f },
                initialVelocity = velocity ?? new Vec3f(0.0f, 0.0f, 0.0f),
                Shape = new Handle<PhysicsShape>(physicsShape)
            };

            entity.AddComponent(ref rigidBodyComponent);

            return entity;
        }

        public Material GetMaterial(Color color, float roughness = 0.6f)
        {
            if (_materials.TryGetValue((color, roughness), out Material? material))
            {
                return material;
            }

            MaterialAttributes attributes = new MaterialAttributes();
            attributes.ShaderName = new Name("GeometryPass");
            attributes.Bucket = RenderBucket.Opaque;

            MaterialParameters parameters = new MaterialParameters();
            parameters.albedo = color;
            parameters.roughness = roughness;
            parameters.metalness = 0.0f;

            material = new Material();
            material.Name = $"PropMaterial_{_materials.Count}";
            material.SetAttributes(attributes);
            material.SetParameters(parameters);

            _materials[(color, roughness)] = material;

            return material;
        }
    }
}
