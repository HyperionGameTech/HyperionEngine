using System;
using System.Collections.Generic;
using Hyperion;

namespace Hyperion.Samples
{
    public class SampleGame : Game
    {
        private const int MaxBalls = 40;
        private const float BallRadius = 0.18f;
        private const float BallSpeed = 22.0f;
        private const float KillHeight = -30.0f;

        private static readonly Vec3f TowerPosition = new Vec3f(0.0f, 0.0f, -8.0f);
        private const float TowerPlatformHeight = 1.5f;
        private const float CrateSize = 0.7f;

        private static readonly Color[] CrateColors =
        {
            new Color(0.75f, 0.12f, 0.08f, 1.0f),
            new Color(0.9f, 0.45f, 0.02f, 1.0f),
            new Color(0.06f, 0.3f, 0.75f, 1.0f),
            new Color(0.12f, 0.55f, 0.15f, 1.0f)
        };

        private Props? _props;
        private Player? _player;
        private InputManager? _input;
        private ConsoleOverlay? _console;

        private readonly List<Entity> _towerCrates = new List<Entity>();
        private readonly Queue<Entity> _balls = new Queue<Entity>();
        private readonly List<Entity> _droppedCrates = new List<Entity>();

        private readonly HashSet<KeyCode> _keysDown = new HashSet<KeyCode>();
        private bool _throwButtonWasDown;

        private int _cratesKnockedOff;
        private bool _won;
        private float _throwCooldown;
        private readonly Random _random = new Random();

        protected override void OnLaunch()
        {
            World world = World ?? throw new InvalidOperationException("Game has no World");

            Scene scene = new Scene();
            scene.Name = "PlaygroundScene";
            scene.SceneFlags = SceneFlags.Default & ~SceneFlags.Streamed;
            world.AddScene(scene, false);

            Node root = scene.RootNode ?? throw new InvalidOperationException("Scene has no root node");

            DirectionalLight sun = new DirectionalLight();
            sun.Name = "Sun";
            sun.Direction = new Vec3f(0.35f, 0.8f, 0.25f).Normalize();
            sun.Color = new Color(1.0f, 0.93f, 0.85f, 1.0f);
            sun.Intensity = 10.0f;
            root.AddChild(sun);

            world.AddSystem(new DynamicSkySystem());

            _props = new Props(root);

            BuildLevel();
            BuildTower();

            _player = new Player(root, new Vec3f(0.0f, 0.0f, 6.0f), cameraYawDegrees: 180.0f);

            _input = AppContextBase.Instance.GetMainWindow()?.GetInputManager();

            _console = new ConsoleOverlay();
            UISubsystem?.AddDebugOverlay(_console);

            this.StartSimulating();

            Logger.Log(LogLevel.Info, "Playground ready. WASD/Space/Shift to move, click to capture the mouse, left click or F to throw, G to drop a crate, R to rebuild the tower, ~ for the console");
        }

        protected override void OnUpdate(float delta)
        {
            if (_props == null || _player == null)
            {
                return;
            }

            _throwCooldown = MathF.Max(0.0f, _throwCooldown - delta);

            HandleInput();
            UpdateTowerScore();
            RemoveFallenProps();
        }

        protected override void OnShutdown()
        {
            Logger.Log(LogLevel.Info, "Playground shutting down");
        }

        private void BuildLevel()
        {
            Props props = _props!;

            Color groundColor = new Color(0.32f, 0.34f, 0.36f, 1.0f);
            Color stoneColor = new Color(0.22f, 0.24f, 0.28f, 1.0f);
            Color trimColor = new Color(0.12f, 0.13f, 0.16f, 1.0f);

            // ground slab with its top face at y = 0
            props.Spawn("Ground", PropShape.Box, new Vec3f(0.0f, -0.5f, 0.0f), new Vec3f(80.0f, 1.0f, 80.0f), groundColor, roughness: 0.9f);

            // tower platform
            props.Spawn("TowerPlatform", PropShape.Box,
                TowerPosition + new Vec3f(0.0f, TowerPlatformHeight * 0.5f, 0.0f),
                new Vec3f(4.0f, TowerPlatformHeight, 4.0f), trimColor, roughness: 0.7f);

            // a ring of pillars around the arena
            for (int i = 0; i < 10; i++)
            {
                float angle = i / 10.0f * MathF.PI * 2.0f;
                Vec3f position = new Vec3f(MathF.Cos(angle) * 18.0f, 2.0f, MathF.Sin(angle) * 18.0f);

                props.Spawn($"Pillar{i}", PropShape.Box, position, new Vec3f(1.2f, 4.0f, 1.2f), stoneColor, roughness: 0.8f);
            }

            // loose physics toys scattered around
            for (int i = 0; i < 6; i++)
            {
                float angle = i / 6.0f * MathF.PI * 2.0f + 0.4f;
                Vec3f position = new Vec3f(MathF.Cos(angle) * 9.0f, 0.5f, MathF.Sin(angle) * 9.0f);

                props.Spawn($"Ball{i}", PropShape.Sphere, position, new Vec3f(0.5f), CrateColors[i % CrateColors.Length], mass: 8.0f, roughness: 0.3f);
            }
        }

