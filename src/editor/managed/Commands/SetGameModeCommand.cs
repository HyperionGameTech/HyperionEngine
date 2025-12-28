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

        public void Execute(object? parameter)
        {
            if (Interlocked.CompareExchange(ref _isChangingGameMode, 1, 0) != 0)
            {
                Logger.Log(LogType.Warn, "Cannot set game mode; already setting");
                return;
            }

            Game? currentGameInstance = EngineManager.GameInstance;
            Debug.Assert(currentGameInstance != null);

            Game? gameInstance = currentGameInstance;

            switch (_mode)
            {
                case GameStateMode.Simulating:
                {
                    if (currentGameInstance is HyperionEditorGame hyperionEditorGame)
                    {
                        gameInstance = hyperionEditorGame.EditorSubsystem?.CurrentProject?.GameInstance;
                        Debug.Assert(gameInstance != null, "Failed to get game instance from current project");
                    }
                    else
                    {
                        throw new InvalidOperationException("Cannot enter Simulating mode when game instance is not HyperionEditorGame");
                    }

                    EngineManager.InitializeGame(gameInstance);

                    _ = EngineManager.PostToSimThread(() =>
                    {
                        try
                        {
                            gameInstance.StartSimulating();
                        }
                        finally
                        {
                            Interlocked.Exchange(ref _isChangingGameMode, 0);
                        }
                    });

                    break;
                }
                case GameStateMode.Paused:
                    _ = EngineManager.PostToSimThread(() =>
                    {
                        try
                        {
                            gameInstance.PauseSimulation();
                        }
                        finally
                        {
                            Interlocked.Exchange(ref _isChangingGameMode, 0);
                        }
                    });

                    break;
                case GameStateMode.Stopped:
                    _ = EngineManager.PostToSimThread(() =>
                    {
                        gameInstance.StopSimulating();

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
                    throw new NotImplementedException();
            }
        }

        public event EventHandler? CanExecuteChanged;

        public void RaiseCanExecuteChanged() => CanExecuteChanged?.Invoke(this, EventArgs.Empty);
    }
}