#pragma once

#include <cstdint>
#include <vector>

// Loading images from files.
class ImageLoader
{
public:
    ImageLoader() = default;
    ~ImageLoader();

    ImageLoader(const ImageLoader &other) = delete;
    ImageLoader(ImageLoader &&other) noexcept = delete;
    ImageLoader &operator=(const ImageLoader &other) = delete;
    ImageLoader &operator=(ImageLoader &&other) noexcept = delete;

    // Read the file and get array of R8G8B8A8 values.
    // Returns null if image wasn't loaded.
    const uint32_t *LoadRGBA8(const char *path, uint32_t *outWidth, uint32_t *outHeight);
    // Must be called after using the loaded data to free the allocated memory
    void FreeLoaded();

private:
    std::vector<void*> loadedImages;
};