#pragma once

enum class AxPathQuery
{
    eShaders,
    eDataPacks,
};


const char* FxAssetPath(AxPathQuery query);
