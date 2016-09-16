/*
 * RenderSystemFlags.cpp
 * 
 * This file is part of the "LLGL" project (Copyright (c) 2015 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#include <LLGL/RenderSystemFlags.h>


namespace LLGL
{


LLGL_EXPORT std::size_t DataTypeSize(const DataType dataType)
{
    switch (dataType)
    {
        case DataType::Float32: return 4;
        case DataType::Float64: return 8;
        case DataType::Int8:    return 1;
        case DataType::UInt8:   return 1;
        case DataType::Int16:   return 2;
        case DataType::UInt16:  return 2;
        case DataType::Int32:   return 4;
        case DataType::UInt32:  return 4;
    }
    return 0;
}


} // /namespace LLGL



// ================================================================================
