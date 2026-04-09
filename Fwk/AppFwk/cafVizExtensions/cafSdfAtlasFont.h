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

#include "cvfBase.h"
#include "cvfFont.h"
#include "cvfObject.h"

namespace cvf
{
class SdfFont;
}

namespace caf
{
//==================================================================================================
//
// SDF atlas font wrapper that loads the bundled Droid Sans TTF and provides crisp text at any size.
//
//==================================================================================================
class SdfAtlasFont : public cvf::Font
{
public:
    explicit SdfAtlasFont( int pointSize );
    ~SdfAtlasFont() override;

    const cvf::String&   name() const override;
    cvf::ref<cvf::Glyph> getGlyph( wchar_t character ) override;
    cvf::uint            advance( wchar_t character, wchar_t nextCharacter ) override;
    bool                 isEmpty() override;
    bool                 isSdfFont() const override { return true; }

private:
    cvf::ref<cvf::SdfFont> m_sdfFont;
};

} // namespace caf
