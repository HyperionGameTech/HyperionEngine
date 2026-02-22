@echo on
echo "This will install packages using vcpkg. Please make sure it is installed and in your PATH then press any key to continue."
PAUSE
vcpkg install openal-soft :x64-windows
vcpkg install Vulkan :x64-windows
vcpkg install sdl2[vulkan]:x64-windows
