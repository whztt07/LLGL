/*
 * DXCore.cpp
 * 
 * This file is part of the "LLGL" project (Copyright (c) 2015 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#include "DXCore.h"
#include "ComPtr.h"
#include "../../Core/Helper.h"
#include "../../Core/HelperMacros.h"
#include "../../Core/Vendor.h"
#include <LLGL/Shader.h>
#include <stdexcept>
#include <algorithm>
#include <d3dcompiler.h>


namespace LLGL
{


std::string DXErrorToStr(const HRESULT errorCode)
{
    switch (errorCode)
    {
        // see https://msdn.microsoft.com/en-us/library/windows/desktop/aa378137(v=vs.85).aspx
        LLGL_CASE_TO_STR( S_OK );
        LLGL_CASE_TO_STR( S_FALSE );
        LLGL_CASE_TO_STR( E_ABORT );
        LLGL_CASE_TO_STR( E_ACCESSDENIED );
        LLGL_CASE_TO_STR( E_FAIL );
        LLGL_CASE_TO_STR( E_HANDLE );
        LLGL_CASE_TO_STR( E_INVALIDARG );
        LLGL_CASE_TO_STR( E_NOINTERFACE );
        LLGL_CASE_TO_STR( E_NOTIMPL );
        LLGL_CASE_TO_STR( E_OUTOFMEMORY );
        LLGL_CASE_TO_STR( E_POINTER );
        LLGL_CASE_TO_STR( E_UNEXPECTED );

        // see https://msdn.microsoft.com/en-us/library/windows/desktop/bb509553(v=vs.85).aspx
        LLGL_CASE_TO_STR( DXGI_ERROR_DEVICE_HUNG );
        LLGL_CASE_TO_STR( DXGI_ERROR_DEVICE_REMOVED );
        LLGL_CASE_TO_STR( DXGI_ERROR_DEVICE_RESET );
        LLGL_CASE_TO_STR( DXGI_ERROR_DRIVER_INTERNAL_ERROR );
        LLGL_CASE_TO_STR( DXGI_ERROR_FRAME_STATISTICS_DISJOINT );
        LLGL_CASE_TO_STR( DXGI_ERROR_GRAPHICS_VIDPN_SOURCE_IN_USE );
        LLGL_CASE_TO_STR( DXGI_ERROR_INVALID_CALL );
        LLGL_CASE_TO_STR( DXGI_ERROR_MORE_DATA );
        LLGL_CASE_TO_STR( DXGI_ERROR_NONEXCLUSIVE );
        LLGL_CASE_TO_STR( DXGI_ERROR_NOT_CURRENTLY_AVAILABLE );
        LLGL_CASE_TO_STR( DXGI_ERROR_NOT_FOUND );
        LLGL_CASE_TO_STR( DXGI_ERROR_REMOTE_CLIENT_DISCONNECTED );
        LLGL_CASE_TO_STR( DXGI_ERROR_REMOTE_OUTOFMEMORY );
        LLGL_CASE_TO_STR( DXGI_ERROR_WAS_STILL_DRAWING );
        LLGL_CASE_TO_STR( DXGI_ERROR_UNSUPPORTED );
        LLGL_CASE_TO_STR( DXGI_ERROR_ACCESS_LOST );
        LLGL_CASE_TO_STR( DXGI_ERROR_WAIT_TIMEOUT );
        LLGL_CASE_TO_STR( DXGI_ERROR_SESSION_DISCONNECTED );
        LLGL_CASE_TO_STR( DXGI_ERROR_RESTRICT_TO_OUTPUT_STALE );
        LLGL_CASE_TO_STR( DXGI_ERROR_CANNOT_PROTECT_CONTENT );
        LLGL_CASE_TO_STR( DXGI_ERROR_ACCESS_DENIED );
        LLGL_CASE_TO_STR( DXGI_ERROR_NAME_ALREADY_EXISTS );
        LLGL_CASE_TO_STR( DXGI_ERROR_SDK_COMPONENT_MISSING );

        // see https://msdn.microsoft.com/en-us/library/windows/desktop/ff476174(v=vs.85).aspx
        LLGL_CASE_TO_STR( D3D10_ERROR_FILE_NOT_FOUND );
        LLGL_CASE_TO_STR( D3D10_ERROR_TOO_MANY_UNIQUE_STATE_OBJECTS );
        LLGL_CASE_TO_STR( D3D11_ERROR_FILE_NOT_FOUND );
        LLGL_CASE_TO_STR( D3D11_ERROR_TOO_MANY_UNIQUE_STATE_OBJECTS );
        LLGL_CASE_TO_STR( D3D11_ERROR_TOO_MANY_UNIQUE_VIEW_OBJECTS );
        LLGL_CASE_TO_STR( D3D11_ERROR_DEFERRED_CONTEXT_MAP_WITHOUT_INITIAL_DISCARD );

        #ifdef LLGL_DX_ENABLE_D3D12
        LLGL_CASE_TO_STR( D3D12_ERROR_ADAPTER_NOT_FOUND );
        LLGL_CASE_TO_STR( D3D12_ERROR_DRIVER_VERSION_MISMATCH );
        #endif
    }
    return ToHex(errorCode);
}

void DXThrowIfFailed(const HRESULT errorCode, const std::string& info)
{
    if (FAILED(errorCode))
        throw std::runtime_error(info + " (error code = " + DXErrorToStr(errorCode) + ")");
}

template <typename Cont>
Cont GetBlobDataTmpl(ID3DBlob* blob)
{
    auto data = reinterpret_cast<const char*>(blob->GetBufferPointer());
    auto size = static_cast<std::size_t>(blob->GetBufferSize());

    Cont container;
    {
        container.resize(size);
        std::copy(data, data + size, &container[0]);
    }
    return container;
}

std::string DXGetBlobString(ID3DBlob* blob)
{
    return GetBlobDataTmpl<std::string>(blob);
}

std::vector<char> DXGetBlobData(ID3DBlob* blob)
{
    return GetBlobDataTmpl<std::vector<char>>(blob);
}

static int GetMaxTextureDimension(D3D_FEATURE_LEVEL featureLevel)
{
    if (featureLevel >= D3D_FEATURE_LEVEL_11_0) return 16384;
    if (featureLevel >= D3D_FEATURE_LEVEL_10_0) return 8192;
    if (featureLevel >= D3D_FEATURE_LEVEL_9_3 ) return 4096;
    else                                        return 2048;
}

static int GetMaxCubeTextureDimension(D3D_FEATURE_LEVEL featureLevel)
{
    if (featureLevel >= D3D_FEATURE_LEVEL_11_0) return 16384;
    if (featureLevel >= D3D_FEATURE_LEVEL_10_0) return 8192;
    if (featureLevel >= D3D_FEATURE_LEVEL_9_3 ) return 4096;
    else                                        return 512;
}

static unsigned int GetMaxRenderTargets(D3D_FEATURE_LEVEL featureLevel)
{
    if (featureLevel >= D3D_FEATURE_LEVEL_10_0) return 8;
    if (featureLevel >= D3D_FEATURE_LEVEL_9_3 ) return 4;
    else                                        return 1;
}

// Returns the HLSL version for the specified Direct3D feature level.
static ShadingLanguage DXGetHLSLVersion(D3D_FEATURE_LEVEL featureLevel)
{
    #ifdef LLGL_DX_ENABLE_D3D12
    if (featureLevel >= D3D_FEATURE_LEVEL_12_0) return ShadingLanguage::HLSL_5_1;
    #endif
    if (featureLevel >= D3D_FEATURE_LEVEL_11_0) return ShadingLanguage::HLSL_5_0;
    if (featureLevel >= D3D_FEATURE_LEVEL_10_1) return ShadingLanguage::HLSL_4_1;
    if (featureLevel >= D3D_FEATURE_LEVEL_10_0) return ShadingLanguage::HLSL_4_0;
    if (featureLevel >= D3D_FEATURE_LEVEL_9_3 ) return ShadingLanguage::HLSL_3_0;
    if (featureLevel >= D3D_FEATURE_LEVEL_9_2 ) return ShadingLanguage::HLSL_2_0b;
    else                                        return ShadingLanguage::HLSL_2_0a;
}

// see https://msdn.microsoft.com/en-us/library/windows/desktop/ff476876(v=vs.85).aspx
void DXGetRenderingCaps(RenderingCaps& caps, D3D_FEATURE_LEVEL featureLevel)
{
    unsigned int maxThreadGroups = 65535;//D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;

    caps.screenOrigin                   = ScreenOrigin::UpperLeft;
    caps.clippingRange                  = ClippingRange::ZeroToOne;
    caps.shadingLanguage                = DXGetHLSLVersion(featureLevel);
    caps.hasRenderTargets               = true;
    caps.has3DTextures                  = true;
    caps.hasCubeTextures                = true;
    caps.hasTextureArrays               = (featureLevel >= D3D_FEATURE_LEVEL_10_0);
    caps.hasCubeTextureArrays           = (featureLevel >= D3D_FEATURE_LEVEL_10_1);
    caps.hasMultiSampleTextures         = (featureLevel >= D3D_FEATURE_LEVEL_10_0);
    caps.hasSamplers                    = (featureLevel >= D3D_FEATURE_LEVEL_9_3);
    caps.hasConstantBuffers             = true;
    caps.hasStorageBuffers              = true;
    caps.hasUniforms                    = false;
    caps.hasGeometryShaders             = (featureLevel >= D3D_FEATURE_LEVEL_10_0);
    caps.hasTessellationShaders         = (featureLevel >= D3D_FEATURE_LEVEL_11_0);
    caps.hasComputeShaders              = (featureLevel >= D3D_FEATURE_LEVEL_10_0);
    caps.hasInstancing                  = (featureLevel >= D3D_FEATURE_LEVEL_9_3);
    caps.hasOffsetInstancing            = (featureLevel >= D3D_FEATURE_LEVEL_9_3);
    caps.hasViewportArrays              = true;
    caps.hasConservativeRasterization   = (featureLevel >= D3D_FEATURE_LEVEL_11_1);
    caps.hasStreamOutputs               = (featureLevel >= D3D_FEATURE_LEVEL_10_0);
    caps.maxNumTextureArrayLayers       = (featureLevel >= D3D_FEATURE_LEVEL_10_0 ? 2048 : 256);
    caps.maxNumRenderTargetAttachments  = GetMaxRenderTargets(featureLevel);
    caps.maxConstantBufferSize          = 16384;
    caps.maxPatchVertices               = 32;
    caps.max1DTextureSize               = GetMaxTextureDimension(featureLevel);
    caps.max2DTextureSize               = GetMaxTextureDimension(featureLevel);
    caps.max3DTextureSize               = (featureLevel >= D3D_FEATURE_LEVEL_10_0 ? 2048 : 256);
    caps.maxCubeTextureSize             = GetMaxCubeTextureDimension(featureLevel);
    caps.maxAnisotropy                  = (featureLevel >= D3D_FEATURE_LEVEL_9_2 ? 16 : 2);
    caps.maxNumComputeShaderWorkGroups  = { maxThreadGroups, maxThreadGroups, (featureLevel >= D3D_FEATURE_LEVEL_11_0 ? maxThreadGroups : 1u) };
    caps.maxComputeShaderWorkGroupSize  = { 1024, 1024, 1024 };
}

std::vector<D3D_FEATURE_LEVEL> DXGetFeatureLevels(D3D_FEATURE_LEVEL maxFeatureLevel)
{
    std::vector<D3D_FEATURE_LEVEL> featureLeves =
    {
        #ifdef LLGL_DX_ENABLE_D3D12
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        #endif
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1,
    };

    auto it = std::remove_if(
        featureLeves.begin(), featureLeves.end(),
        [maxFeatureLevel](D3D_FEATURE_LEVEL entry)
        {
            return (entry > maxFeatureLevel);
        }
    );
    featureLeves.erase(it, featureLeves.end());

    return featureLeves;
}

std::string DXFeatureLevelToVersion(D3D_FEATURE_LEVEL featureLevel)
{
    switch (featureLevel)
    {
        #ifdef LLGL_DX_ENABLE_D3D12
        case D3D_FEATURE_LEVEL_12_1:    return "12.1";
        case D3D_FEATURE_LEVEL_12_0:    return "12.0";
        #endif
        case D3D_FEATURE_LEVEL_11_1:    return "11.1";
        case D3D_FEATURE_LEVEL_11_0:    return "11.0";
        case D3D_FEATURE_LEVEL_10_1:    return "10.1";
        case D3D_FEATURE_LEVEL_10_0:    return "10.0";
        case D3D_FEATURE_LEVEL_9_3:     return "9.3";
        case D3D_FEATURE_LEVEL_9_2:     return "9.2";
        case D3D_FEATURE_LEVEL_9_1:     return "9.1";
    }
    return "";
}

std::string DXFeatureLevelToShaderModel(D3D_FEATURE_LEVEL featureLevel)
{
    switch (featureLevel)
    {
        #ifdef LLGL_DX_ENABLE_D3D12
        case D3D_FEATURE_LEVEL_12_1:    /*pass*/
        case D3D_FEATURE_LEVEL_12_0:    /*pass*/
        #endif
        case D3D_FEATURE_LEVEL_11_1:    /*pass*/
        case D3D_FEATURE_LEVEL_11_0:    return "5.0";
        case D3D_FEATURE_LEVEL_10_1:    return "4.1";
        case D3D_FEATURE_LEVEL_10_0:    return "4.0";
        case D3D_FEATURE_LEVEL_9_3:     return "3.0";
        case D3D_FEATURE_LEVEL_9_2:     return "2.0b";
        case D3D_FEATURE_LEVEL_9_1:     return "2.0a";
    }
    return "";
}

