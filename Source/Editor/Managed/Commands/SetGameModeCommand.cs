using System;
using System.Diagnostics;
using System.Threading;
using System.Windows.Input;
using Avalonia.Threading;
using Hyperion;

namespace Hyperion.Editor.Commands
{

    public class SetGameModeCommand : ICommand
    {
        private static int _isChangingGameMode = 0;
        private GameStateMode _mode;

        public SetGameModeCommand(GameStateMode mode)
        {
            _mode = mode;
        }

        public bool CanExecute(object? parameter) => true; // TEMP : debug
                                                           // EngineManager.GameInstance?.World?.GetGameState().Mode != _mode
                                                           //     && Interlocked.CompareExchange(ref _isChangingGameMode, 0, 0) == 0;

        public async void Execute(object? parameter)
        {
            Dispatcher.UIThread.VerifyAccess();

            if (Interlocked.CompareExchange(ref _isChangingGameMode, 1, 0) != 0)
            {
                Logger.Log(LogLevel.Warning, "Cannot set game mode; already setting");
                return;
            }

            try
            {
                Game? currentGameInstance = EngineManager.GameInstance;
                Debug.Assert(currentGameInstance != null);

                Game? gameInstance = currentGameInstance;

                switch (_mode)
                {
                    case GameStateMode.Simulating:
                        {
                            await EngineManager.PostToSimThread(() =>
                            {
                                Game? innerGameInstance = null;

                                try
                                {
                                    EditorSubsystem? editorSubsystem = EngineManager.EditorGame?.EditorSubsystem;
                                    Debug.Assert(editorSubsystem != null, "EditorSubsystem is null");

                                    if (!editorSubsystem.StartSimulation())
                                    {
                                        throw new Exception("StartSimulation() returned false");
                                    }

                                    innerGameInstance = editorSubsystem.CurrentProject?.GameInstance;
                                    Debug.Assert(innerGameInstance != null);
                                }
                                catch (Exception ex)
                                {
                                    Logger.Log(LogLevel.Error, $"Failed to start simulation: {ex.Message}");

                                    Interlocked.Exchange(ref _isChangingGameMode, 0);
                                    return;
                                }

                                Dispatcher.UIThread.Post(() =>
                                {
                                    try
                                    {
                                        EngineManager.InitializeGame(innerGameInstance);
                                    } finally
                                    {
                                        Interlocked.Exchange(ref _isChangingGameMode, 0);
                                    }
                                });
                            });

                            break;
                        }
                    case GameStateMode.Paused:
                        await EngineManager.PostToSimThread(() =>
                        {
                            try
                            {
                                EditorSubsystem? editorSubsystem = EngineManager.EditorGame?.EditorSubsystem;
                                Debug.Assert(editorSubsystem != null, "EditorSubsystem is null");

                                if (!editorSubsystem.PauseSimulation())
                                {
                                    throw new Exception("Failed to pause simulation!");
                                }
                            }
                            finally
                            {
                                Interlocked.Exchange(ref _isChangingGameMode, 0);
                            }
                        });

                        break;
                    case GameStateMode.Stopped:
                        await EngineManager.PostToSimThread(() =>
                        {
                            EditorProject? simulationProject;

                            // We need to keep these separate as StopSimulation() will mutate the world
                            World? simulationWorld;
                            List<IDisposable> deferredDisposeObjects = [];

                            try
                            {
                                EditorSubsystem? editorSubsystem = EngineManager.EditorGame?.EditorSubsystem;
                                Debug.Assert(editorSubsystem != null, "EditorSubsystem is null");

                                simulationProject = editorSubsystem.CurrentProject;

                                Debug.Assert(simulationProject != null);

                                Game? gameInstance = simulationProject.GameInstance;
                                Debug.Assert(gameInstance != null);

                                simulationWorld = gameInstance.World;

                                if (simulationWorld != null)
                                {
                                    // we need to defer this shit because StopSimulation() will Shutdown() each scene.
                                    Action<Node>? addNodeRecur = null;
                                    addNodeRecur = (Node n) =>
                                    {
                                        deferredDisposeObjects.Add(n);
                                        foreach (Node? child in n.Children)
                                        {
                                            addNodeRecur!(child!);
                                        }
                                    };

                                    foreach (Scene? scene in simulationWorld.Scenes)
                                    {
                                        addNodeRecur(scene!.RootNode!);

                                        // ! Must be before root node !
                                        // Or else the emgr gets all shitfucked.
                                        deferredDisposeObjects.Add(scene!);
                                    }

                                    deferredDisposeObjects.Add(simulationWorld);
                                    deferredDisposeObjects.Add(gameInstance);
                                }

                                if (!editorSubsystem.StopSimulation())
                                {
                                    throw new Exception("Failed to stop simulating!");
                                }

                                foreach (IDisposable o in deferredDisposeObjects)
                                {
                                    o.Dispose();
                                }

                                deferredDisposeObjects.Clear();

                                // To force collection of dependent objects eg Mesh,Texture,Material etc.
                                GC.Collect(0, GCCollectionMode.Forced, blocking: true, compacting: true);
                            }
                            catch (Exception)
                            {
                                Interlocked.Exchange(ref _isChangingGameMode, 0);
                                throw;
                            }

                            Dispatcher.UIThread.Post(() =>
                            {
                                try
                                {
                                    EngineManager.InitializeEditor();
                                }
                                finally
                                {
                                    Interlocked.Exchange(ref _isChangingGameMode, 0);
                                }
                            });
                        });

                        break;
                    default:
                        Interlocked.Exchange(ref _isChangingGameMode, 0);
                        throw new NotImplementedException();
                }
            }
            catch (Exception)
            {
                Interlocked.Exchange(ref _isChangingGameMode, 0);
                throw;
            }
        }

        public event EventHandler? CanExecuteChanged;

        public void RaiseCanExecuteChanged() => CanExecuteChanged?.Invoke(this, EventArgs.Empty);
    }
}