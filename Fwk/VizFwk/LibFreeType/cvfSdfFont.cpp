//##################################################################################################
//
//   Custom Visualization Core library
//   Copyright (C) Ceetron Solutions AS
//
//   This library may be used under the terms of either the GNU General Public License or
//   the GNU Lesser General Public License as follows:
//
//   GNU General Public License Usage
//   This library is free software: you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, either version 3 of the License, or
//   (at your option) any later version.
//
//   This library is distributed in the hope that it will be useful, but WITHOUT ANY
//   WARRANTY; without even the implied warranty of MERCHANTABILITY or
//   FITNESS FOR A PARTICULAR PURPOSE.
//
//   See the GNU General Public License at <<http://www.gnu.org/licenses/gpl.html>>
//   for more details.
//
//   GNU Lesser General Public License Usage
//   This library is free software; you can redistribute it and/or modify
//   it under the terms of the GNU Lesser General Public License as published by
//   the Free Software Foundation; either version 2.1 of the License, or
//   (at your option) any later version.
//
//   This library is distributed in the hope that it will be useful, but WITHOUT ANY
//   WARRANTY; without even the implied warranty of MERCHANTABILITY or
//   FITNESS FOR A PARTICULAR PURPOSE.
//
//   See the GNU Lesser General Public License at <<http://www.gnu.org/licenses/lgpl-2.1.html>>
//   for more details.
//
//##################################################################################################


#include "cvfBase.h"
#include "cvfSdfFont.h"
#include "cvfGlyph.h"
#include "cvfTextureImage.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H

#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>

namespace cvf {


//==================================================================================================
//
// Private FreeType interface for SDF font
//
//==================================================================================================
class SdfFreeTypeInterface : public Object
{
public:
    FT_Library  m_ftLib;
    FT_Face     m_ftFace;
    UByteArray  m_data;

    SdfFreeTypeInterface()
    :   m_ftLib(NULL),
        m_ftFace(NULL)
    {
        FT_Error error = FT_Init_FreeType(&m_ftLib);
        CVF_ASSERT(!error);
        CVF_UNUSED(error);
    }

    ~SdfFreeTypeInterface()
    {
        unload();
        FT_Done_FreeType(m_ftLib);
    }

    bool isEmpty() const
    {
        return m_ftFace ? false : true;
    }

    String name()
    {
        String fontName;
        if (!isEmpty() && m_ftFace->family_name)
        {
            fontName += m_ftFace->family_name;
            if (m_ftFace->style_name)
            {
                fontName += " ";
                fontName += m_ftFace->style_name;
            }
        }
        return fontName;
    }

    bool load(const ubyte* data, size_t numBytes)
    {
        CVF_TIGHT_ASSERT(data);
        CVF_TIGHT_ASSERT(numBytes > 0);
        CVF_TIGHT_ASSERT(m_ftLib);

        unload();

        m_data.assign(data, numBytes);

        const FT_Byte* file_base = static_cast<const FT_Byte*>(m_data.ptr());
        FT_Long file_size = static_cast<FT_Long>(m_data.size());

        FT_Error error = FT_New_Memory_Face(m_ftLib, file_base, file_size, 0, &m_ftFace);
        return error ? false : true;
    }

    bool load(const cvf::String& filename)
    {
        unload();
        FT_Error error = FT_New_Face(m_ftLib, filename.toUtf8().ptr(), 0, &m_ftFace);
        return error ? false : true;
    }

    bool setSize(uint size)
    {
        CVF_ASSERT(!isEmpty());
        FT_Error error = FT_Set_Char_Size(m_ftFace, 0, static_cast<FT_F26Dot6>(size) << 6, 96, 96);
        return error ? false : true;
    }

    void unload()
    {
        if (isEmpty()) return;
        FT_Done_Face(m_ftFace);
        m_ftFace = NULL;
        m_data.clear();
    }