        /// A pyramid of crates on the platform
        private void BuildTower()
        {
            Props props = _props!;

            foreach (Entity crate in _towerCrates)
            {
                RemoveProp(crate);
            }

            _towerCrates.Clear();
            _cratesKnockedOff = 0;
            _won = false;

            const int baseWidth = 4;
            float spacing = CrateSize + 0.02f;

            for (int layer = 0; layer < baseWidth; layer++)
            {
                int width = baseWidth - layer;
                float offset = (width - 1) * spacing * 0.5f;

                for (int x = 0; x < width; x++)
                {
                    for (int z = 0; z < width; z++)
                    {
                        Vec3f position = TowerPosition + new Vec3f(
                            x * spacing - offset,
                            TowerPlatformHeight + CrateSize * 0.5f + layer * CrateSize + 0.01f,
                            z * spacing - offset);

                        Color color = CrateColors[(x + z + layer) % CrateColors.Length];

                        _towerCrates.Add(props.Spawn($"Crate{_towerCrates.Count}", PropShape.Box, position, new Vec3f(CrateSize), color, mass: 4.0f));
                    }
                }
            }

            Logger.Log(LogLevel.Info, "Tower built: knock all {0} crates off the platform!", _towerCrates.Count);
        }

        private void HandleInput()
        {
            if (_input == null)
            {
                return;
            }

            // keys typed into the console shouldn't trigger game actions
            if (_console != null && _console.IsOpen())
            {
                _keysDown.Clear();
                _throwButtonWasDown = true;

                return;
            }

            bool mouseCaptured = _input.IsMouseLocked();
            bool throwButtonDown = _input.IsButtonDown(MouseButtonKey.Left);

            // the click that captures the mouse shouldn't also throw
            if ((throwButtonDown && !_throwButtonWasDown && mouseCaptured) || WasKeyPressed(KeyCode.F))
            {
                ThrowBall();
            }

            _throwButtonWasDown = throwButtonDown;

            if (WasKeyPressed(KeyCode.G))
            {
                DropCrate();
            }

            if (WasKeyPressed(KeyCode.R))
            {
                BuildTower();
            }
        }

        private bool WasKeyPressed(KeyCode key)
        {
            bool isDown = _input!.IsKeyDown(key);
            bool wasDown = _keysDown.Contains(key);

            if (isDown)
            {
                _keysDown.Add(key);
            }
            else
            {
                _keysDown.Remove(key);
            }

            return isDown && !wasDown;
        }

        private void ThrowBall()
        {
            if (_throwCooldown > 0.0f)
            {
                return;
            }

            _throwCooldown = 0.15f;

            // throw from the player's chest along the camera's aim, with a little arc
            Vec3f direction = _player!.AimDirection.Normalize();
            Vec3f origin = _player.Position + new Vec3f(0.0f, -0.7f, 0.0f) + direction * 1.5f;
            Vec3f velocity = direction * BallSpeed + new Vec3f(0.0f, 5.0f, 0.0f);

            Color color = CrateColors[_random.Next(CrateColors.Length)];

            _balls.Enqueue(_props!.Spawn("ThrownBall", PropShape.Sphere, origin, new Vec3f(BallRadius), color, mass: 2.5f, velocity: velocity, roughness: 0.25f));

            while (_balls.Count > MaxBalls)
            {
                RemoveProp(_balls.Dequeue());
            }
        }

        private void DropCrate()
        {
            Vec3f offset = new Vec3f((float)_random.NextDouble() * 2.0f - 1.0f, 6.0f, (float)_random.NextDouble() * 2.0f - 1.0f);

            _droppedCrates.Add(_props!.Spawn("DroppedCrate", PropShape.Box, _player!.Position + offset, new Vec3f(1.0f),
                CrateColors[_random.Next(CrateColors.Length)], mass: 6.0f));
        }

        private void UpdateTowerScore()
        {
            if (_towerCrates.Count == 0)
            {
                return;
            }

            // a crate counts as knocked off once it's below the platform's top face
            int knockedOff = 0;

            foreach (Entity crate in _towerCrates)
            {
                if (crate.GetWorldTranslation().Y < TowerPlatformHeight - CrateSize)
                {
                    knockedOff++;
                }
            }

            if (knockedOff == _cratesKnockedOff)
            {
                return;
            }

            _cratesKnockedOff = knockedOff;

            Logger.Log(LogLevel.Info, "Crates knocked off: {0}/{1}", knockedOff, _towerCrates.Count);

            if (knockedOff == _towerCrates.Count && !_won)
            {
                _won = true;

                Logger.Log(LogLevel.Info, "Tower cleared! Press R to rebuild it");
            }
        }

        private void RemoveFallenProps()
        {
            if (_balls.Count > 0 && _balls.Peek().GetWorldTranslation().Y < KillHeight)
            {
                RemoveProp(_balls.Dequeue());
            }

            for (int i = _droppedCrates.Count - 1; i >= 0; i--)
            {
                if (_droppedCrates[i].GetWorldTranslation().Y < KillHeight)
                {
                    RemoveProp(_droppedCrates[i]);
                    _droppedCrates.RemoveAt(i);
                }
            }
        }

        private static void RemoveProp(Entity entity)
        {
            entity.Remove(false);
            entity.Dispose();
        }
    }

    public static class Program
    {
        public static int Main(string[] args) => HyperionApp.Run<SampleGame>(args);
    }
}
