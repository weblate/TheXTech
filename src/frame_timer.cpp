/*
 * TheXTech - A platform game engine ported from old source code for VB6
 *
 * Copyright (c) 2009-2011 Andrew Spinks, original VB6 code
 * Copyright (c) 2020-2021 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <SDL2/SDL_timer.h>

#include <fmt_format_ne.h>
#include <Logger/logger.h>
#include "pge_delay.h"

#include "frame_timer.h"
#include "globals.h"
#include "graphics.h"


PerformanceStats_t g_stats;

void PerformanceStats_t::reset()
{
   renderedBlocks = 0;
   renderedSzBlocks = 0;
   renderedBGOs = 0;
   renderedNPCs = 0;
   renderedEffects = 0;

   checkedBlocks = 0;
   checkedSzBlocks = 0;
   checkedBGOs = 0;
   checkedNPCs = 0;
   checkedEffects = 0;

   renderedTiles = 0;
   renderedScenes = 0;
   renderedPaths = 0;
   renderedLevels = 0;

   checkedTiles = 0;
   checkedScenes = 0;
   checkedPaths = 0;
   checkedLevels = 0;

   physScannedBlocks = 0;
   physScannedBGOs = 0;
   physScannedNPCs = 0;
}

void PerformanceStats_t::print()
{
    if(!enabled)
        return;

    if(LevelSelect && !GameMenu)
    {
        SuperPrint(fmt::sprintf_ne("DRAW: T=%03d S=%03d P=%03d L=%03d, SUM=%03d",
                                   renderedTiles, renderedScenes, renderedPaths, renderedLevels,
                                   (renderedTiles + renderedScenes + renderedPaths + renderedLevels)),
                   3, 45, 8);
        SuperPrint(fmt::sprintf_ne("CHEK: T=%03d S=%03d P=%03d L=%03d, SUM=%03d",
                                   checkedTiles, checkedScenes, checkedPaths, checkedLevels,
                                   (checkedTiles + checkedScenes + checkedPaths + checkedLevels)),
                   3, 45, 26);
    }
    else
    {
        frmMain.renderRect(42, 6, 745, 72, 0.0f,0.0f, 0.0f, 0.3f, true);
        SuperPrint(fmt::sprintf_ne("DRAW: B=%05d Z=%04d G=%04d N=%04d, E=%03d",
                                   renderedBlocks, renderedSzBlocks, renderedBGOs, renderedNPCs, renderedEffects,
                                   (renderedBlocks + renderedSzBlocks + renderedBGOs + renderedNPCs + renderedEffects)),
                   3, 45, 8, 0.5f, 1.f, 1.f);
        SuperPrint(fmt::sprintf_ne("DRAW: SUMM=%d", (renderedBlocks + renderedSzBlocks + renderedBGOs + renderedNPCs + renderedEffects)),
                   3, 45, 26, 0.5f, 1.f, 1.f);
        SuperPrint(fmt::sprintf_ne("CHEK: B=%05d Z=%04d G=%04d N=%04d, E=%03d",
                                   checkedBlocks, checkedSzBlocks, checkedBGOs, checkedNPCs, checkedEffects),
                   3, 45, 44, 0.5f, 1.f, 1.f);
        SuperPrint(fmt::sprintf_ne("CHEK: SUMM=%d", (checkedBlocks + checkedSzBlocks+ checkedBGOs + checkedNPCs + checkedEffects)),
                   3, 45, 62, 0.5f, 1.f, 1.f);
        // WIP
//        SuperPrint(fmt::sprintf_ne("PHYS: B%03d G%03d N%03d, S:%03d",
//                                   physScannedBlocks, physScannedBGOs, physScannedNPCs,
//                                   (physScannedBlocks + physScannedBGOs + physScannedNPCs)),
//                   3, 45, 44);
    }

    if(GameMenu)
    {
        SuperPrint(fmt::sprintf_ne("MENU-MODE: %d", MenuMode),
                   3, 45, 70, 0.5f, 1.f, 1.f);
    }
}

//#if !defined(__EMSCRIPTEN__)
#define USE_NEW_TIMER
#define USE_NEW_FRAMESKIP
//#endif

#ifdef USE_NEW_TIMER
#define COMPUTE_FRAME_TIME_1_REAL computeFrameTime1Real_2
#define COMPUTE_FRAME_TIME_2_REAL computeFrameTime2Real_2
#else
#define COMPUTE_FRAME_TIME_1_REAL computeFrameTime1Real
#define COMPUTE_FRAME_TIME_2_REAL computeFrameTime2Real
#endif


#ifdef USE_NEW_TIMER

#define ONE_MILLIARD 1000000000

typedef int64_t nanotime_t;

static SDL_INLINE nanotime_t getNanoTime()
{
    return static_cast<nanotime_t>(SDL_GetTicks()) * 1000000;
}

static SDL_INLINE nanotime_t getElapsedTime(nanotime_t oldTime)
{
    return getNanoTime() - oldTime;
}

static SDL_INLINE nanotime_t getSleepTime(nanotime_t oldTime, nanotime_t target)
{
    return target - getElapsedTime(oldTime);
}

static SDL_INLINE void xtech_nanosleep(nanotime_t sleepTime)
{
    if(sleepTime <= 0)
        return;
    PGE_Delay((Uint32)SDL_ceil(sleepTime / 1000000.0));
}


struct TimeStore
{
    const size_t size = 4;
    size_t       pos = 0;
    nanotime_t   sum = 0;
    nanotime_t   items[4] = {0, 0, 0, 0};

    void add(nanotime_t item)
    {
        sum -= items[pos];
        sum += item;
        items[pos] = item;
        pos = (pos + 1) % size;
    }

    nanotime_t average()
    {
        return sum / size;
    }
};

static TimeStore         s_overheadTimes;
static const  nanotime_t c_frameRateNano = 1000000000.0 / 64.1025;
static nanotime_t        s_oldTime = 0,
                         s_overhead = 0;
#ifdef USE_NEW_FRAMESKIP
static nanotime_t        s_startProcessing = 0;
static nanotime_t        s_stopProcessing = 0;
static nanotime_t        s_doUpdate = 0;
#endif

#endif
// ----------------------------------------------------

//Public Const frameRate As Double = 15 'for controlling game speed
//const int frameRate = 15;
static const  double c_frameRate = 15.0;

static double s_overTime = 0;
static double s_goalTime = 0;
static double s_fpsCount = 0.0;
static double s_fpsTime = 0.0;
static int    s_cycleCount = 0;
static double s_gameTime = 0.0;
static double s_currentTicks = 0.0;


void resetFrameTimer()
{
    s_overTime = 0;
    s_fpsCount = 0;
    s_fpsTime = 0;
    s_cycleCount = 0;
    s_gameTime = 0;
#ifdef USE_NEW_FRAMESKIP
    s_doUpdate = 0;
    s_goalTime = 0;
#else
    s_goalTime = SDL_GetTicks() + 1000;
#endif
    D_pLogDebugNA("Time counter reset was called");
}

void resetTimeBuffer()
{
    s_currentTicks = 0;
}

#ifdef USE_NEW_FRAMESKIP

bool frameSkipNeeded()
{
    return s_doUpdate > 0;
}

#else

bool frameSkipNeeded() // Old and buggy Redigit's
{
    return SDL_GetTicks() + SDL_floor(1000 * (1 - (s_cycleCount / 63.0))) > s_goalTime;
}

#endif

void frameNextInc()
{
    s_fpsCount += 1;
}

void cycleNextInc()
{
    s_cycleCount++;
}

extern void CheckActive(); // game_main.cpp

static SDL_INLINE bool canProcessFrameCond()
{
    bool ret = s_currentTicks >= s_gameTime + c_frameRate || s_currentTicks < s_gameTime || MaxFPS;
#ifdef USE_NEW_FRAMESKIP
    if(ret && s_doUpdate <= 0)
        s_startProcessing = getNanoTime();
#endif
    return ret;
}

bool canProceedFrame()
{
    s_currentTicks = SDL_GetTicks();
    return canProcessFrameCond();
}

#ifndef USE_NEW_TIMER
static SDL_INLINE void computeFrameTime1Real()
{
    if(s_fpsCount >= 32000)
        s_fpsCount = 0; // Fixes Overflow bug

    if(s_cycleCount >= 32000)
        s_cycleCount = 0; // Fixes Overflow bug

    s_overTime += (s_currentTicks - (s_gameTime + c_frameRate));

    if(s_gameTime == 0.0)
        s_overTime = 0;

    if(s_overTime <= 1)
        s_overTime = 0;
    else if(s_overTime > 1000)
        s_overTime = 1000;

    s_gameTime = s_currentTicks - s_overTime;
    s_overTime -= (s_currentTicks - s_gameTime);
}

static SDL_INLINE void computeFrameTime2Real()
{
    if(SDL_GetTicks() > s_fpsTime)
    {
        if(s_cycleCount >= 65)
        {
            s_overTime = 0;
            s_gameTime = s_currentTicks;
        }
        s_cycleCount = 0;
        s_fpsTime = SDL_GetTicks() + 1000;
        s_goalTime = s_fpsTime;
        //      if(Debugger == true)
        //          frmLevelDebugger.lblFPS = fpsCount;
        if(ShowFPS)
            PrintFPS = s_fpsCount;
        s_fpsCount = 0;
    }
}
#endif


#ifdef USE_NEW_TIMER
static SDL_INLINE void computeFrameTime1Real_2()
{
    if(s_fpsCount >= 32000)
        s_fpsCount = 0; // Fixes Overflow bug

    if(s_cycleCount >= 32000)
        s_cycleCount = 0; // Fixes Overflow bug
}

static SDL_INLINE void computeFrameTime2Real_2()
{
#ifdef USE_NEW_FRAMESKIP
    if(s_doUpdate > 0)
        s_doUpdate -= c_frameRateNano;
    s_startProcessing = 0;
    s_stopProcessing = 0;
#endif

    if(SDL_GetTicks() > s_fpsTime)
    {
        if(s_cycleCount >= 65)
        {
            s_overTime = 0;
            s_gameTime = s_currentTicks;
        }
        s_cycleCount = 0;
        s_fpsTime = SDL_GetTicks() + 1000;
        s_goalTime = s_fpsTime;

        if(ShowFPS)
            PrintFPS = s_fpsCount;
        s_fpsCount = 0;
    }

    if(!MaxFPS)
    {
        nanotime_t start = getNanoTime();
        nanotime_t sleepTime = getSleepTime(s_oldTime, c_frameRateNano);
        s_overhead = s_overheadTimes.average();

        // D_pLogDebug("ST=%lld, OH=%lld", sleepTime, s_overhead);
        if(sleepTime > s_overhead)
        {
            nanotime_t adjustedSleepTime = sleepTime - s_overhead;
            xtech_nanosleep(adjustedSleepTime);
            auto e = getElapsedTime(start);
            nanotime_t overslept = e - adjustedSleepTime;
            // SDL_assert(overslept >= 0);
            if(overslept < 0)
                s_overheadTimes.add(0);
            else if(overslept < c_frameRateNano)
                s_overheadTimes.add(overslept);
        }
    }

    s_oldTime = getNanoTime();
}
#endif


void computeFrameTime1()
{
    COMPUTE_FRAME_TIME_1_REAL();
}

void computeFrameTime2()
{
    COMPUTE_FRAME_TIME_2_REAL();
}

void runFrameLoop(LoopCall_t doLoopCallbackPre,
                  LoopCall_t doLoopCallbackPost,
                  std::function<bool(void)> condition,
                  std::function<bool ()> subCondition,
                  std::function<void ()> preTimerExtraPre,
                  std::function<void ()> preTimerExtraPost)
{
    do
    {
        if(preTimerExtraPre)
            preTimerExtraPre();

        DoEvents();
        s_currentTicks = SDL_GetTicks();

        if(preTimerExtraPost)
            preTimerExtraPost();

        if(canProcessFrameCond())
        {
            CheckActive();
            if(doLoopCallbackPre)
                doLoopCallbackPre();

            COMPUTE_FRAME_TIME_1_REAL();

            if(doLoopCallbackPost)
                doLoopCallbackPost(); // Run the loop callback
            DoEvents();

            COMPUTE_FRAME_TIME_2_REAL();

            if(subCondition && subCondition())
                break;
        }

        PGE_Delay(1);
        if(!GameIsActive)
            break;// Break on quit
    }
    while(condition());
}

void frameRenderStart()
{
#ifdef USE_NEW_FRAMESKIP
    if(s_doUpdate <= 0)
        s_startProcessing = getNanoTime();
#endif
}

void frameRenderEnd()
{
#ifdef USE_NEW_FRAMESKIP
    if(s_doUpdate <= 0)
    {
        s_stopProcessing = getNanoTime();
        nanotime_t newTime = FrameSkip ? (s_stopProcessing - s_startProcessing) : 0;
        // D_pLogDebug("newTime/nano=%lld (%lld)", newTime/c_frameRateNano, newTime / 1000000);
        if(newTime > c_frameRateNano * 25) // Limit 25 frames being skipped maximum
        {
            D_pLogDebug("Overloading detected: %lld frames to skip (%lld milliseconds delay)", newTime/c_frameRateNano, newTime / 1000000);
            newTime = c_frameRateNano * 25;
        }
        s_doUpdate += newTime;
        s_goalTime = SDL_GetTicks() + (newTime / 1000000);
    }
#endif
}
