#include "../controls.h"
#include "../main/record.h"
#include "../main/speedrunner.h"
#include "keyboard.h"

#include <Logger/logger.h>

namespace Controls
{

std::vector<InputMethod*> g_InputMethods;
std::vector<InputMethodType*> g_InputMethodTypes;

/*====================================================*\
|| implementation for InputMethod                     ||
\*====================================================*/

InputMethod::~InputMethod() {}

// Used for battery, latency, etc
// If this is dynamically generated, you MUST use an instance-owned buffer
// This will NOT be freed
// returning a null pointer is allowed
const char* InputMethod::StatusInfo()
{
    return nullptr;
}

/*====================================================*\
|| implementation for InputMethodProfile              ||
\*====================================================*/

InputMethodProfile::~InputMethodProfile() {}

// How many per-type special options are there?
size_t InputMethodProfile::GetSpecialOptionCount()
{
    return 0;
}

// get a nullable char* describing the option
const char* InputMethodProfile::GetOptionName(size_t i)
{
    (void)i;
    return nullptr;
}
// get a nullable char* describing the current option value
// must be allocated in static or instance memory
const char* InputMethodProfile::GetOptionValue(size_t i)
{
    (void)i;
    return nullptr;
}
// called when A is pressed; allowed to interrupt main game loop
bool InputMethodProfile::OptionChange(size_t i)
{
    (void)i;
    return false;
}
// called when left is pressed
bool InputMethodProfile::OptionRotateLeft(size_t i)
{
    (void)i;
    return false;
}
// called when right is pressed
bool InputMethodProfile::OptionRotateRight(size_t i)
{
    (void)i;
    return false;
}

/*====================================================*\
|| implementation for InputMethodType                 ||
\*====================================================*/

InputMethodType::~InputMethodType()
{
    for(InputMethodProfile* p : this->m_profiles)
    {
        delete p;
    }
    this->m_profiles.clear();
}

std::vector<InputMethodProfile*> InputMethodType::GetProfiles()
{
    return this->m_profiles;
}

InputMethodProfile* InputMethodType::AddProfile()
{
    InputMethodProfile* profile = this->AllocateProfile();
    if(!profile)
        return nullptr;
    profile->Type = this;
    this->m_profiles.push_back((InputMethodProfile*) profile);
    profile->Name = this->Name + " " + std::to_string(this->m_profiles.size());
    return profile;
}

// be extremely careful never to delete a profile that is in use
bool InputMethodType::DeleteProfile(InputMethodProfile* profile, const std::vector<InputMethod*>& active_methods)
{
    if(!profile)
        return false;

    // delete from m_profiles
    std::vector<InputMethodProfile*>::iterator loc = std::find(this->m_profiles.begin(), this->m_profiles.end(), profile);
    if(loc == this->m_profiles.end())
        return false;

    // figure out a backup profile to assign to all relevant methods
    InputMethodProfile* backup;
    if(loc != this->m_profiles.begin())
    {
        backup = this->m_profiles[0];
    }
    else if(this->m_profiles.size() > 1)
    {
        backup = this->m_profiles[1];
    }
    else
    {
        backup = nullptr;
    }

    for(InputMethod* method : active_methods)
    {
        if(!method)
            continue;
        if(method->Profile == profile)
        {
            if(backup)
            {
                method->Profile = backup;
            }
            // deleting the profile would leave the game inconsistent
            else
            {
                return false;
            }
        }
    }

    for(int i = 0; i < maxLocalPlayers; i++)
    {
        if(this->m_defaultProfiles[i] == profile)
        {
            // m_defaultProfiles is nullable, so this is okay
            this->m_defaultProfiles[i] = backup;
        }
    }

    this->m_profiles.erase(loc);

    // deallocate
    delete profile;

    return true;
}

bool InputMethodType::ClearProfiles(const std::vector<InputMethod*>& active_methods)
{
    // clear existing profiles safely
    size_t i = 0;
    while(i < this->m_profiles.size())
    {
        // only increment the index when deletion fails
        if(!this->DeleteProfile(this->m_profiles[i], active_methods))
        {
            i++;
        }
    }
    // succeeded if all were cleared successfully
    return (i == 0);
}

bool InputMethodType::SetDefaultProfile(int player_no, InputMethodProfile* profile)
{
    if(player_no < 0 || player_no >= maxLocalPlayers || !profile)
    {
        return false;
    }
    this->m_defaultProfiles[player_no] = profile;
    return true;
}

InputMethodProfile* InputMethodType::GetDefaultProfile(int player_no)
{
    if(player_no < 0 || player_no >= maxLocalPlayers)
        return nullptr;
    // still possibly a null pointer
    return this->m_defaultProfiles[player_no];
}

void InputMethodType::SaveConfig(IniProcessing* ctl)
{
    ctl->beginGroup(this->Name);
    this->SaveConfig_Custom(ctl);
    ctl->setValue("n-profiles", this->m_profiles.size());
    for(int i = 0; i < maxLocalPlayers; i++)
    {
        InputMethodProfile* default_profile = this->m_defaultProfiles[i];
        std::string default_profile_key = "default-profile-" + std::to_string(i);
        if(default_profile == nullptr)
        {
            ctl->setValue(default_profile_key.c_str(), -1);
            continue;
        }
        std::vector<InputMethodProfile*>::iterator loc = std::find(this->m_profiles.begin(), this->m_profiles.end(), default_profile);
        size_t index = loc - this->m_profiles.begin();
        ctl->setValue(default_profile_key.c_str(), index);
    }
    ctl->endGroup();
    for(size_t i = 0; i < this->m_profiles.size(); i++)
    {
        ctl->beginGroup(this->Name + "-" + std::to_string(i));
        ctl->setValue("name", this->m_profiles[i]->Name);
        this->m_profiles[i]->SaveConfig(ctl);
        ctl->endGroup();
    }
}

void InputMethodType::LoadConfig(IniProcessing* ctl)
{
    int n_profiles;
    int n_existing = this->m_profiles.size(); // should usually be zero

    ctl->beginGroup(this->Name);
    ctl->read("n-profiles", n_profiles, 0); // TODO
    this->LoadConfig_Custom(ctl);
    ctl->endGroup();

    for(int i = 0; i < n_profiles; i++)
    {
        ctl->beginGroup(this->Name + "-" + std::to_string(i));
        InputMethodProfile* new_profile = this->AddProfile();
        if(new_profile)
        {
            ctl->read("name", new_profile->Name, this->Name + " " + std::to_string(i+n_existing+1));
            new_profile->LoadConfig(ctl);
        }
        ctl->endGroup();
    }

    ctl->beginGroup(this->Name);
    for(int i = 0; i < maxLocalPlayers; i++)
    {
        std::string default_profile_key = "default-profile-" + std::to_string(i);
        int index;
        ctl->read(default_profile_key.c_str(), index, -1);
        if(index == -1)
            this->m_defaultProfiles[i] = nullptr;
        else
        {
            index += n_existing;
            this->m_defaultProfiles[i] = this->m_profiles[index];
        }
    }
    ctl->endGroup();
}

// optionally overriden methods

bool InputMethodType::SetDefaultProfile(InputMethod* method, InputMethodProfile* profile)
{
    if(!method || !profile)
    {
        return false;
    }
    return true;
}

// How many per-type special options are there?
size_t InputMethodType::GetSpecialOptionCount()
{
    return 0;
}

// get a nullable char* describing the option
const char* InputMethodType::GetOptionName(size_t i)
{
    (void)i;
    return nullptr;
}
// get a nullable char* describing the current option value
// must be allocated in static or instance memory
const char* InputMethodType::GetOptionValue(size_t i)
{
    (void)i;
    return nullptr;
}
// called when A is pressed; allowed to interrupt main game loop
bool InputMethodType::OptionChange(size_t i)
{
    (void)i;
    return false;
}
// called when left is pressed
bool InputMethodType::OptionRotateLeft(size_t i)
{
    (void)i;
    return false;
}
// called when right is pressed
bool InputMethodType::OptionRotateRight(size_t i)
{
    (void)i;
    return false;
}

void InputMethodType::SaveConfig_Custom(IniProcessing* ctl)
{
    (void)ctl;
    // must be implemented if user has created special options
    SDL_assert_release(this->GetSpecialOptionCount() == 0);
}
void InputMethodType::LoadConfig_Custom(IniProcessing* ctl)
{
    (void)ctl;
    // must be implemented if user has created special options
    SDL_assert_release(this->GetSpecialOptionCount() == 0);
}

/*====================================================*\
|| implementation for global functions                ||
\*====================================================*/

// allocate InputMethodTypes according to system configuration
void Init()
{
    g_InputMethodTypes.push_back(new InputMethodType_Keyboard);
}

void Quit()
{
    ClearInputMethods();
    for(InputMethodType* type : g_InputMethodTypes)
    {
        delete type;
    }
    g_InputMethodTypes.clear();
}

// 1. Calls the UpdateControlsPre hooks of loaded InputMethodTypes
//    a. Syncs hardware state as needed
// 2. Updates Player and Editor controls by calling currently bound InputMethods
//    a. May call or set changeScreen, frmMain.toggleGifRecorder, TakeScreen, g_stats.enabled, etc
// 3. Calls the UpdateControlsPost hooks of loaded InputMethodTypes
//    a. May call or set changeScreen, frmMain.toggleGifRecorder, TakeScreen, g_stats.enabled, etc
//    b. If GameMenu or GameOutro is set, may update controls or Menu variables using hardcoded keys
// 4. Updates speedrun and recording modules
// 5. Resolves inconsistent control states (Left+Right, etc)
[[nodiscard]] bool Update()
{
    bool okay = true;
    for(InputMethodType* type : g_InputMethodTypes)
    {
        type->UpdateControlsPre();
    }

    Controls_t blankControls = Controls_t();

    for(size_t i = 0; i < maxLocalPlayers; i++)
    {
        Controls_t& controls = Player[i+1].Controls;

        controls = blankControls;

        if(i >= g_InputMethods.size())
            continue;

        InputMethod* method = g_InputMethods[i];
        if(!method)
        {
            okay = false;
            continue;
        }
        if(!method->Update(controls))
        {
            okay = false;
            DeleteInputMethod(method);
            // the method pointer is no longer valid
            continue;
        }
    }

    SharedControls = SharedControls_t();
    for(InputMethodType* type : g_InputMethodTypes)
    {
        type->UpdateControlsPost();
    }

    // sync controls
    record_sync();
    for(int i = 0; i < numPlayers; i++)
    {
        speedRun_syncControlKeys(i, Player[i+1].Controls);
    }

    // resolve invalid states
    For(B, 1, numPlayers)
    {
        int A;
        if(B == 2 && numPlayers == 2) {
            A = 2;
        } else {
            A = 1;
        }

        // With Player(A).Controls
        {
            auto &p = Player[A];
            Controls_t &c = p.Controls;

            if(!c.Start && !c.Jump) {
                p.UnStart = true;
            }

            if(c.Up && c.Down)
            {
                c.Up = false;
                c.Down = false;
            }

            if(c.Left && c.Right)
            {
                c.Left = false;
                c.Right = false;
            }

            if(!(p.State == 5 && p.Mount == 0) && c.AltRun)
                c.Run = true;
            if(ForcedControls && !GamePaused)
            {
                c = ForcedControl;
            }
        } // End With
    }

    // single coop code -- may want to revise
    if(SingleCoop > 0)
    {
        if(numPlayers == 1 || numPlayers > 2)
            SingleCoop = 0;

        if(SingleCoop == 1) {
            Player[2].Controls = blankControls;
        } else {
            Player[2].Controls = Player[1].Controls;
            Player[1].Controls = blankControls;
        }
    }

    For(A, 1, numPlayers)
    {
        {
            Player_t &p = Player[A];
            if(p.SpinJump)
            {
                if(p.SpinFrame < 4 || p.SpinFrame > 9) {
                    p.Direction = -1;
                } else {
                    p.Direction = 1;
                }
            }
        }
    }

    return okay;
}

void SaveConfig(IniProcessing* ctl)
{
    for(InputMethodType* type : g_InputMethodTypes)
    {
        type->SaveConfig(ctl);
    }
}

void LoadConfig(IniProcessing* ctl)
{
    for(InputMethodType* type : g_InputMethodTypes)
    {
        type->ClearProfiles(g_InputMethods);
        type->LoadConfig(ctl);
    }
}

InputMethod* PollInputMethod() noexcept
{
    InputMethod* new_method = nullptr;
    for(InputMethodType* type : g_InputMethodTypes)
    {
        new_method = type->Poll(g_InputMethods);
        if(new_method)
            break;
    }
    if(!new_method)
        return nullptr;
    size_t player_idx = 0;
    while(player_idx < g_InputMethods.size() && g_InputMethods[player_idx] != nullptr)
    {
        player_idx ++;
    }
    if(!new_method->Profile)
    {
        // fallback 1: last profile used by this player index
        InputMethodProfile* default_profile = new_method->Type->GetDefaultProfile(player_idx);
        if(default_profile)
        {
            new_method->Profile = default_profile;
        }
        else
        {
            std::vector<InputMethodProfile*> profiles = new_method->Type->GetProfiles();
            if(!profiles.empty())
            {
                // fallback 2: find first unused profile
                size_t i;
                for(i = 0; i < profiles.size(); i++)
                {
                    bool unused = true;
                    for(InputMethod* other_method : g_InputMethods)
                    {
                        if(other_method && other_method->Profile == profiles[i])
                        {
                            unused = false;
                            break;
                        }
                    }
                    if(unused)
                        break;
                }
                if(i != profiles.size())
                    new_method->Profile = profiles[i];
                // fallback 3: first profile
                else
                    new_method->Profile = profiles[0];
            }
            // fallback 4: new default profile
            else
            {
                new_method->Profile = new_method->Type->AddProfile();
            }
        }
    }
    // should only be null if something is very wrong (alloc failed, etc)
    if(!new_method->Profile)
    {
        delete new_method;
        return nullptr;
    }
    if(player_idx < g_InputMethods.size())
    {
        g_InputMethods[player_idx] = new_method;
    }
    else
    {
        g_InputMethods.push_back(new_method);
    }
    pLogDebug("Just connected new %s '%s' with profile '%s'.",
        new_method->Type->Name.c_str(),  new_method->Name.c_str(), new_method->Profile->Name.c_str());
    return new_method;
}

void DeleteInputMethod(InputMethod* method)
{
    std::vector<InputMethod*>::iterator loc
        = std::find(g_InputMethods.begin(), g_InputMethods.end(), method);
    while(loc != g_InputMethods.end())
    {
        *loc = nullptr;
        loc = std::find(g_InputMethods.begin(), g_InputMethods.end(), method);
    }
    delete method;
}

void DeleteInputMethodSlot(int slot)
{
    if(slot < 0 || (size_t)slot > g_InputMethods.size())
        return;
    if(g_InputMethods[slot] != nullptr)
    {
        DeleteInputMethod(g_InputMethods[slot]);
    }
    g_InputMethods.erase(g_InputMethods.begin() + slot);
}

void ClearInputMethods()
{
    for(size_t i = 0; i < g_InputMethods.size(); i++)
    {
        DeleteInputMethod(g_InputMethods[i]);
    }
    g_InputMethods.clear();
}

} // namespace Controls