////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
//
// Raspberry Pi dispmanx implementation
// Copyright (C) 2016 Andrew Mickelson
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Window/VideoModeImpl.hpp>
#if 0 // by trngaje
#include <bcm_host.h>
#endif
namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
std::vector<VideoMode> VideoModeImpl::getFullscreenModes()
{
    std::vector<VideoMode> modes;
    modes.push_back(getDesktopMode());
    return modes;
}


////////////////////////////////////////////////////////////
VideoMode VideoModeImpl::getDesktopMode()
{
    static bool initialized=false;

    if (!initialized)
    {
#if 0 // by trngaje
        bcm_host_init();
#endif
        initialized=true;
    }

#if 0 // by trngaje
    uint32_t width( 0 ), height( 0 );
    graphics_get_display_size( 0 /* LCD */, &width, &height );
    return VideoMode(width, height);
#endif

    //return VideoMode(480, 320);

    return VideoMode(854, 480);
	
}

} // namespace priv

} // namespace sf
