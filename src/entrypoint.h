#ifndef _entrypoint_h
#define _entrypoint_h

#include <string>
#include <steam/steam_api_common.h>
#include <steam/isteamugc.h>

#include <swiftly-ext/core.h>
#include <swiftly-ext/extension.h>

#include <public/filesystem.h>

class GameSessionConfiguration_t
{
};

class AddonsExtension : public SwiftlyExt
{
public:
    bool Load(std::string& error, SourceHook::ISourceHook *SHPtr, ISmmAPI* ismm, bool late);
    bool Unload(std::string& error);
    
    void AllExtensionsLoaded();
    void AllPluginsLoaded();

    bool OnPluginLoad(std::string pluginName, void* pluginState, PluginKind_t kind, std::string& error);
    bool OnPluginUnload(std::string pluginName, void* pluginState, PluginKind_t kind, std::string& error);

    void Hook_GameServerSteamAPIActivated();
    bool Hook_ClientConnect(CPlayerSlot slot, const char* pszName, uint64 xuid, const char* pszNetworkID, bool unk1, CBufferString* pRejectReason);
    void Hook_StartupServer(const GameSessionConfiguration_t& config, ISource2WorldSession*, const char*);

public:
    const char* GetAuthor();
    const char* GetName();
    const char* GetVersion();
    const char* GetWebsite();

public:
    STEAM_GAMESERVER_CALLBACK_MANUAL(AddonsExtension, OnAddonDownloaded, DownloadItemResult_t, m_CallbackDownloadItemResult);
};

extern AddonsExtension g_Ext;
extern CSteamGameServerAPIContext g_SteamAPI;
extern IVEngineServer2* engine;
extern ISource2Server* server;
extern IServerGameClients* gameclients;
DECLARE_GLOBALVARS();

#endif