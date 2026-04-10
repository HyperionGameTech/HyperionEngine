#pragma once

#include "AxBase.hpp"

#include <Core/FxRef.hpp>
#include <Renderer/Backend/RxImage.hpp>

class AxImage : public AxBase
{
protected:
public:
    friend class AxManager;

    AxImage() = default;
    AxImage(const AxImage& other);

    static FxPagedArray<FxTSRef<AxImage>>& GetEmptyImagesArray();

    template <RxImageFormat TFormat>
    static FxTSRef<AxImage> GetEmptyImage()
    {
        // Stashed ptr for the image
        static FxTSRef<AxImage> spEmptyImage { nullptr };

        if (spEmptyImage) {
            return spEmptyImage;
        }

        FxPagedArray<FxTSRef<AxImage>>& empty_images = GetEmptyImagesArray();

        if (!empty_images.IsInited()) {
            empty_images.Create(10);
        }

        constexpr uint32 pixel_size = RxImageFormatUtil::GetSize(TFormat);

        FxSizedArray<uint8> image_data(pixel_size);
        memset(image_data.pData, 1, pixel_size);
        image_data.MarkFull();

        spEmptyImage = FxTSRef<AxImage>::New();
        spEmptyImage->Image.CreateGpuOnly(RxImageType::e2d, FxVec2u(1, 1), TFormat, image_data);
        spEmptyImage->MarkAndSignalLoaded();

        empty_images.Insert(spEmptyImage);

        return spEmptyImage;
    }

    AxImage& operator=(const AxImage& other);

    /**
     * Signal to any assets that are waiting that this asset is now loaded and ready to be used.
     */
    void MarkAndSignalLoaded();

    ~AxImage() override { Destroy(); }

    // private:
    void Destroy() override;

public:
    RxImage Image {};

    RxImageType ImageType = RxImageType::e2d;
    FxVec2u Size = FxVec2u::sZero;
};
