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

#ifndef JOYSTICK_H
#define JOYSTICK_H

#include "../controls.h"

#include <string>
#include <unordered_map>

typedef struct _SDL_Joystick SDL_Joystick;
typedef struct _SDL_GameController SDL_GameController;
typedef struct _SDL_Haptic SDL_Haptic;

namespace Controls
{

struct JoystickDevices
{
    SDL_Joystick* joy = nullptr;
    SDL_GameController* ctrl = nullptr;
    SDL_Haptic* haptic = nullptr;

    std::string guid;
};

struct KM_Key
{
    enum CtrlTypes
    {
        NoControl=-1,
        JoyAxis=0,
        JoyBallX,
        JoyBallY,
        JoyHat,
        JoyButton,
        CtrlButton,
        CtrlAxis
    };

    // SDL_Joystick control or SDL_GameController control, depending on context
    int val = -1;
    int id = -1;
    int type = -1;

    inline bool operator==(const KM_Key& o)
    {
        return o.id == id && o.val == val && o.type == type;
    }

    inline void assign(int type, int id, int val)
    {
        this->type = type;
        this->id = id;
        this->val = val;
    }
};

class InputMethod_Joystick : public InputMethod
{
public:
    JoystickDevices* m_devices;

    using InputMethod::Type;
    using InputMethod::Profile;

    // Update functions that set player controls (and editor controls)
    // based on current device input. Return false if device lost.
    bool Update(Controls_t& c);
    // bool Update(EditorControls_t& c);

    void Rumble(int ms, float strength);
};

class InputMethodProfile_Joystick : public InputMethodProfile
{
private:
    bool m_canPoll = false;

public:
    using InputMethodProfile::Name;
    using InputMethodProfile::Type;

    bool m_controllerProfile = false;
    bool m_rumbleEnabled = true;

    KM_Key m_keys[PlayerControls::n_buttons];
    KM_Key m_keys2[PlayerControls::n_buttons];

    InputMethodProfile_Joystick();

    void InitAsJoystick();
    void InitAsController();

    // Polls a new (secondary) device button for the i'th player button
    // Returns true on success and false if no button pressed
    // Never allows two player buttons to bind to the same device button
    bool PollPrimaryButton(size_t i);
    bool PollSecondaryButton(size_t i);

    // Deletes a secondary device button for the i'th player button
    bool DeleteSecondaryButton(size_t i);

    // Gets strings for the device buttons currently used for the i'th player button
    const char* NamePrimaryButton(size_t i);
    const char* NameSecondaryButton(size_t i);

    // one can assume that the IniProcessing* is already in the correct group
    void SaveConfig(IniProcessing* ctl);
    void LoadConfig(IniProcessing* ctl);

    // OPTIONAL METHODS

    // How many per-type special options are there?
    size_t GetSpecialOptionCount();
    // Methods to manage per-profile options
    // It is guaranteed that none of these will be called if
    // GetOptionCount() returns 0.
    // get a char* describing the option
    const char* GetOptionName(size_t i);
    // get a char* describing the current option value
    // must be allocated in static or instance memory
    // WILL NOT be freed
    const char* GetOptionValue(size_t i);
    // called when A is pressed; allowed to interrupt main game loop
    bool OptionChange(size_t i);
    // called when left is pressed
    bool OptionRotateLeft(size_t i);
    // called when right is pressed
    bool OptionRotateRight(size_t i);
};

class InputMethodType_Joystick : public InputMethodType
{
private:
    bool m_canPoll = false;
    std::unordered_map<int, JoystickDevices*> m_availableJoysticks;
    std::unordered_map<std::string, InputMethodProfile*> m_lastProfileByGUID;

    InputMethodProfile* AllocateProfile() noexcept;

    /*-----------------------*\
    || CUSTOM METHODS        ||
    \*-----------------------*/
    bool OpenJoystick(int joystick_index);
    bool CloseJoystick(int instance_id);

public:
    using InputMethodType::Name;
    using InputMethodType::m_profiles;

    const uint8_t* m_JoystickState;
    int m_JoystickStateSize;

    InputMethodType_Joystick();

    void UpdateControlsPre();
    void UpdateControlsPost();

    // null if no input method is ready
    // allocates the new InputMethod on the heap
    InputMethod* Poll(const std::vector<InputMethod*>& active_methods) noexcept;

    /*-----------------------*\
    || CUSTOM METHODS        ||
    \*-----------------------*/
    KM_Key PollJoystickKeyAll();
    KM_Key PollControllerKeyAll();
    InputMethodProfile* AddControllerProfile();

    /*-----------------------*\
    || OPTIONAL METHODS      ||
    \*-----------------------*/
protected:
    // optional function allowing developer to associate device information with profile, etc
    // if developer wants to forbid assignment, return false
    bool SetProfile_Custom(InputMethod* method, int player_no, InputMethodProfile* profile, const std::vector<InputMethod*>& active_methods);
public:
    bool ConsumeEvent(const SDL_Event* ev);

public:
    // How many per-type special options are there?
    size_t GetSpecialOptionCount();
    // Methods to manage per-profile options
    // It is guaranteed that none of these will be called if
    // GetOptionCount() returns 0.
    // get a char* describing the option
    const char* GetOptionName(size_t i);
    // get a char* describing the current option value
    // must be allocated in static or instance memory
    // WILL NOT be freed
    const char* GetOptionValue(size_t i);
    // called when A is pressed; allowed to interrupt main game loop
    bool OptionChange(size_t i);

protected:
    void SaveConfig_Custom(IniProcessing* ctl);
    void LoadConfig_Custom(IniProcessing* ctl);

};

} // namespace Controls

#endif // #ifndef JOYSTICK_H
