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


#pragma once

#include "cvfObject.h"
#include "cvfFont.h"
#include "cvfArray.h"
#include "cvfString.h"

#include <map>

namespace cvf {

class Glyph;
class TextureImage;
class SdfFreeTypeInterface;


//==================================================================================================
//
// SDF (Signed Distance Field) font class.
// Renders glyphs at a high resolution, generates SDF textures, and scales metrics to display size.
// Produces crisp text at any zoom level from a single small texture per glyph.
//
//==================================================================================================
class SdfFont : public Font
{
public:
    SdfFont(uint renderSize = 64, uint sdfSize = 48, uint spread = 6);
    virtual ~SdfFont();

    // Overridden pure virtual functions from Font
    virtual const String& name() const;
    virtual ref<Glyph>    getGlyph(wchar_t character);
    virtual uint          advance(wchar_t character, wchar_t nextCharacter);
    virtual bool          isEmpty();
    virtual bool          isSdfFont() const { return true; }

    void setDisplaySize(uint pointSize);

    // Load font
    bool load(const String& path);
    bool load(const ubyte* data, size_t numBytes);

private:
    ref<Glyph>      createSdfGlyph(wchar_t character);
    ref<TextureImage> generateSdfFromBitmap(const TextureImage& bitmap, uint sdfWidth, uint sdfHeight);

private:
    typedef std::map<wchar_t, ref<Glyph> > MapType;

    ref<SdfFreeTypeInterface> m_fontInterface;
    MapType     m_atlasMap;
    String      m_name;
    uint        m_renderSize;   // FreeType rasterization size
    uint        m_sdfSize;      // Output SDF texture size
    uint        m_spread;       // Distance field spread in pixels
    uint        m_displaySize;  // Logical display size (point size)
};

} // namespace cvf
