# Hyperion Engine

## About

Hyperion Engine is a 3D game engine written in C++. We aim to make Hyperion be easy to understand and use, while still enabling the user to have ample control over the game engine.

Hyperion currently renders using OpenGL primarily, but plans are in place to add support for Vulkan as well as other rendering platforms (currently it runs on OpenGL). It is currently in the process of being ported over to Metal as well using [MGL](https://github.com/openglonmetal/MGL).

Feel free to contribute anything - We'd love to have some more eyes on this project! Submit an issue if you run into anything, as well.

## Screenshots

![Apex engine screenshot](/screenshots/screenshot17.PNG)
![Apex engine screenshot](/screenshots/screenshot11.PNG)
![Apex engine screenshot](/screenshots/screenshot7.png)

## Building

Hyperion uses CMake to build. We have added a couple shell scripts to make the building process a bit more convenient, although there really aren't too many steps.

If it is your first time building the engine, you can run the `setup.sh` script from the root directory of the project. This will create the `build` folder, as well as copy all needed resources over from the `res` folder.
> NOTE: Currently, assets are loaded from `build/res`, so if you make a change to any shaders, textures or models here, you'll have to copy them over to the root `res` folder before pushing a change, or else that change will not be reflected in your commit.

After that process has completed, `cd` into the `build` folder. From here, you're able to run `make` directly and then run the engine test with `./hyperion`. We also have a `build.sh` script,which when ran from the root project folder, will do this whole process for you.

Really, all you need to know is:
* Run `setup.sh` to regenerate the makefile and copy assets from `res` into `build/res`
* Run `build.sh` to build the engine
* Run `./hyperion` to run the engine test
