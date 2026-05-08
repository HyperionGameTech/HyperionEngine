# Commandlets and console variables (CVars)

*Commandlets* - are isolated tasks that can be run:
- from within the editor's console window by typing the name of the commandlet and pressing enter (args can be provided after the name of the commandlet, separated by spaces),
- by invoking them via cli by running the engine's executable with the `--exec` argument followed by the name of the commandlet you want to run and any args you want to provide (e.g. `HyperionEngine.exe --exec MyCommandlet arg1 arg2`).
- or as a specifically compiled executable, as long as `add_commandlet_target()` is used in the Source/Commandlets/CMakeLists.txt. For example, `PrecompileShaders` is one of these, and can be run by executing `PrecompileShaders.exe` directly from the command line.

> To find all commandlets, do a project-wide search for `: public CommandletBase`. That will show you all defined console command classes.

When Commandlets are executed outside of the editor, the engine will be initialized in a headless mode, load config and initialize the asset registry as well as any other necessary subsystems, etc.
This allows commandlets to perform a wide variety of tasks within an engine context, such as cooking assets, precompiling shaders, building lighting data, etc.

*Console variables (CVars)* - are global variables that can be set to modify the behavior of the engine. They can be of various types, such as `bool`, `int`, `float`, `String`, etc. CVars can be defined in code and registered with the engine, allowing them to be accessed and modified at runtime. Most CVars are initialized from the engine config file (Config/EngineConfig.json) on engine startup, but they can also be modified at runtime via the console or programmatically via code.

From within the editor, you can use the console window to set a CVar by name. The name you pass is case-insensitive and can be either the full name of the CVar (e.g. `Rendering.SSGI`) or just the last part of the name (e.g. `SSGI`), followed by the value you want to set it to (e.g. `true`/`1` or `false`/`0` for bool CVars, a number for int or float CVars, etc.). For example, you can type `ssgi 1` in the console to enable SSGI at runtime.

> For a full list of available CVars, do a project-wide regex search for `^CVar<([A-Za-z_]+)>` to find all CVars defined in the codebase.
