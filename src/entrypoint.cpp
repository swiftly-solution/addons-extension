#include "entrypoint.h"
#include "core.h"
#include <thread>
#include <public/iserver.h>
#include <steam/steam_gameserver.h>
#include <swiftly-ext/hooks/NativeHooks.h>

//////////////////////////////////////////////////////////////
/////////////////        Core Variables        //////////////
////////////////////////////////////////////////////////////

SH_DECL_HOOK0_void(IServerGameDLL, GameServerSteamAPIActivated, SH_NOATTRIB, 0);
SH_DECL_HOOK3_void(INetworkServerService, StartupServer, SH_NOATTRIB, 0, const GameSessionConfiguration_t&, ISource2WorldSession*, const char*);
SH_DECL_HOOK6(IServerGameClients, ClientConnect, SH_NOATTRIB, 0, bool, CPlayerSlot, const char*, uint64, const char*, bool, CBufferString*);

AddonsExtension g_Ext;
CUtlVector<FuncHookBase *> g_vecHooks;

CSteamGameServerAPIContext g_SteamAPI;
IVEngineServer2* engine = nullptr;
ISource2Server* server = nullptr;
IServerGameClients* gameclients = nullptr;

CREATE_GLOBALVARS();

Addons g_addons;

//////////////////////////////////////////////////////////////
/////////////////             Hooks            //////////////
////////////////////////////////////////////////////////////

FuncHook<decltype(Hook_HostStateRequest)> THostStateRequest(Hook_HostStateRequest, "HostStateRequest");
FuncHook<decltype(Hook_SendNetMessage)> TSendNetMessage(Hook_SendNetMessage, "engine2", "CServerSideClient", "SendNetMessage");

void* Hook_HostStateRequest(void* a1, void** pRequest)
{
    if (g_addons.GetStatus() == false || g_addons.GetAddons().size() == 0)
        return THostStateRequest(a1, pRequest);

    CUtlString* pszAddonString = (CUtlString*)(pRequest + 11);
    auto v = explode(pszAddonString->Get(), ",");
    auto adns = g_addons.GetAddons();

    pszAddonString->Clear();
    std::string extra = implode(adns, ",");

    if (v.size() == 0 || std::find(adns.begin(), adns.end(), v[0]) != adns.end()) {
        pszAddonString->Set(extra.c_str());
        g_addons.currentWorkshopMap.clear();
    }
    else {
        if(v[0].empty()) pszAddonString->Format("%s", extra.c_str());
        else pszAddonString->Format("%s,%s", v[0].c_str(), extra.c_str());
        g_addons.currentWorkshopMap = v[0];
    }

    return THostStateRequest(a1, pRequest);
}

bool Hook_SendNetMessage(CServerSideClient* pClient, CNetMessage* pData, NetChannelBufType_t bufType)
{
    NetMessageInfo_t* info = pData->GetNetMessage()->GetNetMessageInfo();

    if (info->m_MessageId != net_SignonState || g_addons.GetStatus() == false || g_addons.GetAddons().size() == 0)
        return TSendNetMessage(pClient, pData, bufType);

    auto pMsg = pData->ToPB<CNETMsg_SignonState>();
    if (pMsg->signon_state() == SIGNONSTATE_CHANGELEVEL)
        pMsg->set_addons(g_addons.currentWorkshopMap.empty() ? g_addons.GetAddons()[0].c_str() : g_addons.currentWorkshopMap.c_str());

    int idx;
    ClientJoinInfo_t* pPendingClient = GetPendingClient(pClient->GetClientSteamID().ConvertToUint64(), idx);
    if (pPendingClient)
    {
        pMsg->set_addons(g_addons.GetAddons()[pPendingClient->addon].c_str());
        pMsg->set_signon_state(SIGNONSTATE_CHANGELEVEL);
        pPendingClient->signon_timestamp = Plat_FloatTime();
    }

    return TSendNetMessage(pClient, pData, bufType);
}

//////////////////////////////////////////////////////////////
/////////////////          Core Class          //////////////
////////////////////////////////////////////////////////////

