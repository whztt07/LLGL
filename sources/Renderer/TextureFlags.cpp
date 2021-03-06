/*
 * TextureFlags.cpp
 * 
 * This file is part of the "LLGL" project (Copyright (c) 2015 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#include <LLGL/TextureFlags.h>
#include <cmath>


namespace LLGL
{


LLGL_EXPORT unsigned int NumMipLevels(unsigned int width, unsigned int height, unsigned int depth)
{
    auto maxSize = std::max({ width, height, depth });
    auto log2Size = static_cast<unsigned int>(std::log2(maxSize));
    return (1 + log2Size);
}

LLGL_EXPORT bool IsCompressedFormat(const TextureFormat format)
{
    return (format >= TextureFormat::RGB_DXT1);
}

LLGL_EXPORT bool IsDepthStencilFormat(const TextureFormat format)
{
    return (format == TextureFormat::DepthComponent || format == TextureFormat::DepthStencil);
}

LLGL_EXPORT bool IsArrayTexture(const TextureType type)
{
    return (type >= TextureType::Texture1DArray && type != TextureType::Texture2DMS);
}

LLGL_EXPORT bool IsMultiSampleTexture(const TextureType type)
{
    return (type >= TextureType::Texture2DMS);
}


} // /namespace LLGL



// ================================================================================
