    {
        ktx_error_code_e r;

        const int imgSize = 128;
        const int imgCount = 128;

        std::vector<ktxTexture *> textures;

        for (int i = 0; i < imgCount; i++)
        {
            std::string path = "C:\\Git\\BlueNoiseGen\\Data\\128_KTX\\LDR_RGBA_" + std::to_string(i) + "_png_ARGB_8888.ktx";

            ktxTexture *t;

            r = ktxTexture_CreateFromNamedFile(path.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &t);
            assert(r == KTX_SUCCESS);

            assert(t->baseWidth == imgSize);
            assert(t->dataSize == imgSize * imgSize * 4);

            textures.push_back(t);
        }


        ktxTextureCreateInfo createInfo = {};
        createInfo.vkFormat = VK_FORMAT_R8G8B8A8_UNORM;
        createInfo.baseWidth = imgSize;
        createInfo.baseHeight = imgSize;
        createInfo.baseDepth = 1;
        createInfo.numDimensions = 2;
        createInfo.numLevels = 1;
        createInfo.numLayers = imgCount;
        createInfo.numFaces = 1;
        createInfo.isArray = 1;
        createInfo.generateMipmaps = 0;

        ktxTexture2 *target;
        r = ktxTexture2_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &target);
        assert(r == KTX_SUCCESS);

        for (int i = 0; i < imgCount; i++)
        {
            r = ktxTexture_SetImageFromMemory((ktxTexture *)target, 0, i, 0, textures[i]->pData, textures[i]->dataSize);
            assert(r == KTX_SUCCESS);
        }

        ktxTexture_WriteToNamedFile((ktxTexture *)target, "C:\\Git\\BlueNoiseGen\\Data\\BlueNoise_LDR_RGBA_128.ktx2");
        ktxTexture_Destroy((ktxTexture *)target);
    }
