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

                            try
                            {
                                EditorSubsystem? editorSubsystem = EngineManager.EditorGame?.EditorSubsystem;
                                Debug.Assert(editorSubsystem != null, "EditorSubsystem is null");

                                simulationProject = editorSubsystem.CurrentProject;

                                if (!editorSubsystem.StopSimulation())
                                {
                                    throw new Exception("Failed to stop simulating!");
                                }
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

                                    // Release resources the project from the simulation state. We need to do this on simulation thread,
                                    // as the InitializeEditor() call sets GameInstance but it may not have propagated to the sim thread yet
                                    // via LaunchGameAsync.

                                    if (simulationProject != null)
                                    {
                                        _ = EngineManager.PostToSimThread(() =>
                                        {
                                            //simulationProject.World?.Dispose();
                                            simulationProject.Dispose();
                                        });
                                    }
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