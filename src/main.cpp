/*
 * TheXTech - A platform game engine ported from old source code for VB6
 *
 * Copyright (c) 2009-2011 Andrew Spinks, original VB6 code
 * Copyright (c) 2020-2021 Vitaly Novichkov <admin@wohlnet.ru>
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

#include <SDL2/SDL.h>

#include "game_main.h"
#include "main/game_info.h"
#include "compat.h"
#include <AppPath/app_path.h>
#include <tclap/CmdLine.h>
#include <Utils/strings.h>
#include <Utils/files.h>
#include <CrashHandler/crash_handler.h>

#ifdef ENABLE_XTECH_LUA
#include "xtech_lua_main.h"
#endif

#ifdef __APPLE__
#include <Utils/files.h>
#include <Logger/logger.h>

static std::string g_fileToOpen;
/**
 * @brief Receive an opened file from the Finder (Must be created at least one window!)
 */
static void macosReceiveOpenFile()
{
    if(g_fileToOpen.empty())
    {
        pLogDebug("Attempt to take Finder args...");
        SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
        SDL_Event event;
        while(SDL_PollEvent(&event))
        {
            if(event.type == SDL_DROPFILE)
            {
                std::string file(event.drop.file);
                if(Files::fileExists(file))
                {
                    g_fileToOpen = file;
                    pLogDebug("Got file path: [%s]", file.c_str());
                }
                else
                    pLogWarning("Invalid file path, sent by Mac OS X Finder event: [%s]", file.c_str());
            }
        }
        SDL_EventState(SDL_DROPFILE, SDL_DISABLE);
    }
}
#endif

static void strToPlayerSetup(int player, const std::string &setupString)
{
    if(setupString.empty())
        return; // Do nothing

    std::vector<std::string> keys;

    auto &p = testPlayer[player];

    Strings::split(keys, setupString, ";");
    for(auto &k : keys)
    {
        if(k.empty())
            continue;
        if(k[0] == 'c') // Character
            p.Character = int(SDL_strtol(k.substr(1).c_str(), nullptr, 10));
        else if(k[0] == 's') // State
            p.State = int(SDL_strtol(k.substr(1).c_str(), nullptr, 10));
        else if(k[0] == 'm') // Mounts
            p.Mount = int(SDL_strtol(k.substr(1).c_str(), nullptr, 10));
        else if(k[0] == 't') // Mount types
            p.MountType = int(SDL_strtol(k.substr(1).c_str(), nullptr, 10));
    }

    if(p.Character < 1)
        p.Character = 1;
    if(p.Character > 5)
        p.Character = 5;

    if(p.State < 1)
        p.State = 1;
    if(p.State > 7)
        p.State = 7;


    switch(p.Mount)
    {
    default:
    case 0:
        p.Mount = 0;
        p.MountType = 0;
        break; // Rejected aliens
    case 1: case 2: case 3:
        break; // Allowed
    }

    switch(p.Mount)
    {
    case 1:
        if(p.MountType < 1 || p.MountType > 3) // Socks
            p.MountType = 1;
        break;
    default:
        break;
    case 3:
        if(p.MountType < 1 || p.MountType > 8) // Cat Llamas
            p.MountType = 1;
        break;
    }
}

