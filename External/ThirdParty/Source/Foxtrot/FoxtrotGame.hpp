#pragma once

#include "FxObjectManager.hpp"

#include <Asset/FxConfigFile.hpp>
#include <FxObject.hpp>
#include <FxPlayer.hpp>
#include <FxScene.hpp>

class RxShadowDirectional;

class FoxtrotGame
{
public:
    FoxtrotGame();

    void CreateGame();


    ~FoxtrotGame();

private:
    void InitEngine();
    void CreateLights();

    void Tick();
    void ProcessControls();


    void DestroyGame();


public:
    FxPlayer Player {};

    FxRef<FxLightDirectional> pSun { nullptr };

    // TODO: Player attachment system
    FxTSRef<FxObject> pPistolObject { nullptr };
    FxTSRef<FxObject> pHelmetObject { nullptr };

    double FrameTimeAvg = 0.0f;


    FxQuat PistolRotationGoal = FxQuat::sIdentity;

    double DeltaTime = 1.0f / 60.0f;

    FxObjectManager ObjectManager;

private:
    uint64 mLastTick = 0;
    FxScene mMainScene {};

    FxConfigFile Config;
};
