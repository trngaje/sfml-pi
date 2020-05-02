////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2013 Jonathan De Wachter (dewachter.jonathan@gmail.com)
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
#include <SFML/Window/EglContext.hpp>
#include <SFML/Window/WindowImpl.hpp>
#include <SFML/OpenGL.hpp>
#include <SFML/System/Err.hpp>
#include <SFML/System/Sleep.hpp>
#include <SFML/System/Mutex.hpp>
#include <SFML/System/Lock.hpp>
#ifdef SFML_SYSTEM_ANDROID
    #include <SFML/System/Android/Activity.hpp>
#endif
#if defined(SFML_SYSTEM_LINUX) && !defined(SFML_RPI)
    #include <X11/Xlib.h>
#endif

#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <go2/display.h>
#include <drm/drm_fourcc.h>
//#include <cstring>
#include <gbm.h>

#define BUFFER_MAX (3)







typedef struct buffer_surface_pair
{
    struct gbm_bo* gbmBuffer;
    go2_surface_t* surface;
} buffer_surface_pair_t;

typedef struct go2_context
{
    go2_display_t* display;    
    int width;
    int height;
    go2_context_attributes_t attributes;
    struct gbm_device* gbmDevice;
    EGLDisplay eglDisplay;
    struct gbm_surface* gbmSurface;
    EGLSurface eglSurface;
    EGLContext eglContext;
    uint32_t drmFourCC;
    buffer_surface_pair_t bufferMap[BUFFER_MAX];
    int bufferCount;
} go2_context_t;


go2_display_t* go2_display = NULL;
go2_presenter_t* go2_presenter = NULL;
go2_surface_t* go2_surface = NULL;
go2_context_t* go2_context3D = NULL;
GLuint fbo;



namespace
{
    EGLDisplay getInitializedDisplay()
    {
#if defined(SFML_SYSTEM_LINUX)

        static EGLDisplay display = EGL_NO_DISPLAY;

        if (display == EGL_NO_DISPLAY)
        {
            display = eglCheck(eglGetDisplay(EGL_DEFAULT_DISPLAY));
            eglCheck(eglInitialize(display, NULL, NULL));
        }

        return display;

#elif defined(SFML_SYSTEM_ANDROID)

    // On Android, its native activity handles this for us
    sf::priv::ActivityStates* states = sf::priv::getActivity(NULL);
    sf::Lock lock(states->mutex);

    return states->display;

#endif
    }
}


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
EglContext::EglContext(EglContext* shared) :
m_display (EGL_NO_DISPLAY),
m_context (EGL_NO_CONTEXT),
m_surface (EGL_NO_SURFACE),
m_config  (NULL)
{
    // Get the initialized EGL display
    //m_display = getInitializedDisplay();
	createContext(shared);
	
    // Get the best EGL config matching the default video settings
    m_config = getBestConfig(m_display, VideoMode::getDesktopMode().bitsPerPixel, ContextSettings());
    updateSettings();

    // Note: The EGL specs say that attrib_list can be NULL when passed to eglCreatePbufferSurface,
    // but this is resulting in a segfault. Bug in Android?
    EGLint attrib_list[] = {
        EGL_WIDTH, 1,
        EGL_HEIGHT,1,
        EGL_NONE
    };

    m_surface = eglCheck(eglCreatePbufferSurface(m_display, m_config, attrib_list));

    // Create EGL context
    //createContext(shared);
}


////////////////////////////////////////////////////////////
EglContext::EglContext(EglContext* shared, const ContextSettings& settings, const WindowImpl* owner, unsigned int bitsPerPixel) :
m_display (EGL_NO_DISPLAY),
m_context (EGL_NO_CONTEXT),
m_surface (EGL_NO_SURFACE),
m_config  (NULL)
{
#ifdef SFML_SYSTEM_ANDROID

    // On Android, we must save the created context
    ActivityStates* states = getActivity(NULL);
    Lock lock(states->mutex);

    states->context = this;

#endif

    // Get the initialized EGL display
    //m_display = getInitializedDisplay();

	createSurface((EGLNativeWindowType)owner->getSystemHandle());
    // Get the best EGL config matching the requested video settings
    m_config = getBestConfig(m_display, bitsPerPixel, settings);
    updateSettings();

    // Create EGL context
    createContext(shared);

#if !defined(SFML_SYSTEM_ANDROID)
    // Create EGL surface (except on Android because the window is created
    // asynchronously, its activity manager will call it for us)
    //createSurface((EGLNativeWindowType)owner->getSystemHandle());
#endif
}


