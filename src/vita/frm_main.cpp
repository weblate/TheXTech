/*
 * TheXTech - A platform game engine ported from old source code for VB6
 *
 * Copyright (c) 2009-2011 Andrew Spinks, original VB6 code
 * Copyright (c) 2020-2020 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef USE_STBI
#define USE_STBI
#endif

#include "../globals.h"
#include "../game_main.h"
#include "../graphics.h"
#include "../control/joystick.h"
#include "../sound.h"
#include "../editor/editor.h"

#include <AppPath/app_path.h>
#include <Logger/logger.h>
#include <Utils/files.h>
#include <Utils/elapsed_timer.h>
#include <DirManager/dirman.h>
#include <chrono>
#include <fmt_format_ne.h>
#include <Graphics/graphics_funcs.h>
#include <FreeImageLite.h>
#include "../editor/new_editor.h"


#include <vitasdk.h>
#include <vitaGL.h>


#include "../frm_main.h"

#ifndef NO_SDL
#include <SDL2/SDL.h>
#endif

#ifndef DISPLAY_WIDTH_DEF
#define DISPLAY_WIDTH_DEF 960
#endif
#ifndef DISPLAY_HEIGHT_DEF
#define DISPLAY_HEIGHT_DEF 544
#endif

#ifdef USE_STBI
#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PSD
#define STBI_ONLY_BMP
#define STBI_ONLY_GIF
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_PNM
#define STBI_ONLY_TGA
#include "stb_image.h"
#endif
#endif

static unsigned int num_textures_loaded = 0;
static const int vgl_pool_size = 0x100000;
static const int vgl_cdram_threshold = 256 * 1024;
static const int vgl_phycont_threshold = 1 * 1024 * 1024;
static const int vgl_pool_ram_threshold = vgl_pool_size * 2;//0x1000000
static const SceGxmMultisampleMode vgl_msaa = SCE_GXM_MULTISAMPLE_NONE;
static int ram_pool_count = vgl_pool_size;

// typedef struct SDL_Point
// {
//     float x, y;
// } SDL_Point;

FrmMain::FrmMain()
{
    ScaleWidth = 960;
    ScaleHeight = 544;
}

#include <psp2/kernel/sysmem.h>

static const int MEMORY_DIVISOR = 1e+6;

static inline void print_memory_info()
{
    SceKernelFreeMemorySizeInfo info;
    info.size = sizeof(SceKernelFreeMemorySizeInfo);
    if(sceKernelGetFreeMemorySize(&info) < 0)
    {
        pLogCritical("sceKernelGetFreeMemorySize returned less than 0.");
        return;
    }

    pLogDebug(
        "PS VITA MEMORY STATS\nUSER_RW MEMORY FREE: %.2fMB\nUSER_CDRAM_RW: %.2fMB\nUSER_MAIN_PHYCONT_*_RW: %.2fMB\n\n",
        (info.size_user / (float)MEMORY_DIVISOR),
        (info.size_cdram / (float)MEMORY_DIVISOR),
        (info.size_phycont / (float)MEMORY_DIVISOR)
    );
}

bool FrmMain::initSDL(const CmdLineSetup_t &setup)
{
    bool res = false;
    LoadLogSettings(setup.interprocess, setup.verboseLogging);

    _debugPrintf_("--Before vglInit--");
    print_memory_info();

    //void vglInitWithCustomThreshold(int pool_size, int width, int height, int ram_threshold, int cdram_threshold, int phycont_threshold, SceGxmMultisampleMode msaa)
    // vglInitWithCustomThreshold(
    //     vgl_pool_size, 
    //     DISPLAY_WIDTH_DEF, DISPLAY_HEIGHT_DEF, 
    //     vgl_pool_ram_threshold, 
    //     vgl_cdram_threshold, 
    //     vgl_phycont_threshold, 
    //     vgl_msaa);
    // vglInit(vgl_pool_size);
    vglInitExtended(0x1400000, DISPLAY_WIDTH_DEF, DISPLAY_HEIGHT_DEF, 0x1000000, SCE_GXM_MULTISAMPLE_NONE);
    vglUseVram(GL_TRUE);

    _debugPrintf_("--After vglInit--");
    _debugPrintf_("PS VITA: Init with pool size of %.4fMB", (vgl_pool_size / (float)MEMORY_DIVISOR));
    print_memory_info();



    if(_debugPrintf_ != 0)
    {
        _debugPrintf_("PS VITA: TODO, init vitaGL and init SDL BUT only init SDL for input and audio.");
    }
    else
    {
        _debugPrintf_ = pLogWarning;
        _debugPrintf_("PS VITA: _debugPrintf_ assigned to pLogWarning.");
        _debugPrintf_("PS VITA: TODO, init vitaGL and init SDL BUT only init SDL for input and audio.");
    }

    Uint32 sdlInitFlags = 0;
    sdlInitFlags |= SDL_INIT_TIMER;
    sdlInitFlags |= SDL_INIT_AUDIO;
    // sdlInitFlags |= SDL_INIT_VIDEO;
    sdlInitFlags |= SDL_INIT_EVENTS;
    sdlInitFlags |= SDL_INIT_JOYSTICK;
    sdlInitFlags |= SDL_INIT_HAPTIC;
    sdlInitFlags |= SDL_INIT_GAMECONTROLLER;

    
    res = (SDL_Init(sdlInitFlags) < 0);
    m_sdlLoaded = !res;

    _debugPrintf_("--After SDL_Init--");
    print_memory_info();
    
    const char* error = SDL_GetError();
    if(*error != '\0')
        pLogWarning("Error while SDL Init: %s", error);
    SDL_ClearError();

    // TODO: Will this fuck everything up?
    m_window = SDL_CreateWindow("Vita", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 16, 16, SDL_WINDOW_HIDDEN);
    if(m_window == nullptr)
    {
        pLogCritical("Unable to create an SDL Window!");
        SDL_ClearError();
        return false;
    }

    clearBuffer();
    m_keyboardState = SDL_GetKeyboardState(nullptr);
    updateViewport();
    clearBuffer();
    setTargetScreen();
    repaint();
    doEvents();

    // m_gif.init();

    _debugPrintf_("SDL has initialized.\n");

    return res;
}

bool FrmMain::freeTextureMem() // make it take an amount of memory, someday.....
{
    printf("Freeing texture memory...\n");
    StdPicture* oldest = nullptr;
    uint32_t earliestDraw = 0;
    StdPicture* second_oldest = nullptr;
    uint32_t second_earliestDraw = 0;
    for (StdPicture* poss : m_textureBank)
    {
        if (poss->texture && poss->lazyLoaded && (poss->lastDrawFrame+30 < currentFrame))
        {
            if ((oldest == nullptr) || (poss->lastDrawFrame < earliestDraw))
            {
                second_oldest = oldest;
                second_earliestDraw = earliestDraw;
                oldest = poss;
                earliestDraw = poss->lastDrawFrame;
            }
            else if ((second_oldest == nullptr) || (poss->lastDrawFrame < second_earliestDraw))
            {
                second_oldest = poss;
                second_earliestDraw = poss->lastDrawFrame;
            }
        }
    }
    if (oldest == nullptr) return false;
    printf("Clearing %p, %p\n", oldest, second_oldest);
    printf("Clearing %s, %s\n", oldest->path.c_str(), (second_oldest) ? second_oldest->path.c_str() : "");
    lazyUnLoad(*oldest);
    if (second_oldest) lazyUnLoad(*second_oldest);
    return true;
}

void FrmMain::freeSDL()
{
    pLogDebug("<Application Closing>");

    pLogDebug("GFX.unLoad");
    GFX.unLoad();

    pLogDebug("clearAllTextures");
    clearAllTextures();

    pLogDebug("joyCloseJoysticks");
    joyCloseJoysticks();

    pLogDebug("SDL_DestroyWindow");
    if(m_window)
        SDL_DestroyWindow(m_window);
    m_window = nullptr;

    pLogDebug("SDL_Quit");
    SDL_Quit();

    pLogDebug("vglEnd");
    vglEnd();

    // TODO: Fix "has not currently been declared"
    // GraphicsHelps::closeFreeImage();
    pLogDebug("<Application Closed>");
    CloseLog();
}

void FrmMain::show()
{
    SDL_ShowWindow(m_window);
}

void FrmMain::hide()
{
    SDL_HideWindow(m_window);
    showCursor(1);
}

bool FrmMain::isWindowActive() 
{return true;}

bool FrmMain::hasWindowMouseFocus()
{return true;}

void FrmMain::eventResize()
{
    // update viewport?
    SetupScreens();
}

int FrmMain::setFullScreen(bool fs)
{
    return 0;
}

bool FrmMain::isSdlError()
{
    return false;
}

void FrmMain::setTargetTexture() {}
void FrmMain::setTargetScreen() {}

void FrmMain::processEvent()
{
    switch(m_event.type)
    {
    case SDL_QUIT:
        showCursor(1);
        KillIt();
        break;
    case SDL_JOYDEVICEADDED:
        joyDeviceAddEvent(&m_event.jdevice);
        break;
    case SDL_JOYDEVICEREMOVED:
        joyDeviceRemoveEvent(&m_event.jdevice);
        break;
    case SDL_WINDOWEVENT:
        if((m_event.window.event == SDL_WINDOWEVENT_RESIZED) || (m_event.window.event == SDL_WINDOWEVENT_MOVED))
        {
            eventResize();
        }
//#ifndef __EMSCRIPTEN__
//        else if(m_event.window.event == SDL_WINDOWEVENT_MAXIMIZED)
//        {
//            SDL_RestoreWindow(m_window);
//            SetRes();
//        }
//#endif
        break;
    case SDL_KEYDOWN:
        eventKeyDown(m_event.key);
        eventKeyPress(m_event.key.keysym.scancode);
        break;
    case SDL_KEYUP:
        eventKeyUp(m_event.key);
        break;
    case SDL_MOUSEBUTTONDOWN:
        eventMouseDown(m_event.button);
        break;
    case SDL_MOUSEBUTTONUP:
        eventMouseUp(m_event.button);
        break;
    case SDL_MOUSEMOTION:
        eventMouseMove(m_event.motion);
        break;
    }
    return;
}
void FrmMain::waitEvents()
{
    if(SDL_WaitEventTimeout(&m_event, 1000))
        processEvent();
    doEvents();
}

void FrmMain::doEvents()
{
    while(SDL_PollEvent(&m_event))
    {
        processEvent();
    }
}

void FrmMain::repaint()
{
    // paint from render target to screen.
}



/// INCOMPLETE
StdPicture FrmMain::LoadPicture(std::string path)
{
    StdPicture target;
    // FIBITMAP *sourceImage;
    stbi_uc *sourceImage;

    if(!GameIsActive) return target;
    target.inited = false;
    target.path = path;

    if(target.path.empty()) return target;

    int w = 0, h = 0, channels = 0;
    sourceImage = stbi_load(path.c_str(), &w, &h, &channels, STBI_rgb_alpha);

    if(!sourceImage)
    {
        pLogWarning("Error, loading of image file:\n%s", path.c_str());
        return target;
    }
    else
    {
        pLogDebug("VITA: Successfully loaded %s. Size: %d x %d with %d channels.", path.c_str(), w, h, channels);
    }

    target.inited = true;
    target.lazyLoaded = false;

    // Get width and height
    // uint32_t w = static_cast<uint32_t>(FreeImage_GetWidth(sourceImage));
    // uint32_t h = static_cast<uint32_t>(FreeImage_GetHeight(sourceImage));

    if((w == 0) || (h == 0))
    {
        // FreeImage_Unload(sourceImage);
        stbi_image_free(sourceImage);
        pLogWarning("Error loading of image file:\n%s\nReason: Zero Image size.", path.c_str());
        return target;
    }

    GLubyte* textura = reinterpret_cast<GLubyte*>(sourceImage);
    loadTexture(target, w, h, textura);

    num_textures_loaded++;
    // GraphicsHelps::closeImage(sourceImage);
    stbi_image_free(sourceImage);

    if(!target.texture)
        printf("FAILED TO LOAD!!!! %s\n", path.c_str());
    return target;
}

StdPicture FrmMain::lazyLoadPicture(std::string path)
{
    StdPicture target;
    if(!GameIsActive)
        return target;

    target.inited = false;
    target.path = path;
    if(target.path.empty())
        return target;

    target.inited = true;
    target.lazyLoaded = true;

    // Check for existing .size file 
    std::string sizePath = path + ".size";
    FILE *fs;
    fs = fopen(sizePath.c_str(), "r");

    char contents[10];
    if(fs != nullptr)
    {
        fread(&contents[0], 1, 10, fs);

        // null terminate for atoi to convert
        contents[4] = '\0';
        contents[9] = '\0';

        target.w = atoi(&contents[0]);
        target.h = atoi(&contents[5]);

        if(fclose(fs)) printf("lazyLoadPicture: Couldn't close file.\n");
    }
    else 
    {
        lazyLoad(target);
        lazyUnLoad(target);
        printf("lazyLoadPicture: Couldn't open size file.\n");
    }

    return target;
}

void FrmMain::loadTexture(StdPicture &target, uint32_t width, uint32_t height, uint8_t *RGBApixels)
{
    // Take our raw bytes and load them in as an OpenGL image.
    GLuint _newTexture = 0;
    glGenTextures(1, &_newTexture);
    glBindTexture(GL_TEXTURE_2D, _newTexture);

    // Nearest neighbor filtering.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        width, height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        RGBApixels
    );

    if(_newTexture != 0)
    {
        target.texture = _newTexture;
        target.w = width;
        target.h = height;
        pLogDebug("VITA: loaded texture with GLuint %d and size %d x %d.", _newTexture, width, height);
    }
    else
    {
        pLogWarning("VITA: loadTexture: _newTexture was 0 after glGenTextures.");
        return;
    }

    num_textures_loaded++;
    m_textureBank.insert(_newTexture);
}

void FrmMain::lazyLoad(StdPicture &target)
{
    if(!target.inited || !target.lazyLoaded || target.texture)
        return;

    // Try and load source image data from disk.
    // EG:
    #if USE_STBI
    stbi_uc* sourceImage;
    int width = 0, height = 0, channels = 0;
    sourceImage = stbi_load(target.path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    #else
    FIBITMAP* sourceImage;
    sourceImage = GraphicsHelps::loadImage(target.path);
    #endif
    

    
    if(!sourceImage)
    {
        printf("[lazyLoad] Failed to load %s. Not implemented or no free memory.\n", target.path.c_str());
        target.inited = false;
        return;
    }

#ifndef USE_STBI
    uint32_t w = static_cast<uint32_t>(FreeImage_GetWidth(sourceImage));
    uint32_t h = static_cast<uint32_t>(FreeImage_GetHeight(sourceImage));
    GLubyte *textura = reinterpret_cast<GLubyte *>(FreeImage_GetBits(sourceImage));
    loadTexture(target, w, h, textura);
#else
    loadTexture(target, width, height, sourceImage);
#endif

    

    num_textures_loaded++;

#if USE_STBI
    stbi_image_free(uc);
#endif

    // TODO: Track free ram space? 
    // TODO: free texture memory every so often?
    // TODO: Why does VitaGL take up so much memory at start?
    // TODO: Do I need to track "big textures" and have them
    // split like the 3DS version?
}

void FrmMain::lazyUnLoad(StdPicture &target)
{
    if(!target.inited || !target.lazyLoaded || !target.texture)
        return;

    deleteTexture(target, true);
}

SDL_Point FrmMain::MapToScr(int x, int y)
{
    return SDL_Point {
        static_cast<int>((static_cast<float>(x) - this->offset_x) / this->viewport_scale_x),
        static_cast<int>((static_cast<float>(y) - this->offset_y) / this->viewport_scale_y)
    };
}

void FrmMain::deleteTexture(StdPicture &tx, bool lazyUnload)
{
    if(!tx.inited)
        return;

    if(tx.texture)
    {
        // num_textures_loaded--;
        if(m_textureBank.find(tx.texture) != m_textureBank.end())
            m_textureBank.erase(tx.texture);

        // Free sprite from memory.
        glDeleteTextures(1, &tx.texture);
        
        // For good measure.
        tx.texture = 0;
    }

    if(!lazyUnload)
    {
        tx.inited = false;
        tx.lazyLoaded = false;
        tx.w = 0;
        tx.h = 0;
        tx.frame_w = 0;
        tx.frame_h = 0;
    }
}

void FrmMain::clearAllTextures()
{
#if DEBUG_BUILD
    int texturesDeleted = 0;
#endif
    for(GLuint tx : m_textureBank)
    {
        if(tx != 0)
        {
            #if DEBUG_BUILD
            texturesDeleted++;
            #endif
            
            glDeleteTextures(1, &tx);
        }
    }

    #if DEBUG_BUILD
    pLogDebug("Cleared %d textures from vram.\n", texturesDeleted);
    #endif

    m_textureBank.clear();
}

void FrmMain::renderRect(int x, int y, int w, int h, float red, float green, float blue, float alpha, bool filled)
{
    // uint32_t clr = C2D_Color32f(red, green, blue, alpha);

    // Filled is always True in this game
    // if (filled)
    //     C2D_DrawRectSolid(x/2+viewport_offset_x,
    //                       y/2+viewport_offset_y,
    //                       0, w/2, h/2, clr);
    // else
    // {
    //     C2D_DrawRectangle(x/2+viewport_offset_x,
    //                       y/2+viewport_offset_y,
    //                       0, 1, h/2, clr, clr, clr, clr);
    //     C2D_DrawRectangle(x/2+viewport_offset_x+w/2-1,
    //                       y/2+viewport_offset_y,
    //                       0, 1, h/2, clr, clr, clr, clr);
    //     C2D_DrawRectangle(x/2+viewport_offset_x,
    //                       y/2+viewport_offset_y,
    //                       0, w/2, 1, clr, clr, clr, clr);
    //     C2D_DrawRectangle(x/2+viewport_offset_x,
    //                       y/2+viewport_offset_y+h/2-1,
    //                       0, w/2, 1, clr, clr, clr, clr);
    // }
}

void FrmMain::renderRectBR(int _left, int _top, int _right, int _bottom, float red, float green, float blue, float alpha)
{
    renderRect(_left, _top, _right-_left, _bottom-_top, red, green, blue, alpha, true);
}

void FrmMain::renderTexturePrivate(float xDst, float yDst, float wDst, float hDst,
                             StdPicture &tx,
                             float xSrc, float ySrc, float wSrc, float hSrc,
                             float rotateAngle, SDL_Point *center, unsigned int flip,
                             float red, float green, float blue, float alpha)
{
    // This is mostly lifted from the 3DS version so thank you, ds-sloth <3


    if(!tx.inited)
        return;

    if(!tx.texture && tx.lazyLoaded)
        lazyLoad(tx);

    // tx.lastDrawFrame = currentFrame;

    if(!tx.texture)
        return;
    if(xDst > this->viewport_w || yDst > this->viewport_h)
        return;

    unsigned int mode = 0;
    while(ySrc >= tx.h / 2 && mode < 3)
    {
        ySrc -= tx.h / 2;
        mode += 1;
    }
    flip ^= mode;

    // texture boundaries
    // this never happens unless there was an invalid input
    // if((xSrc < 0.f) || (ySrc < 0.f)) return;

    // TODO: graphics tests for how offscreen draws interact with flips
    //       handling rotations properly is probably impossible
    if(xDst < 0.f)
    {
        if(!(flip & SDL_FLIP_HORIZONTAL))
            xSrc -= xDst * wSrc/wDst;
        if (wDst+xDst > viewport_w)
        {
            if(flip & SDL_FLIP_HORIZONTAL)
                xSrc += (wDst+xDst-viewport_w) * wSrc/wDst;
            wSrc = viewport_w * wSrc/wDst;
            wDst = viewport_w;
        }
        else
        {
            wSrc += xDst * wSrc/wDst;
            wDst += xDst;
        }
        xDst = 0.f;
    }
    else if(xDst + wDst > viewport_w)
    {
        if(flip & SDL_FLIP_HORIZONTAL)
            xSrc += (wDst+xDst-viewport_w) * wSrc/wDst;
        wSrc = (viewport_w - xDst) * wSrc/wDst;
        wDst = (viewport_w - xDst);
    }
    if(yDst < 0.f)
    {
        if(!(flip & SDL_FLIP_VERTICAL))
            ySrc -= yDst * hSrc/hDst;
        if (hDst+yDst > viewport_h)
        {
            if(flip & SDL_FLIP_VERTICAL)
                ySrc += (hDst+yDst-viewport_h) * hSrc/hDst;
            hSrc = viewport_h * hSrc/hDst;
            hDst = viewport_h;
        }
        else
        {
            hSrc += yDst * hSrc/hDst;
            hDst += yDst;
        }
        yDst = 0.f;
    }
    else if(yDst + hDst > viewport_h)
    {
        if(flip & SDL_FLIP_VERTICAL)
            ySrc += (hDst+yDst-viewport_h) * hSrc/hDst;
        hSrc = (viewport_h - yDst) * hSrc/hDst;
        hDst = (viewport_h - yDst);
    }

    // figure out GLuint textures to draw.
    GLuint to_draw = 0;
    // TODO: Should I be rendering large images in multiple parts?

    // Don't go more than size of texture
    // Failure conditions should only happen if texture is smaller than expected
    if(xSrc + wSrc > tx.w/2)
    {
        wDst = (tx.w/2 - xSrc) * wDst/wSrc;
        wSrc = tx.w/2 - xSrc;
        if(wDst < 0.f)
            return;
    }
    if(ySrc + hSrc > tx.h/2)
    {
        hDst = (tx.h/2 - ySrc) * hDst/hSrc;
        hSrc = tx.h/2 - ySrc;
        if(hDst < 0.f)
            return;
    }

    if(tx.texture) to_draw = tx.texture;

    // VitaGL_DrawImage_Custom code here.
    // Todo also in the future, rotation ?!? !

}

inline int ROUNDDIV2(int x)
{
    return (x<0)?(x-1)/2:x/2;
}

inline float ROUNDDIV2(float x)
{
    return std::nearbyintf(std::roundf(x)/2.f);
}

inline float ROUNDDIV2(double x)
{
    return std::nearbyintf(std::roundf((float)x)/2.f);
}

inline float FLOORDIV2(float x)
{
    return std::floor(x/2.f);
}

void FrmMain::renderTextureI(int xDst, int yDst, int wDst, int hDst,
                        StdPicture &tx,
                        int xSrc, int ySrc,
                        double rotateAngle, SDL_Point *center, unsigned int flip,
                        float red, float green, float blue, float alpha)
{
    int w = wDst/2;
    int h = hDst/2;
    renderTexturePrivate(
        ROUNDDIV2(xDst), ROUNDDIV2(yDst), w, h,
        tx,
        xSrc/2, ySrc/2, w, h,
        rotateAngle, center, flip,
        red, green, blue, alpha);
}

void FrmMain::renderTextureScaleI(int xDst, int yDst, int wDst, int hDst,
                             StdPicture &tx,
                             int xSrc, int ySrc,
                             int wSrc, int hSrc,
                             double rotateAngle, SDL_Point *center, unsigned int flip,
                             float red, float green, float blue, float alpha)
{
    renderTexturePrivate(
        ROUNDDIV2(xDst), ROUNDDIV2(yDst), wDst/2, hDst/2,
        tx,
        xSrc/2, ySrc/2, wSrc/2, hSrc/2,
        rotateAngle, center, flip,
        red, green, blue, alpha);
}

// public draw methods

void FrmMain::renderTextureScale(double xDst, double yDst, double wDst, double hDst,
                            StdPicture &tx,
                            int xSrc, int ySrc, int wSrc, int hSrc,
                            float red, float green, float blue, float alpha)
{
    renderTexturePrivate(
        ROUNDDIV2(xDst), ROUNDDIV2(yDst), ROUNDDIV2(wDst), ROUNDDIV2(hDst),
        tx,
        xSrc/2, ySrc/2, wSrc/2, hSrc/2,
        0.f, nullptr, SDL_FLIP_NONE,
        red, green, blue, alpha);
}

void FrmMain::renderTexture(double xDst, double yDst, double wDst, double hDst,
                            StdPicture &tx,
                            int xSrc, int ySrc,
                            float red = 1.f, float green, float blue, float alpha)
{
    float w = ROUNDDIV2(wDst);
    float h = ROUNDDIV2(hDst);
    renderTexturePrivate(
        ROUNDDIV2(xDst), ROUNDDIV2(yDst), w, h,
        tx,
        xSrc/2, ySrc/2, w, h,
        0.f, nullptr, SDL_FLIP_NONE,
        red, green, blue, alpha);
}

void FrmMain::renderTexture(int xDst, int yDst, StdPicture &tx, float red, float green, float blue, float alpha)
{
    int w = tx.w/2;
    int h = tx.h/2;
    renderTexturePrivate(
        ROUNDDIV2(xDst), ROUNDDIV2(yDst), w, h,
        tx,
        0.f, 0.f, w, h,
        0.f, nullptr, SDL_FLIP_NONE,
        red, green, blue, alpha);
}

void FrmMain::renderTextureScale(int xDst, int yDst, int wDst, int hDst, StdPicture &tx, float red, float green, float blue, float alpha)
{
    renderTexturePrivate(
        ROUNDDIV2(xDst), ROUNDDIV2(yDst), tx.w/2, tx.h/2,
        tx,
        0.f, 0.f, wDst/2, hDst/2,
        0.f, nullptr, SDL_FLIP_NONE,
        red, green, blue, alpha);
}

///// MORE BORING, FAIRLY CROSS IMPLEMENTATION DEPENDENT STUFF HERE
///// MORE BORING, FAIRLY CROSS IMPLEMENTATION DEPENDENT STUFF HERE
///// MORE BORING, FAIRLY CROSS IMPLEMENTATION DEPENDENT STUFF HERE

size_t FrmMain::lazyLoadedBytes()
{
    return 0;
}

void FrmMain::lazyLoadedBytesReset()
{
}

void FrmMain::clearBuffer()
{
    // Clear the render buffer texture
    return;
}

void FrmMain::updateViewport()
{
    resetViewport();
    offsetViewport(0, 0);
}

void FrmMain::resetViewport()
{
    setViewport(0, 0, ScreenW, ScreenH);
}

void FrmMain::setViewport(int x, int y, int w, int h)
{
    int offset_x = viewport_offset_x - viewport_x;
    int offset_y = viewport_offset_y - viewport_y;
    viewport_x = x/2;
    viewport_y = y/2;
    viewport_w = w/2;
    viewport_h = h/2;
    viewport_offset_x = viewport_x + offset_x;
    viewport_offset_y = viewport_y + offset_y;
}

void FrmMain::offsetViewport(int x, int y)
{
    viewport_offset_x = viewport_x+x/2;
    viewport_offset_y = viewport_y+y/2;
}

SDL_Window *FrmMain::getWindow()
{
    return m_window;
}

Uint8 FrmMain::getKeyState(SDL_Scancode key)
{
    if(m_keyboardState)
        return m_keyboardState[key];
    return 0;
}


/////// SDL REIMPLEMENTATION KEY EVENTS
void FrmMain::eventKeyDown(SDL_KeyboardEvent &evt)
{
    int KeyCode = evt.keysym.scancode;
    inputKey = KeyCode;

    // bool ctrlF = ((evt.keysym.mod & KMOD_CTRL) != 0 && evt.keysym.scancode == SDL_SCANCODE_F);
    // bool altEnter = ((evt.keysym.mod & KMOD_ALT) != 0 && (evt.keysym.scancode == SDL_SCANCODE_RETURN || evt.keysym.scancode == SDL_SCANCODE_KP_ENTER));
}

void FrmMain::eventDoubleClick()
{
    return;
}

void FrmMain::eventKeyPress(SDL_Scancode KeyASCII)
{
    switch(KeyASCII)
    {
    case SDL_SCANCODE_A: CheatCode('a'); break;
    case SDL_SCANCODE_B: CheatCode('b'); break;
    case SDL_SCANCODE_C: CheatCode('c'); break;
    case SDL_SCANCODE_D: CheatCode('d'); break;
    case SDL_SCANCODE_E: CheatCode('e'); break;
    case SDL_SCANCODE_F: CheatCode('f'); break;
    case SDL_SCANCODE_G: CheatCode('g'); break;
    case SDL_SCANCODE_H: CheatCode('h'); break;
    case SDL_SCANCODE_I: CheatCode('i'); break;
    case SDL_SCANCODE_J: CheatCode('j'); break;
    case SDL_SCANCODE_K: CheatCode('k'); break;
    case SDL_SCANCODE_L: CheatCode('l'); break;
    case SDL_SCANCODE_M: CheatCode('m'); break;
    case SDL_SCANCODE_N: CheatCode('n'); break;
    case SDL_SCANCODE_O: CheatCode('o'); break;
    case SDL_SCANCODE_P: CheatCode('p'); break;
    case SDL_SCANCODE_Q: CheatCode('q'); break;
    case SDL_SCANCODE_R: CheatCode('r'); break;
    case SDL_SCANCODE_S: CheatCode('s'); break;
    case SDL_SCANCODE_T: CheatCode('t'); break;
    case SDL_SCANCODE_U: CheatCode('u'); break;
    case SDL_SCANCODE_V: CheatCode('v'); break;
    case SDL_SCANCODE_W: CheatCode('w'); break;
    case SDL_SCANCODE_X: CheatCode('x'); break;
    case SDL_SCANCODE_Y: CheatCode('y'); break;
    case SDL_SCANCODE_Z: CheatCode('z'); break;
    case SDL_SCANCODE_1: CheatCode('1'); break;
    case SDL_SCANCODE_2: CheatCode('2'); break;
    case SDL_SCANCODE_3: CheatCode('3'); break;
    case SDL_SCANCODE_4: CheatCode('4'); break;
    case SDL_SCANCODE_5: CheatCode('5'); break;
    case SDL_SCANCODE_6: CheatCode('6'); break;
    case SDL_SCANCODE_7: CheatCode('7'); break;
    case SDL_SCANCODE_8: CheatCode('8'); break;
    case SDL_SCANCODE_9: CheatCode('9'); break;
    case SDL_SCANCODE_0: CheatCode('0'); break;
    default: CheatCode(' '); break;
    }
}

void FrmMain::eventKeyUp(SDL_KeyboardEvent &evt)
{
    UNUSED(evt);
}

void FrmMain::eventMouseDown(SDL_MouseButtonEvent &event)
{
    if(event.button == SDL_BUTTON_LEFT)
    {
        MenuMouseDown = true;
        MenuMouseMove = true;
        if(LevelEditor || MagicHand || TestLevel)
            EditorControls.MouseClick = true;
    }
    else if(event.button == SDL_BUTTON_RIGHT)
    {
        MenuMouseBack = true;
        if(LevelEditor || MagicHand || TestLevel)
        {
            optCursor.current = OptCursor_t::LVL_SELECT;
            MouseMove(float(MenuMouseX), float(MenuMouseY));
            SetCursor();
        }
    }
    else if(event.button == SDL_BUTTON_MIDDLE)
    {
        if(LevelEditor || MagicHand || TestLevel)
        {
            optCursor.current = OptCursor_t::LVL_ERASER;
            MouseMove(float(MenuMouseX), float(MenuMouseY));
            SetCursor();
        }
    }
}

void FrmMain::eventMouseMove(SDL_MouseMotionEvent &event)
{
    SDL_Point p = MapToScr(event.x, event.y);
    MenuMouseX = p.x;// int(event.x * ScreenW / ScaleWidth);
    MenuMouseY = p.y;//int(event.y * ScreenH / ScaleHeight);
    MenuMouseMove = true;
    if(LevelEditor || MagicHand || TestLevel)
    {
        EditorCursor.X = p.x;
        EditorCursor.Y = p.y;
        MouseMove(EditorCursor.X, EditorCursor.Y, true);
        MouseRelease = true;
    }
}

void FrmMain::eventMouseUp(SDL_MouseButtonEvent &event)
{
    bool doubleClick = false;
    MenuMouseDown = false;
    MenuMouseRelease = true;
    if(LevelEditor || MagicHand || TestLevel)
        EditorControls.MouseClick = false;

    if(event.button == SDL_BUTTON_LEFT)
    {
        doubleClick = (m_lastMousePress + 300) >= SDL_GetTicks();
        m_lastMousePress = SDL_GetTicks();
    }

    if(doubleClick)
    {
        eventDoubleClick();
        m_lastMousePress = 0;
    }
}
///////