#pragma once

#include <Core/FxRef.hpp>
#include <Math/FxVec2.hpp>

struct SDL_Window;

class FxWindow
{
public:
    static FxRef<FxWindow> New(const char* title, const FxVec2i& size) { return FxRef<FxWindow>::New(title, size); }

public:
    FxWindow() = default;
    FxWindow(const FxWindow& other) = delete;
    FxWindow(const char* title, const FxVec2i& size) { Create(title, size); }

    void Create(const char* title, const FxVec2i& size);

    FxVec2i GetSize();

    float32 GetAspectRatio()
    {
        FxVec2i size = GetSize();
        return static_cast<float32>(size.X) / (size.Y);
    }

    SDL_Window* GetWindow() { return mWindow; }

    ~FxWindow();

public:
private:
    FxVec2i mSize = FxVec2i::sZero;
    SDL_Window* mWindow;
};