////////////////////////////////////////////////////////////
EglContext::EglContext(EglContext* shared, const ContextSettings& settings, unsigned int width, unsigned int height) :
m_display (EGL_NO_DISPLAY),
m_context (EGL_NO_CONTEXT),
m_surface (EGL_NO_SURFACE),
m_config  (NULL)
{
}


////////////////////////////////////////////////////////////
EglContext::~EglContext()
{
	printf("[trngaje] EglContext::~EglContext()\n");
    // Deactivate the current context
    EGLContext currentContext = eglCheck(eglGetCurrentContext());

    if (currentContext == m_context)
    {
        eglCheck(eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
    }

    // Destroy context
    if (m_context != EGL_NO_CONTEXT)
    {
        eglCheck(eglDestroyContext(m_display, m_context));
    }

    // Destroy surface
    if (m_surface != EGL_NO_SURFACE)
    {
        eglCheck(eglDestroySurface(m_display, m_surface));
    }


	
	if (go2_context3D != NULL)
	{
		go2_context_destroy(go2_context3D);
		go2_context3D = NULL;
	}
	
#if 0
	m_surface = EGL_NO_SURFACE;
	m_context = EGL_NO_CONTEXT;
	
	if (go2_surface != NULL)
	{
		go2_surface_destroy(go2_surface);
		go2_surface = NULL;
	}
	
	if (go2_context3D != NULL)
	{
		go2_context_destroy(go2_context3D);
		go2_context3D = NULL;
	}
	
	if (go2_presenter != NULL)
	{
		go2_presenter_destroy(go2_presenter);
		go2_presenter = NULL;
	}
	
	if (go2_display != NULL)
	{
		go2_display_destroy(go2_display);
		go2_display = NULL;
	}
		
	#endif
}


////////////////////////////////////////////////////////////
bool EglContext::makeCurrent(bool current)
{
    if (current)
        return m_surface != EGL_NO_SURFACE && eglCheck(eglMakeCurrent(m_display, m_surface, m_surface, m_context));

    return m_surface != EGL_NO_SURFACE && eglCheck(eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
}


////////////////////////////////////////////////////////////
void EglContext::display()
{
    if (m_surface != EGL_NO_SURFACE)
	{
        eglCheck(eglSwapBuffers(m_display, m_surface));
		
        //go2_context_swap_buffers(go2_context3D);

        go2_surface_t* gles_surface = go2_context_surface_lock(go2_context3D);
        go2_presenter_post(go2_presenter,
                    gles_surface,
                    0, 0, 480, 320,
                    0, 0, 320, 480,
                    GO2_ROTATION_DEGREES_270);
		go2_context_surface_unlock(go2_context3D, gles_surface);		
	}
	else{
		printf("[trngaje] EglContext.cpp:display, m_surface==EGL_NO_SURFACE");
	}
}


////////////////////////////////////////////////////////////
void EglContext::setVerticalSyncEnabled(bool enabled)
{
    eglCheck(eglSwapInterval(m_display, enabled ? 1 : 0));
}


////////////////////////////////////////////////////////////
void EglContext::createContext(EglContext* shared)
{
    const EGLint contextVersion[] = {
        EGL_CONTEXT_CLIENT_VERSION, 1,
        EGL_NONE
    };

    EGLContext toShared;

    if (shared)
        toShared = shared->m_context;
    else
        toShared = EGL_NO_CONTEXT;

    if (toShared != EGL_NO_CONTEXT)
        eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    // Create EGL context
    m_context = eglCheck(eglCreateContext(m_display, m_config, toShared, contextVersion));
}


////////////////////////////////////////////////////////////
void EglContext::createSurface(EGLNativeWindowType window)
{
	//m_surface = eglCheck(eglCreateWindowSurface(m_display, m_config, window, NULL));
	
	if (go2_display == NULL)
		go2_display = go2_display_create();
		
 	if (go2_presenter == NULL)
		go2_presenter = go2_presenter_create(go2_display, DRM_FORMAT_RGB565, 0xff080808);


	go2_context_attributes_t attr;
	attr.major = 3;
	attr.minor = 2;
	attr.red_bits = 5;
	attr.green_bits = 6;
	attr.blue_bits = 5;
	attr.alpha_bits = 0;
	attr.depth_bits = 24;
	attr.stencil_bits = 8;

	go2_context3D = go2_context_create(go2_display, 480, 320, &attr);

	m_surface = go2_context3D->eglSurface;
	m_display = go2_context3D->eglDisplay;
	
	if (m_context == NULL)
		m_context = go2_context3D->eglContext;

	
	printf("[trngaje] EglContext.cpp:createSurface m_surface=0x%x\n", m_surface);
}


////////////////////////////////////////////////////////////
void EglContext::destroySurface()
{
    eglCheck(eglDestroySurface(m_display, m_surface));
    m_surface = EGL_NO_SURFACE;

    // Ensure that this context is no longer active since our surface is now destroyed
    setActive(false);
}


////////////////////////////////////////////////////////////
EGLConfig EglContext::getBestConfig(EGLDisplay display, unsigned int bitsPerPixel, const ContextSettings& settings)
{
    // Set our video settings constraint
    const EGLint attributes[] = {
        EGL_BUFFER_SIZE, bitsPerPixel,
        EGL_DEPTH_SIZE, settings.depthBits,
        EGL_STENCIL_SIZE, settings.stencilBits,
        EGL_SAMPLE_BUFFERS, settings.antialiasingLevel,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
        EGL_NONE
    };

    EGLint configCount;
    EGLConfig configs[1];

    // Ask EGL for the best config matching our video settings
    eglCheck(eglChooseConfig(display, attributes, configs, 1, &configCount));

    // TODO: This should check EGL_CONFORMANT and pick the first conformant configuration.

    return configs[0];
}


////////////////////////////////////////////////////////////
void EglContext::updateSettings()
{
    EGLint tmp;
    
    // Update the internal context settings with the current config
    eglCheck(eglGetConfigAttrib(m_display, m_config, EGL_DEPTH_SIZE, &tmp));
    m_settings.depthBits = tmp;
    
    eglCheck(eglGetConfigAttrib(m_display, m_config, EGL_STENCIL_SIZE, &tmp));
    m_settings.stencilBits = tmp;
    
    eglCheck(eglGetConfigAttrib(m_display, m_config, EGL_SAMPLES, &tmp));
    m_settings.antialiasingLevel = tmp;
    
    m_settings.majorVersion = 1;
    m_settings.minorVersion = 1;
    m_settings.attributeFlags = ContextSettings::Default;
}


#if defined(SFML_SYSTEM_LINUX) && !defined(SFML_RPI)
////////////////////////////////////////////////////////////
XVisualInfo EglContext::selectBestVisual(::Display* XDisplay, unsigned int bitsPerPixel, const ContextSettings& settings)
{
    // Get the initialized EGL display
    EGLDisplay display = getInitializedDisplay();

    // Get the best EGL config matching the default video settings
    EGLConfig config = getBestConfig(display, bitsPerPixel, settings);

    // Retrieve the visual id associated with this EGL config
    EGLint nativeVisualId;

    eglCheck(eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &nativeVisualId));

    if (nativeVisualId == 0)
    {
        // Should never happen...
        err() << "No EGL visual found. You should check your graphics driver" << std::endl;

        return XVisualInfo();
    }

    XVisualInfo vTemplate;
    vTemplate.visualid = static_cast<VisualID>(nativeVisualId);

    // Get X11 visuals compatible with this EGL config
    XVisualInfo *availableVisuals, bestVisual;
    int visualCount = 0;

    availableVisuals = XGetVisualInfo(XDisplay, VisualIDMask, &vTemplate, &visualCount);

    if (visualCount == 0)
    {
        // Can't happen...
        err() << "No X11 visual found. Bug in your EGL implementation ?" << std::endl;

        return XVisualInfo();
    }

    // Pick up the best one
    bestVisual = availableVisuals[0];
    XFree(availableVisuals);

    return bestVisual;
}
#endif

} // namespace priv

} // namespace sf