// see https://msdn.microsoft.com/en-us/library/windows/desktop/gg615083(v=vs.85).aspx
UINT DXGetCompilerFlags(int flags)
{
    UINT dxFlags = 0;

    if ((flags & ShaderCompileFlags::Debug) != 0)
        dxFlags |= D3DCOMPILE_DEBUG;

    if ((flags & ShaderCompileFlags::O1) != 0)
        dxFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL1;
    else if ((flags & ShaderCompileFlags::O2) != 0)
        dxFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL2;
    else if ((flags & ShaderCompileFlags::O3) != 0)
        dxFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
    else
        dxFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;//D3DCOMPILE_OPTIMIZATION_LEVEL0;

    if ((flags & ShaderCompileFlags::WarnError) != 0)
        dxFlags |= D3DCOMPILE_WARNINGS_ARE_ERRORS;

    return dxFlags;
}

// see https://msdn.microsoft.com/en-us/library/windows/desktop/dd607326(v=vs.85).aspx
UINT DXGetDisassemblerFlags(int flags)
{
    UINT dxFlags = 0;

    if ((flags & ShaderDisassembleFlags::InstructionOnly) != 0)
        dxFlags |= D3D_DISASM_INSTRUCTION_ONLY;

    return dxFlags;
}

VideoAdapterDescriptor DXGetVideoAdapterDesc(IDXGIAdapter* adapter)
{
    ComPtr<IDXGIOutput> output;

    /* Query adapter description */
    DXGI_ADAPTER_DESC desc;
    adapter->GetDesc(&desc);

    /* Setup adapter information */
    VideoAdapterDescriptor videoAdapterDesc;

    videoAdapterDesc.name           = std::wstring(desc.Description);
    videoAdapterDesc.vendor         = GetVendorByID(desc.VendorId);
    videoAdapterDesc.videoMemory    = desc.DedicatedVideoMemory;

    /* Enumerate over all adapter outputs */
    for (UINT j = 0; adapter->EnumOutputs(j, &output) != DXGI_ERROR_NOT_FOUND; ++j)
    {
        /* Get output description */
        DXGI_OUTPUT_DESC desc;
        output->GetDesc(&desc);

        /* Query number of display modes */
        UINT numModes = 0;
        output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, nullptr);

        /* Query display modes */
        std::vector<DXGI_MODE_DESC> modeDesc(numModes);

        auto hr = output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, modeDesc.data());
        DXThrowIfFailed(hr, "failed to get display mode list with format DXGI_FORMAT_R8G8B8A8_UNORM");

        /* Add output information to the current adapter */
        VideoOutput videoOutput;
            
        for (UINT i = 0; i < numModes; ++i)
        {
            VideoDisplayMode displayMode;
            {
                displayMode.width       = modeDesc[i].Width;
                displayMode.height      = modeDesc[i].Height;
                displayMode.refreshRate = (modeDesc[i].RefreshRate.Denominator > 0 ? modeDesc[i].RefreshRate.Numerator / modeDesc[i].RefreshRate.Denominator : 0);
            }
            videoOutput.displayModes.push_back(displayMode);
        }
            
        /* Remove duplicate display modes */
        std::sort(videoOutput.displayModes.begin(), videoOutput.displayModes.end(), CompareSWO);

        videoOutput.displayModes.erase(
            std::unique(videoOutput.displayModes.begin(), videoOutput.displayModes.end()),
            videoOutput.displayModes.end()
        );

        /* Add output to the list and release handle */
        videoAdapterDesc.outputs.push_back(videoOutput);

        output.Reset();
    }

    return videoAdapterDesc;
}

