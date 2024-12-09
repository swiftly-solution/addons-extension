#include "entrypoint.h"
#include "core.h"

void SwiftlyAddonsManagerHelp()
{
    g_SMAPI->ConPrintf("[Addons] Swiftly Addons Management Menu\n");
    g_SMAPI->ConPrintf("[Addons] Usage: sw_addons <command>\n");
    g_SMAPI->ConPrintf("[Addons]  disable    - Disables the addons downloading.\n");
    g_SMAPI->ConPrintf("[Addons]  enable     - Enables the addons downloading.\n");
    g_SMAPI->ConPrintf("[Addons]  reload     - Reloads the addons from the configuration.\n");
    g_SMAPI->ConPrintf("[Addons]  status     - Shows the status of the addons downloading.\n");
}

void SwiftlyAddonsManagerReload()
{
    g_addons.LoadAddons();
    g_SMAPI->ConPrintf("[Addons] All addons has been succesfully reloaded.\n");
}

void SwiftlyAddonsManagerDisable()
{
    if (!g_addons.GetStatus())
        return g_SMAPI->ConPrintf("[Addons] Addons is already disabled.\n");

    g_addons.ToggleStatus();
    g_SMAPI->ConPrintf("[Addons] Addons has been disabled.\n");
}

void SwiftlyAddonsManagerEnable()
{
    if (g_addons.GetStatus())
        return g_SMAPI->ConPrintf("[Addons] Addons is already enabled.\n");

    g_addons.ToggleStatus();
    g_SMAPI->ConPrintf("[Addons] Addons has been enabled.\n");
}

void SwiftlyAddonsManagerStatus()
{
    g_SMAPI->ConPrintf("[Addons] Addons Status: %s.\n", g_addons.GetStatus() ? "Enabled" : "Disabled");
}

CON_COMMAND(sw_addons, "Addons Manager Menu")
{
    std::string sbcmd = args[1];
    if (sbcmd.size() == 0)
    {
        SwiftlyAddonsManagerHelp();
        return;
    }

    if (sbcmd == "reload")
        SwiftlyAddonsManagerReload();
    else if (sbcmd == "disable")
        SwiftlyAddonsManagerDisable();
    else if (sbcmd == "enable")
        SwiftlyAddonsManagerEnable();
    else if (sbcmd == "status")
        SwiftlyAddonsManagerStatus();
    else
        SwiftlyAddonsManagerHelp();
}