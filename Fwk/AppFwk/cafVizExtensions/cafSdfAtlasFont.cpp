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

#include "cafSdfAtlasFont.h"
#include "cafDroidSansData.h"
#include "cvfBase.h"
#include "cvfGlyph.h"
#include "cvfSdfFont.h"

namespace caf
{

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
SdfAtlasFont::SdfAtlasFont( int pointSize )
{
    m_sdfFont = new cvf::SdfFont( 64, 48, 6 );
    m_sdfFont->load( cafDroidSansData, cafDroidSansData_len );
    m_sdfFont->setDisplaySize( static_cast<cvf::uint>( pointSize ) );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
SdfAtlasFont::~SdfAtlasFont()
{
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
const cvf::String& SdfAtlasFont::name() const
{
    return m_sdfFont->name();
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
cvf::ref<cvf::Glyph> SdfAtlasFont::getGlyph( wchar_t character )
{
    return m_sdfFont->getGlyph( character );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
cvf::uint SdfAtlasFont::advance( wchar_t character, wchar_t nextCharacter )
{
    return m_sdfFont->advance( character, nextCharacter );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
bool SdfAtlasFont::isEmpty()
{
    return m_sdfFont->isEmpty();
}

} // namespace caf
