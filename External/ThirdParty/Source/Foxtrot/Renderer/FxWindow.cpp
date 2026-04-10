#include "FxWindow.hpp"

#include <SDL3/SDL.h>

#include <Core/FxPanic.hpp>
#include <Core/FxRef.hpp>
#include <Core/FxTypes.hpp>


void FxWindow::Create(const char* title, const FxVec2i& size)
{
    mSize = size;

    if (size.X > 8000 || size.Y > 8000) {
        FxPanic("FxWindow", "Window size is too large! (Size: {})", size);
    }

    const uint64 window_flags = SDL_WINDOW_VULKAN;

    mWindow = SDL_CreateWindow(title, static_cast<int32>(size.X), static_cast<int32>(size.Y), window_flags);


    if (mWindow == nullptr) {
        FxPanic("Window", "Could not create SDL window (SDL err: {})", SDL_GetError());
    }
}

FxVec2i FxWindow::GetSize()
{
    bool success = SDL_GetWindowSize(mWindow, &mSize.X, &mSize.Y);
    if (!success) {
        FxLogError("Error retrieving window size from SDL! (SDL err: {})", SDL_GetError());
        return mSize;
    }

    return mSize;
}

FxWindow::~FxWindow() { SDL_DestroyWindow(mWindow); }