D3DTextureFormatDescriptor DXGetTextureFormatDesc(DXGI_FORMAT format)
{
    switch (format)
    {
        case DXGI_FORMAT_D32_FLOAT:             return { ImageFormat::Depth,            DataType::Float  };
        case DXGI_FORMAT_D24_UNORM_S8_UINT:     return { ImageFormat::DepthStencil,     DataType::Float  };
        case DXGI_FORMAT_R8_UNORM:              return { ImageFormat::R,                DataType::UInt8  };
        case DXGI_FORMAT_R8_SNORM:              return { ImageFormat::R,                DataType::Int8   };
        case DXGI_FORMAT_R16_UNORM:             return { ImageFormat::R,                DataType::UInt16 };
        case DXGI_FORMAT_R16_SNORM:             return { ImageFormat::R,                DataType::Int16  };
        case DXGI_FORMAT_R32_UINT:              return { ImageFormat::R,                DataType::UInt32 };
        case DXGI_FORMAT_R32_SINT:              return { ImageFormat::R,                DataType::Int32  };
        case DXGI_FORMAT_R32_FLOAT:             return { ImageFormat::R,                DataType::Float  };
        case DXGI_FORMAT_R8G8_UNORM:            return { ImageFormat::RG,               DataType::UInt8  };
        case DXGI_FORMAT_R8G8_SNORM:            return { ImageFormat::RG,               DataType::Int8   };
        case DXGI_FORMAT_R16G16_UNORM:          return { ImageFormat::RG,               DataType::UInt16 };
        case DXGI_FORMAT_R16G16_SNORM:          return { ImageFormat::RG,               DataType::Int16  };
        case DXGI_FORMAT_R32G32_UINT:           return { ImageFormat::RG,               DataType::UInt32 };
        case DXGI_FORMAT_R32G32_SINT:           return { ImageFormat::RG,               DataType::Int32  };
        case DXGI_FORMAT_R32G32_FLOAT:          return { ImageFormat::RG,               DataType::Float  };
        case DXGI_FORMAT_R32G32B32_UINT:        return { ImageFormat::RGB,              DataType::UInt32 };
        case DXGI_FORMAT_R32G32B32_SINT:        return { ImageFormat::RGB,              DataType::Int32  };
        case DXGI_FORMAT_R32G32B32_FLOAT:       return { ImageFormat::RGB,              DataType::Float  };
        case DXGI_FORMAT_R8G8B8A8_UNORM:        return { ImageFormat::RGBA,             DataType::UInt8  };
        case DXGI_FORMAT_R8G8B8A8_SNORM:        return { ImageFormat::RGBA,             DataType::Int8   };
        case DXGI_FORMAT_R16G16B16A16_UNORM:    return { ImageFormat::RGBA,             DataType::UInt16 };
        case DXGI_FORMAT_R16G16B16A16_SNORM:    return { ImageFormat::RGBA,             DataType::Int16  };
        case DXGI_FORMAT_R32G32B32A32_UINT:     return { ImageFormat::RGBA,             DataType::UInt32 };
        case DXGI_FORMAT_R32G32B32A32_SINT:     return { ImageFormat::RGBA,             DataType::Int32  };
        case DXGI_FORMAT_R32G32B32A32_FLOAT:    return { ImageFormat::RGBA,             DataType::Float  };
        case DXGI_FORMAT_BC1_UNORM:             return { ImageFormat::CompressedRGB,    DataType::UInt8  };
        case DXGI_FORMAT_BC2_UNORM:             return { ImageFormat::CompressedRGBA,   DataType::UInt8  };
        case DXGI_FORMAT_BC3_UNORM:             return { ImageFormat::CompressedRGBA,   DataType::UInt8  };
        default:                                break;
    }
    throw std::invalid_argument("failed to map hardware texture format into image buffer format");
}


} // /namespace LLGL



// ================================================================================