EXT_EXPOSE(g_Ext);
bool AddonsExtension::Load(std::string& error, SourceHook::ISourceHook *SHPtr, ISmmAPI* ismm, bool late)
{
    SAVE_GLOBALVARS();

    if(!InitializeHooks()) {
        error = "Failed to initialize hooks.";
        return false;
    }

    engine = (IVEngineServer *)ismm->VInterfaceMatch(ismm->GetEngineFactory(), INTERFACEVERSION_VENGINESERVER); 
    if (!engine) { 
        error = "Could not find interface: " INTERFACEVERSION_VENGINESERVER;
        return false; 
    }

    g_pNetworkServerService = (INetworkServerService *)ismm->VInterfaceMatch(ismm->GetEngineFactory(), NETWORKSERVERSERVICE_INTERFACE_VERSION); 
    if (!g_pNetworkServerService) { 
        error = "Could not find interface: " NETWORKSERVERSERVICE_INTERFACE_VERSION;
        return false; 
    }

    g_pFullFileSystem = (IFileSystem *)ismm->VInterfaceMatch(ismm->GetFileSystemFactory(), FILESYSTEM_INTERFACE_VERSION, 0); 
    if (!g_pFullFileSystem) {
        error = "Could not find interface: " FILESYSTEM_INTERFACE_VERSION;
        return false;
    }

    server = (ISource2Server *)ismm->VInterfaceMatch(ismm->GetServerFactory(), INTERFACEVERSION_SERVERGAMEDLL, 0); 
    if (!server) {
        error = "Could not find interface: " INTERFACEVERSION_SERVERGAMEDLL;
        return false;
    }

    gameclients = (IServerGameClients *)ismm->VInterfaceMatch(ismm->GetServerFactory(), INTERFACEVERSION_SERVERGAMECLIENTS, 0); 
    if (!gameclients) {
        error = "Could not find interface: " INTERFACEVERSION_SERVERGAMECLIENTS;
        return false;
    }

    SH_ADD_HOOK_MEMFUNC(IServerGameDLL, GameServerSteamAPIActivated, server, this, &AddonsExtension::Hook_GameServerSteamAPIActivated, false);
    SH_ADD_HOOK_MEMFUNC(IServerGameClients, ClientConnect, gameclients, this, &AddonsExtension::Hook_ClientConnect, false);
    SH_ADD_HOOK_MEMFUNC(INetworkServerService, StartupServer, g_pNetworkServerService, this, &AddonsExtension::Hook_StartupServer, true);

    g_addons.LoadAddons();

    if(late)
    {
        g_SteamAPI.Init();
        m_CallbackDownloadItemResult.Register(this, &AddonsExtension::OnAddonDownloaded);
    }

    g_addons.SetupThread();

    return true;
}

void AddonsExtension::Hook_GameServerSteamAPIActivated()
{
    if (!CommandLine()->HasParm("-dedicated") || g_SteamAPI.SteamUGC())
        return;

    g_SteamAPI.Init();
    m_CallbackDownloadItemResult.Register(this, &AddonsExtension::OnAddonDownloaded);

    if (g_addons.GetStatus()) {
        std::thread([&]() -> void
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(3000));

                if (g_addons.GetStatus())
                    g_addons.RefreshAddons(true); })
            .detach();
    }

    RETURN_META(MRES_IGNORED);
}

void AddonsExtension::Hook_StartupServer(const GameSessionConfiguration_t& config, ISource2WorldSession*, const char*)
{
    g_ClientsPendingAddon.RemoveAll();

    if (g_addons.GetStatus())
        g_addons.RefreshAddons();
}

void AddonsExtension::OnAddonDownloaded(DownloadItemResult_t* pResult)
{
    g_addons.OnAddonDownloaded(pResult);
}

bool AddonsExtension::Hook_ClientConnect(CPlayerSlot slot, const char* pszName, uint64 xuid, const char* pszNetworkID, bool unk1, CBufferString* pRejectReason)
{
    g_addons.OnClientConnect(xuid);
    RETURN_META_VALUE(MRES_IGNORED, true);
}

bool AddonsExtension::Unload(std::string& error)
{
    SH_REMOVE_HOOK_MEMFUNC(IServerGameDLL, GameServerSteamAPIActivated, server, this, &AddonsExtension::Hook_GameServerSteamAPIActivated, false);
    SH_REMOVE_HOOK_MEMFUNC(IServerGameClients, ClientConnect, gameclients, this, &AddonsExtension::Hook_ClientConnect, false);
    SH_REMOVE_HOOK_MEMFUNC(INetworkServerService, StartupServer, g_pNetworkServerService, this, &AddonsExtension::Hook_StartupServer, true);
    
    UnloadHooks();
    return true;
}

void Addons::ToggleHooks()
{
    THostStateRequest.Disable();
    TSendNetMessage.Disable();

    if (this->m_status) {
        if (!GetSignature("HostStateRequest"))
        {
            SetStatus(false);
            g_SMAPI->ConPrint("[Addons] The signature for \"HostStateRequest\" has not been found. The \"Addons\" system has been forcefully disabled.\n");
            return;
        }

        THostStateRequest.Enable();
        TSendNetMessage.Enable();
    }
}

void AddonsExtension::AllExtensionsLoaded()
{

}

void AddonsExtension::AllPluginsLoaded()
{

}

bool AddonsExtension::OnPluginLoad(std::string pluginName, void* pluginState, PluginKind_t kind, std::string& error)
{
    return true;
}

bool AddonsExtension::OnPluginUnload(std::string pluginName, void* pluginState, PluginKind_t kind, std::string& error)
{
    return true;
}

const char* AddonsExtension::GetAuthor()
{
    return "Swiftly Development Team";
}

const char* AddonsExtension::GetName()
{
    return "Addons Extension";
}

const char* AddonsExtension::GetVersion()
{
#ifndef VERSION
    return "Local";
#else
    return VERSION;
#endif
}

const char* AddonsExtension::GetWebsite()
{
    return "https://swiftlycs2.net/";
}