    bool getKerning(wchar_t character, wchar_t nextCharacter, int* kerning)
    {
        if (isEmpty()) return false;
        if (!FT_HAS_KERNING(m_ftFace)) return false;

        FT_UInt curr_glyph_index = FT_Get_Char_Index(m_ftFace, character);
        FT_UInt next_glyph_index = FT_Get_Char_Index(m_ftFace, nextCharacter);

        FT_Vector delta;
        FT_Get_Kerning(m_ftFace, curr_glyph_index, next_glyph_index, FT_KERNING_DEFAULT, &delta);

        *kerning = delta.x >> 6;
        return true;
    }
};


//==================================================================================================
///
/// \class cvf::SdfFont
/// \ingroup FreeType
///
/// SDF font class that generates Signed Distance Field textures for crisp text at any scale.
//==================================================================================================

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
SdfFont::SdfFont(uint renderSize, uint sdfSize, uint spread)
:   m_renderSize(renderSize),
    m_sdfSize(sdfSize),
    m_spread(spread),
    m_displaySize(12)
{
    m_fontInterface = new SdfFreeTypeInterface();
}


//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
SdfFont::~SdfFont()
{
    m_atlasMap.clear();
}


//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
const String& SdfFont::name() const
{
    return m_name;
}


//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
ref<Glyph> SdfFont::getGlyph(wchar_t character)
{
    if (isEmpty()) return NULL;

    MapType::iterator it = m_atlasMap.find(character);
    if (it != m_atlasMap.end())
    {
        return it->second;
    }

    ref<Glyph> glyph = createSdfGlyph(character);
    if (glyph.notNull())
    {
        m_atlasMap.insert(MapType::value_type(character, glyph));
    }

    return glyph;
}


//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
uint SdfFont::advance(wchar_t character, wchar_t nextCharacter)
{
    int kerning = 0;
    m_fontInterface->getKerning(character, nextCharacter, &kerning);

    // Scale kerning from render size to display size
    double scale = static_cast<double>(m_displaySize) / static_cast<double>(m_renderSize);
    kerning = static_cast<int>(kerning * scale);

    ref<Glyph> glyph = getGlyph(character);
    if (glyph.isNull()) return 0;

    uint charWidth = glyph->horizontalBearingX() + glyph->width() + kerning;
    return CVF_MAX(charWidth, glyph->horizontalAdvance());
}


//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool SdfFont::isEmpty()
{
    return m_fontInterface->isEmpty();
}


//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void SdfFont::setDisplaySize(uint pointSize)
{
    if (m_displaySize != pointSize)
    {
        m_displaySize = pointSize;
        m_atlasMap.clear();
    }
}


//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool SdfFont::load(const String& path)
{
    m_atlasMap.clear();

    bool loaded = m_fontInterface->load(path);
    if (loaded)
    {
        m_fontInterface->setSize(m_renderSize);
        m_name = m_fontInterface->name();
    }
    return loaded;
}


//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool SdfFont::load(const ubyte* data, size_t numBytes)
{
    m_atlasMap.clear();

    bool loaded = m_fontInterface->load(data, numBytes);
    if (loaded)
    {
        m_fontInterface->setSize(m_renderSize);
        m_name = m_fontInterface->name();
    }
    return loaded;
}


//--------------------------------------------------------------------------------------------------
/// 8SSEDT (8-point Signed Sequential Euclidean Distance Transform)
/// Generates an SDF texture from a high-res bitmap glyph.
//--------------------------------------------------------------------------------------------------
ref<TextureImage> SdfFont::generateSdfFromBitmap(const TextureImage& bitmap, uint sdfWidth, uint sdfHeight)
{
    uint bmpW = bitmap.width();
    uint bmpH = bitmap.height();

    // Helper struct for distance grid
    struct Point
    {
        float dx, dy;
        float distSq() const { return dx * dx + dy * dy; }
    };

    const float INF = 1e10f;
    Point pointInside  = { 0.0f, 0.0f };
    Point pointOutside = { INF, INF };

    // Build two distance grids: outside and inside the glyph
    std::vector<Point> gridOutside(bmpW * bmpH);
    std::vector<Point> gridInside(bmpW * bmpH);

    for (uint y = 0; y < bmpH; y++)
    {
        for (uint x = 0; x < bmpW; x++)
        {
            uint idx = y * bmpW + x;
            Color4ub pixel = bitmap.pixel(x, y);
            ubyte alpha = pixel.a();

            if (alpha > 127)
            {
                gridOutside[idx] = pointInside;
                gridInside[idx]  = pointOutside;
            }
            else
            {
                gridOutside[idx] = pointOutside;
                gridInside[idx]  = pointInside;
            }
        }
    }

    // 8SSEDT helper lambda
    auto compare = [](std::vector<Point>& grid, uint bmpW, uint bmpH, int x, int y, int offsetX, int offsetY)
    {
        int nx = x + offsetX;
        int ny = y + offsetY;

        if (nx < 0 || nx >= static_cast<int>(bmpW) || ny < 0 || ny >= static_cast<int>(bmpH))
            return;

        uint idx = y * bmpW + x;
        uint nIdx = ny * bmpW + nx;

        Point other;
        other.dx = grid[nIdx].dx + static_cast<float>(offsetX);
        other.dy = grid[nIdx].dy + static_cast<float>(offsetY);

        if (other.distSq() < grid[idx].distSq())
        {
            grid[idx] = other;
        }
    };

    // Forward pass and backward pass for both grids
    auto propagate = [&](std::vector<Point>& grid)
    {
        // Forward pass: top-left to bottom-right
        for (uint y = 0; y < bmpH; y++)
        {
            for (uint x = 0; x < bmpW; x++)
            {
                compare(grid, bmpW, bmpH, x, y, -1,  0);
                compare(grid, bmpW, bmpH, x, y,  0, -1);
                compare(grid, bmpW, bmpH, x, y, -1, -1);
                compare(grid, bmpW, bmpH, x, y,  1, -1);
            }
        }

        // Backward pass: bottom-right to top-left
        for (int y = static_cast<int>(bmpH) - 1; y >= 0; y--)
        {
            for (int x = static_cast<int>(bmpW) - 1; x >= 0; x--)
            {
                compare(grid, bmpW, bmpH, x, y, 1, 0);
                compare(grid, bmpW, bmpH, x, y, 0, 1);
                compare(grid, bmpW, bmpH, x, y, 1, 1);
                compare(grid, bmpW, bmpH, x, y,-1, 1);
            }
        }
    };

    propagate(gridOutside);
    propagate(gridInside);

    // Create SDF output by downsampling
    ref<TextureImage> sdfImage = new TextureImage;
    sdfImage->allocate(sdfWidth, sdfHeight);

    float scaleX = static_cast<float>(bmpW) / static_cast<float>(sdfWidth);
    float scaleY = static_cast<float>(bmpH) / static_cast<float>(sdfHeight);
    float spreadF = static_cast<float>(m_spread);

    for (uint y = 0; y < sdfHeight; y++)
    {
        for (uint x = 0; x < sdfWidth; x++)
        {
            // Map SDF pixel to high-res bitmap pixel
            uint bx = static_cast<uint>(std::min(static_cast<float>(x) * scaleX + 0.5f, static_cast<float>(bmpW - 1)));
            uint by = static_cast<uint>(std::min(static_cast<float>(y) * scaleY + 0.5f, static_cast<float>(bmpH - 1)));
            uint bIdx = by * bmpW + bx;

            float outsideDist = std::sqrt(gridOutside[bIdx].distSq());
            float insideDist  = std::sqrt(gridInside[bIdx].distSq());
            float signedDist  = outsideDist - insideDist;

            // Map signed distance to [0, 255] with 128 being the edge
            float normalized = 128.0f - signedDist * (128.0f / spreadF);
            ubyte value = static_cast<ubyte>(std::max(0.0f, std::min(255.0f, normalized)));

            sdfImage->setPixel(x, y, Color4ub(255, 255, 255, value));
        }
    }

    return sdfImage;
}


//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
ref<Glyph> SdfFont::createSdfGlyph(wchar_t character)
{
    SdfFreeTypeInterface* fi = m_fontInterface.p();
    if (fi->isEmpty()) return NULL;

    // Ensure we're at render size for high-res rasterization
    fi->setSize(m_renderSize);

    FT_UInt glyph_index = FT_Get_Char_Index(fi->m_ftFace, character);

    FT_Error error = FT_Load_Glyph(fi->m_ftFace, glyph_index, FT_LOAD_DEFAULT);
    if (error) return NULL;

    error = FT_Render_Glyph(fi->m_ftFace->glyph, FT_RENDER_MODE_NORMAL);
    if (error) return NULL;

    // Scale factor from render size to display size
    double scale = static_cast<double>(m_displaySize) / static_cast<double>(m_renderSize);

    ref<Glyph> glyph = new Glyph(character);

    // Scale metrics from render size to display size
    short bearingX = static_cast<short>(fi->m_ftFace->glyph->metrics.horiBearingX >> 6);
    short bearingY = static_cast<short>(fi->m_ftFace->glyph->metrics.horiBearingY >> 6);
    uint  hAdvance = fi->m_ftFace->glyph->metrics.horiAdvance >> 6;

    glyph->setHorizontalBearingX(static_cast<short>(bearingX * scale));
    glyph->setHorizontalBearingY(static_cast<short>(bearingY * scale));
    glyph->setHorizontalAdvance(static_cast<uint>(hAdvance * scale));

    if (fi->m_ftFace->glyph && (fi->m_ftFace->glyph->bitmap.width > 0) && (fi->m_ftFace->glyph->bitmap.rows > 0))
    {
        uint bitmapWidth  = static_cast<uint>(fi->m_ftFace->glyph->bitmap.width);
        uint bitmapHeight = static_cast<uint>(fi->m_ftFace->glyph->bitmap.rows);

        // Create high-res bitmap
        TextureImage highResBitmap;
        highResBitmap.allocate(bitmapWidth, bitmapHeight);

        for (uint y = 0; y < bitmapHeight; y++)
        {
            for (uint x = 0; x < bitmapWidth; x++)
            {
                ubyte alpha = fi->m_ftFace->glyph->bitmap.buffer[((bitmapHeight - 1 - y) * bitmapWidth) + x];
                highResBitmap.setPixel(x, y, Color4ub(255, 255, 255, alpha));
            }
        }

        // Calculate SDF texture size (scale bitmap dimensions)
        uint sdfW = static_cast<uint>(std::max(1.0, bitmapWidth * scale));
        uint sdfH = static_cast<uint>(std::max(1.0, bitmapHeight * scale));

        ref<TextureImage> sdfImage = generateSdfFromBitmap(highResBitmap, sdfW, sdfH);

        glyph->setTextureImage(sdfImage.p());
        glyph->setWidth(sdfImage->width());
        glyph->setHeight(sdfImage->height());

        glyph->setMinFilter(Glyph::LINEAR);
        glyph->setMagFilter(Glyph::LINEAR);
    }
    else
    {
        // Empty glyph (e.g. space character)
        if (glyph->horizontalAdvance() == 0)
        {
            FT_Error err = FT_Load_Glyph(fi->m_ftFace, 0, FT_LOAD_DEFAULT);
            if (err)
            {
                glyph->setHorizontalAdvance(static_cast<uint>(10 * scale));
            }
            else
            {
                glyph->setHorizontalAdvance(static_cast<uint>((fi->m_ftFace->glyph->advance.x >> 6) * scale));
            }
        }

        uint advPx = glyph->horizontalAdvance();
        if (advPx == 0) advPx = 1;

        TextureImage* textureImage = new TextureImage;
        textureImage->allocate(advPx, advPx);
        textureImage->fill(Color4ub(0, 0, 0, 0));
        glyph->setTextureImage(textureImage);

        glyph->setWidth(textureImage->width());
        glyph->setHeight(textureImage->height());

        glyph->setHorizontalBearingX(0);
        glyph->setHorizontalBearingY(0);
    }

    return glyph;
}


}  // namespace cvf