extern "C"
int main(int argc, char**argv)
{
    CmdLineSetup_t setup;

    CrashHandler::initSigs();

    AppPathManager::initAppPath();
    AppPath = AppPathManager::assetsRoot();

    OpenConfig_preSetup();

    setup.renderType = CmdLineSetup_t::RenderType(RenderMode);

    testPlayer.fill(Player_t());

    try
    {
        // Define the command line object.
        TCLAP::CmdLine  cmd("TheXTech\n"
                            "Copyright (c) 2020-2021 Vitaly Novichkov <admin@wohlnet.ru>\n"
                            "This program is distributed under the MIT license\n", ' ', "1.3");

        TCLAP::ValueArg<std::string> customAssetsPath("c", "assets-root", "Specify the different assets root directory to play",
                                                      false, "",
                                                      "directory path",
                                                      cmd);


        TCLAP::SwitchArg switchFrameSkip("f", "frameskip", "Enable frame skipping mode", false);
        TCLAP::SwitchArg switchNoSound("s", "no-sound", "Disable sound", false);
        TCLAP::SwitchArg switchNoPause("p", "never-pause", "Never pause game when window losts a focus", false);
        TCLAP::ValueArg<std::string> renderType("r", "render", "Sets the graphics mode:\n"
                                                "       sw - software render (fallback)\n"
                                                "       hw - hardware accelerated render [Default]\n"
                                                "       vsync - hardware accelerated with the v-sync enabled",
                                                false, "",
                                                "render type",
                                                cmd);

        TCLAP::ValueArg<std::string> testLevel("l", "leveltest", "Start a level test of a given level file. OBSOLETE OPTION: now you able to specify the file path without -l or --leveltest argument.",
                                                false, "",
                                                "file path",
                                                cmd);

        TCLAP::ValueArg<unsigned int> numPlayers("n", "num-players", "Count of players",
                                                    false, 1u,
                                                   "number 1 or 2",
                                                   cmd);

        TCLAP::SwitchArg switchBattleMode("b", "battle", "Test level in battle mode", false);

        TCLAP::ValueArg<std::string> playerCharacter1("1",
                                                      "player1",
                                                      "Setup of playable character for player 1:\n"
                                                      "       Semicolon separated key-argument values:\n"
                                                      "       c - character, s - state, m - mount, t - mount type. Example:\n"
                                                      "       c1;s2;m3;t0 - Character as 1, State as 2, Mount as 3, Mount type as 0",
                                                      false, "",
                                                      "c1;s2;m0;t0",
                                                      cmd);

        TCLAP::ValueArg<std::string> playerCharacter2("2",
                                                      "player2",
                                                      "Setup of playable character for player 2:\n"
                                                      "       Semicolon separated key-argument values:\n"
                                                      "       c - character, s - state, m - mount, t - mount type. Example:\n"
                                                      "       c1;s2;m3;t0 - Character as 1, State as 2, Mount as 3, Mount type as 0",
                                                      false, "",
                                                      "c1;s2;m0;t0",
                                                      cmd);

        TCLAP::SwitchArg switchTestGodMode("g", "god-mode", "Enable god mode in level testing", false);
        TCLAP::SwitchArg switchTestGrabAll("a", "grab-all", "Enable ability to grab everything while level testing", false);
        TCLAP::SwitchArg switchTestShowFPS("m", "show-fps", "Show FPS counter on the screen", false);
        TCLAP::SwitchArg switchTestMaxFPS("x", "max-fps", "Run FPS as fast as possible", false);
        TCLAP::SwitchArg switchTestMagicHand("k", "magic-hand", "Enable magic hand functionality while level test running", false);
        TCLAP::SwitchArg switchTestInterprocess("i", "interprocessing", "Enable an interprocessing mode with Editor", false);

        TCLAP::ValueArg<std::string> compatLevel(std::string(), "compat-level",
                                                   "Enforce the specific gameplay compatibiltiy level. Supported values:\n"
                                                   "       modern - TheXTech native, all features and bugfixes enabled [Default]\n"
                                                   "       smbx2  - Disables all features and bugfixes except fixed at SMBX2\n"
                                                   "       smbx13 - Enforces the full compatibility with the SMBX 1.3 behaviour\n"
                                                   "       Note: If speed-run mode is set, the compatibility level will be overriden by the speed-run mode",
                                                    false, "modern",
                                                   "modern, smbx2, smbx3",
                                                   cmd);
        TCLAP::ValueArg<unsigned int> speedRunMode(std::string(), "speed-run-mode",
                                                   "Enable the speed-runer mode: the playthrough timer will be shown, "
                                                   "and some gameplay limitations will be enabled. Supported values:\n"
                                                   "       0 - Disabled [Default]\n"
                                                   "       1 - TheXTech native\n"
                                                   "       2 - Disable time-winning updates (SMBX2 mode)\n"
                                                   "       3 - Strict vanilla SMBX 1.3, enable all bugs",
                                                    false, 0u,
                                                   "0, 1, 2, or 3",
                                                   cmd);
        TCLAP::SwitchArg switchSpeedRunSemiTransparent(std::string(), "speed-run-semitransparent",
                                                       "Make the speed-runner mode timer be drawn transparently", false);

        TCLAP::SwitchArg switchVerboseLog(std::string(), "verbose", "Enable log output into the terminal", false);

        TCLAP::UnlabeledValueArg<std::string> inputFileNames("levelpath", "Path to level file to run the test", false, std::string(), "path to file");

        cmd.add(&switchFrameSkip);
        cmd.add(&switchNoSound);
        cmd.add(&switchNoPause);
        cmd.add(&switchBattleMode);

        cmd.add(&switchTestGodMode);
        cmd.add(&switchTestGrabAll);
        cmd.add(&switchTestShowFPS);
        cmd.add(&switchTestMaxFPS);
        cmd.add(&switchTestMagicHand);
        cmd.add(&switchTestInterprocess);
        cmd.add(&switchVerboseLog);
        cmd.add(&switchSpeedRunSemiTransparent);
        cmd.add(&inputFileNames);

        cmd.parse(argc, argv);

        std::string customAssets = customAssetsPath.getValue();

        if(!customAssets.empty())
        {
            AppPathManager::setAssetsRoot(customAssets);
            AppPath = AppPathManager::assetsRoot();
        }

        setup.frameSkip = switchFrameSkip.getValue();
        setup.noSound   = switchNoSound.getValue();
        setup.neverPause = switchNoPause.getValue();

        std::string rt = renderType.getValue();
        if(rt == "sw")
            setup.renderType = CmdLineSetup_t::RENDER_SW;
        else if(rt == "vsync")
            setup.renderType = CmdLineSetup_t::RENDER_VSYNC;
        else if(rt == "hw")
            setup.renderType = CmdLineSetup_t::RENDER_HW;

        setup.testLevel = testLevel.getValue();

        if(!inputFileNames.getValue().empty())
        {
            auto fpath = inputFileNames.getValue();
            if(Files::hasSuffix(fpath, ".lvl") || Files::hasSuffix(fpath, ".lvlx"))
            {
                setup.testLevel = fpath;
            }

            //TODO: Implement a world map running and testing
//            if(Files::hasSuffix(fpath, ".wld") || Files::hasSuffix(fpath, ".wldx"))
//            {
//
//            }
        }

        setup.verboseLogging = switchVerboseLog.getValue();
        setup.interprocess = switchTestInterprocess.getValue();
        setup.testLevelMode = !setup.testLevel.empty() || setup.interprocess;
        setup.testNumPlayers = int(numPlayers.getValue());
        if(setup.testNumPlayers > 2)
            setup.testNumPlayers = 2;
        setup.testBattleMode = switchBattleMode.getValue();
        if(setup.testLevelMode)
        {
            strToPlayerSetup(1, playerCharacter1.getValue());
            strToPlayerSetup(2, playerCharacter2.getValue());
        }

        setup.testGodMode = switchTestGodMode.getValue();
        setup.testGrabAll = switchTestGrabAll.getValue();
        setup.testShowFPS = switchTestShowFPS.getValue();
        setup.testMaxFPS = switchTestMaxFPS.getValue();
        setup.testMagicHand = switchTestMagicHand.getValue();

        std::string compatModeVal = compatLevel.getValue();
        if(compatModeVal == "smbx2")
            setup.compatibilityLevel = COMPAT_SMBX2;
        else if(compatModeVal == "smbx13")
            setup.compatibilityLevel = COMPAT_SMBX13;
        else if(compatModeVal == "modern")
            setup.compatibilityLevel = COMPAT_MODERN;
        else
        {
            std::cerr << "Error: Invalid value for the --compat-level argument: " << compatModeVal << std::endl;
            std::cerr.flush();
            return 2;
        }

        setup.speedRunnerMode = speedRunMode.getValue();
        setup.speedRunnerSemiTransparent = switchSpeedRunSemiTransparent.getValue();

        if(setup.speedRunnerMode >= 1) // Always show FPS and don't pause the game work when focusing other windows
        {
            setup.testShowFPS = true;
            setup.neverPause = true;
        }
    }
    catch(TCLAP::ArgException &e)   // catch any exceptions
    {
        std::cerr << "Error: " << e.error() << " for arg " << e.argId() << std::endl;
        std::cerr.flush();
        return 2;
    }

    initGameInfo();

    // set this flag before SDL initialization to allow game be quit when closing a window before a loading process will be completed
    GameIsActive = true;

    if(frmMain.initSDL(setup))
    {
        frmMain.freeSDL();
        return 1;
    }

#ifdef __APPLE__
    macosReceiveOpenFile();
    if(!g_fileToOpen.empty())
    {
        setup.testLevel = g_fileToOpen;
        setup.testLevelMode = !setup.testLevel.empty();
        setup.testNumPlayers = 1;
        setup.testGodMode = false;
        setup.testGrabAll = false;
        setup.testShowFPS = false;
        setup.testMaxFPS = false;
    }
#endif

#ifdef ENABLE_XTECH_LUA
    if(!xtech_lua_init())
        return 1;
#endif

    int ret = GameMain(setup);

#ifdef ENABLE_XTECH_LUA
    if(!xtech_lua_quit())
        return 1;
#endif

    frmMain.freeSDL();

    return ret;
}
