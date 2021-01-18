#include "ImageLoader.h"

#include <cassert>

#include "Stb/stb_image.h"

ImageLoader::~ImageLoader()
{
    assert(loadedImages.empty());
}

const uint32_t *ImageLoader::LoadRGBA8(const char *path, uint32_t *outWidth, uint32_t *outHeight)
{
    int width, height, channels;
    uint8_t *img = stbi_load(path, &width, &height, &channels, 4);

    if (img == nullptr)
    {
        return nullptr;
    }

    *outWidth = width;
    *outHeight = height;

    loadedImages.push_back(static_cast<void*>(img));

    return reinterpret_cast<const uint32_t*>(img);
}

void ImageLoader::FreeLoaded()
{
    for (auto *p : loadedImages)
    {
        stbi_image_free(p);
    }
}
