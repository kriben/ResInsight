/////////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2017     Statoil ASA
//
//  ResInsight is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  ResInsight is distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.
//
//  See the GNU General Public License at <http://www.gnu.org/licenses/gpl.html>
//  for more details.
//
/////////////////////////////////////////////////////////////////////////////////

#include "RiaFontCache.h"

#include "RiaGuiApplication.h"

#include "cafAssert.h"
#include "cafSdfAtlasFont.h"

#include <cmath>

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
std::map<int, cvf::ref<cvf::Font>> RiaFontCache::ms_fonts;

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
cvf::ref<cvf::Font> RiaFontCache::getFont( FontSize pointFontSize )
{
    int pointSize = caf::FontTools::absolutePointSize( pointFontSize );
    return getFont( pointSize );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
cvf::ref<cvf::Font> RiaFontCache::getFont( int pointSize )
{
    int currentDPI = 96;
    if ( RiaGuiApplication::isRunning() )
    {
        currentDPI = RiaGuiApplication::applicationResolution();
    }

    double scaling    = currentDPI / 96.0;
    int    scaledSize = static_cast<int>( scaling * pointSize );
    if ( scaledSize < 1 ) scaledSize = 1;

    auto existing_it = ms_fonts.find( scaledSize );
    if ( existing_it == ms_fonts.end() )
    {
        auto newFont                      = new caf::SdfAtlasFont( scaledSize );
        bool inserted                     = false;
        std::tie( existing_it, inserted ) = ms_fonts.insert( std::make_pair( scaledSize, newFont ) );
        CAF_ASSERT( inserted );
    }
    return existing_it->second;
}

//--------------------------------------------------------------------------------------------------
/// In the 2019 releases the font size was stored as an enum value rather than actual size
/// Use this method for legacy conversion
//--------------------------------------------------------------------------------------------------
RiaFontCache::FontSize RiaFontCache::legacyEnumToPointSize( int enumValue )
{
    switch ( enumValue )
    {
        case 0:
            return FontSize::FONT_SIZE_8;
        case 1:
            return FontSize::FONT_SIZE_10;
        case 2:
            return FontSize::FONT_SIZE_12;
        case 3:
            return FontSize::FONT_SIZE_14;
        case 4:
            return FontSize::FONT_SIZE_16;
        case 5:
            return FontSize::FONT_SIZE_24;
        case 6:
            return FontSize::FONT_SIZE_32;
        default:
            return FontSize::FONT_SIZE_8;
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RiaFontCache::clear()
{
    ms_fonts.clear();
}
