/*
 * IndexFormat.h
 * 
 * This file is part of the "LLGL" project (Copyright (c) 2015 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#ifndef __LLGL_INDEX_FORMAT_H__
#define __LLGL_INDEX_FORMAT_H__


#include "Export.h"
#include "RenderSystemFlags.h"


namespace LLGL
{


class LLGL_EXPORT IndexFormat
{

    public:

        IndexFormat(const DataType dataType);

        //! Returns the data type of this index format.
        inline DataType GetDataType() const
        {
            return dataType_;
        }

        //! Returns the size of this vertex format (in bytes).
        inline unsigned int GetFormatSize() const
        {
            return formatSize_;
        }

    private:

        DataType        dataType_   = DataType::UInt32;
        unsigned int    formatSize_ = 0;

};


} // /namespace LLGL


#endif



// ================================================================================
