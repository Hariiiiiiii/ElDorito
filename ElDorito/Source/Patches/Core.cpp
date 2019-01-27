#include "Core.hpp"

#include "../Blam/Cache/StringIdCache.hpp"

#include "../Blam/Geometry/RenderGeometry.hpp"

#include "../Blam/Math/RealColorARGB.hpp"
#include "../Blam/Math/RealMatrix4x3.hpp"
#include "../Blam/Math/RealQuaternion.hpp"
#include "../Blam/Math/RealVector3D.hpp"

#include "../Blam/Memory/TlsData.hpp"

#include "../Blam/Preferences/Preferences.hpp"

#include "../Blam/Tags/TagInstance.hpp"
#include "../Blam/Tags/Effects/DecalSystem.hpp"
#include "../Blam/Tags/Game/Globals.hpp"
#include "../Blam/Tags/Items/DefinitionWeapon.hpp"
#include "../Blam/Tags/Scenario/Scenario.hpp"

#include "../Blam/BlamData.hpp"
#include "../Blam/BlamEvents.hpp"
#include "../Blam/BlamPlayers.hpp"
#include "../Blam/BlamNetwork.hpp"
#include "../Blam/BlamObjects.hpp"
#include "../Blam/BlamTypes.hpp"

#include "../Modules/ModuleCampaign.hpp"
#include "../Modules/ModuleGame.hpp"
#include "../Modules/ModulePlayer.hpp"
#include "../Modules/ModuleServer.hpp"
#include "../Modules/ModuleTweaks.hpp"

#include "../Console.hpp"

#include "../ElDorito.hpp"
#include "../ElPatches.hpp"
#include "../Patch.hpp"

#include "boost/filesystem.hpp"

#include <codecvt>
#include <Shlobj.h>

// new headers
#include <effects\particles.hpp>
#include <game\game_globals.hpp>
#include <memory\data.hpp>
#include <objects\object_types.hpp>
#include <structures\scenario_structure_bsp.hpp>

namespace
{
	void GameTickHook();
	void TagsLoadedHook();
	void FovHook();
	double AspectRatioHook();
	void ActiveCamoViewModelClipHook(float *nearPlane, float *farPlane);
	void GrenadeLoadoutHook();
	void ShutdownHook();
	const char *GetMapsFolderHook();
	bool LoadMapHook(Blam::LevelData *data);
	void LoadLevelHook(uint8_t* mapinfo, char a2, char *mapsPath, char a4);
	void GameStartHook();
	void __fastcall EdgeDropHook(void* thisptr, void* unused, int a2, int a3, int a4, float* a5);
	void __cdecl BipedFeetZoneOffsetHook(uint32_t bipedObjectIndex, Blam::Math::RealVector3D *position, float *height, float *radius);
	char GetBinkVideoPathHook(int p_VideoID, char *p_DestBuf);
	void DirtyDiskErrorHook();
	int __cdecl GetScreenshotFolderHook(wchar_t *path);
	void __cdecl HsPrintHook(const char *message);
	void ContrailFixHook();
	void HillColorHook();

	std::vector<Patches::Core::ShutdownCallback> shutdownCallbacks;
	std::string MapsFolder;
	std::string MapFormatString;
	std::string StringIdsPath;
	std::string TagsPath;
	std::string TagListPath;
	std::string ResourcesPath;
	std::string ResourcesBPath;
	std::string TexturesPath;
	std::string TexturesBPath;
	std::string AudioPath;
	std::string FontsPath;

	std::vector<Patches::Core::MapLoadedCallback> mapLoadedCallbacks;
	std::vector<Patches::Core::GameStartCallback> gameStartCallbacks;

	void *__cdecl LevelDataGetHook();
	void PrintLevelInfo();

	void preferences_set_defaults_hook()
	{
		static const auto preferences_set_defaults = reinterpret_cast<void(*)()>(0x50A520);

		preferences_set_defaults();

		Blam::Preferences *preferences = ElDorito::GetMainTls(0x18)[0];

		preferences->HideWatermark = 1;
		preferences->Unknown00 = 1;
		preferences->IsDirty = 1;

		*(uint32_t *)ElDorito::GetMainTls(0x18)[0](0x8400C) = 1;
	}

	struct s_file_reference
	{
		uint32_t header_type;
		uint16_t flags;
		uint16_t unknown6;
		char path[256];
		uint32_t file_handle;
		uint32_t file_pointer;
	};

	static const auto global_game_state_initialized = (bool *)0x2497CD0;
	static const auto global_game_state_data = (void **)0x2497CD4;
	static const auto global_game_state_size = (DWORD *)0x2497CDC;
	static const auto global_game_state_valid = (bool *)0x02497CE0;

	static const auto game_state_buffer_handle_read = (int(*)())0x50F280;
	static const auto game_state_security_verify_signature = (bool(__cdecl *)(LPVOID *))0x5106F0;

	static const auto file_create_parent_directories_if_not_present = (bool(__cdecl *)(s_file_reference *))0x527FF0;
	static const auto file_reference_create_from_path = (s_file_reference *(__cdecl *)(s_file_reference *, wchar_t *, bool))0x5285B0;
	static const auto file_close = (bool(__cdecl *)(s_file_reference *))0x528B60;
	static const auto file_create = (bool(__cdecl *)(s_file_reference *))0x528FB0;
	static const auto file_exists = (bool(__cdecl *)(s_file_reference *))0x5295F0;
	static const auto file_open = (bool(__cdecl *)(s_file_reference *, int, int *))0x52A220;
	static const auto file_read = (bool(__cdecl *)(s_file_reference *, DWORD, char, LPVOID))0x52A7E0;
	static const auto file_write = (bool(__cdecl *)(s_file_reference *, DWORD, LPCVOID))0x52B250;

	bool __cdecl file_read_from_path(wchar_t *path, DWORD size, LPVOID buffer)
	{
		s_file_reference file;
		file_reference_create_from_path(&file, path, false);

		int file_error;
		if (!file_open(&file, 1, &file_error))
		{
			// TODO: log file error
			return false;
		}

		auto result = file_read(&file, size, false, buffer);

		file_close(&file);

		return result;
	}

	bool __cdecl file_write_to_path(wchar_t *path, DWORD size, LPVOID buffer)
	{
		s_file_reference file;
		file_reference_create_from_path(&file, path, false);

		file_create_parent_directories_if_not_present(&file);

		if (!file_exists(&file))
			file_create(&file);

		int file_error;
		if (!file_open(&file, 2, &file_error))
		{
			// TODO: log file error
			return false;
		}

		auto result = file_write(&file, size, buffer);
		file_close(&file);

		return result;
	}

	static const auto game_state_call_after_load_procs = (int(__cdecl *)(int))0x58A4B0;
	static const auto game_state_call_before_load_procs = (int(__cdecl *)(char))0x58A5F0;

	bool __cdecl game_state_read_file_from_storage(int a1, int a2)
	{
		auto result = false;

		if (*global_game_state_initialized)
		{
			game_state_call_before_load_procs(a2);

			result = file_read_from_path(L"gamestate.hdr", *global_game_state_size, *global_game_state_data);

			game_state_buffer_handle_read();
			game_state_call_after_load_procs(a2);
		}

		return result;
	}

	bool __cdecl read_campaign_save_file_blocking(int, LPVOID buffer, DWORD size)
	{
		return file_read_from_path(L"mmiof.bmf", size, buffer);
	}

	bool __cdecl game_state_read_header_from_persistent_storage_blocking(int local_user_index, void *buffer, DWORD size)
	{
		return file_read_from_path(L"gamestate.hdr", size, buffer);
	}

	void game_state_write_file_to_storage()
	{
		auto result = false;

		if (*global_game_state_initialized)
			result = file_write_to_path(L"gamestate.hdr", *global_game_state_size, *global_game_state_data);

		*global_game_state_valid = result;
	}

	int __cdecl game_state_write_file_to_storage_blocking(LPVOID buffer1, DWORD size1, LPVOID buffer2, DWORD size2)
	{
		auto result = true;

		if (result)
			result = file_write_to_path(L"gamestate.hdr", size1, buffer1);

		if (result)
			result = file_write_to_path(L"mmiof.bmf", size2, buffer2);

		return 0;
	}

	bool __cdecl campaign_save_exists(int)
	{
		s_file_reference file;
		file_reference_create_from_path(&file, L"mmiof.bmf", false);
		return file_exists(&file);
	}

	bool __cdecl hash_verification(int a1, int a2, bool a3, int32_t *a4, int a5)
	{
		return true;
	}

	void __fastcall campaign_scoring_sub_6E59A0(char *scoreboard, void *, Blam::DatumHandle handle, Blam::Events::EventType event_type, short a4, Blam::ePlayerStatType player_stat_type, char a6)
	{
		static const auto data_array_sub_55B710 = reinterpret_cast<unsigned long(__cdecl *)(Blam::DataArrayBase *, Blam::DatumHandle)>(0x55B710);
		static const auto game_get_current_engine = reinterpret_cast<int(*)()>(0x5CE150);
		static const auto game_is_team_game = reinterpret_cast<bool(__cdecl *)()>(0x5565E0);
		static const auto scoreboard_sub_6E5A90 = reinterpret_cast<void(__thiscall *)(char *, unsigned int, Blam::Events::EventType, short, int)>(0x6E5A90);

		if (Modules::ModuleTweaks::Instance().VarDisableMetagame->ValueInt != 0)
			return;

		if (!scoreboard)
			return;

		auto *scoreboard_unknown = &scoreboard[2 * (event_type + 26 * handle.Index)];
		auto v7 = a4 + *((short *)scoreboard_unknown + 2);

		short v8;

		if (v7 <= -30000 || v7 < 30000)
		{
			v8 = -30000;
			if (v7 > -30000)
				v8 = a4 + *((short *)scoreboard_unknown + 2);
		}
		else
		{
			v8 = 30000;
		}

		*((short *)scoreboard_unknown + 2) = v8;

		auto v9 = 0;

		if (handle.Index >= 0x20u)
			v9 = 1 << *((char *)&handle.Handle);

		auto v10 = v9 ^ (1 << *((char *)&handle.Handle));

		if (handle.Index >= 0x40u)
			v9 ^= 1 << *((char *)&handle.Handle);

		auto v13 = v10;
		auto v14 = v9;

		((char(__cdecl *)(void *))0x4B2A70)(&v13);

		if (player_stat_type != -1)
			((void(__cdecl *)(int, int, int, int))0x5704A0)(handle.Index, -1, player_stat_type, *((signed __int16 *)scoreboard_unknown + 2));

		if (a6)
		{
			if (game_get_current_engine() && game_is_team_game())
			{
				auto player_datum = (Blam::Players::PlayerDatum*)data_array_sub_55B710(ElDorito::Instance().GetMainTls(0x40).Read<Blam::DataArray<Blam::Players::PlayerDatum>*>(), handle);

				if (player_datum)
				{
					auto team_index = player_datum->Properties.TeamIndex;

					if (team_index >= 0 && team_index < 8)
						scoreboard_sub_6E5A90(scoreboard, team_index, event_type, a4, player_stat_type);
				}
			}
		}
	}

	bool __cdecl campaign_metagame_update()
	{
		if (Modules::ModuleTweaks::Instance().VarDisableMetagame->ValueInt != 0)
			return false;

		return ((bool(__cdecl *)())0x60A4F0)();
	}

	bool __cdecl is_multithreaded_sub_42E2C0()
	{
		return Modules::ModuleTweaks::Instance().VarSinglethreaded->ValueInt == 0;
	}

	void __cdecl sub_6948C0_hook(int a1)
	{
		const auto sub_694430 = (int(__fastcall *)(void * /*this*/, void * /*unused*/, int))0x694430;

		auto *tls = (Blam::Memory::tls_data *)ElDorito::Instance().GetMainTls();

		if (tls->decal_system == nullptr || *(long *)0x46DE700 <= 0)
			return;

		for (int v1 = 0, *v2 = (int *)0x46DE718; v1 < *(long *)0x46DE700; v1++, v2 += 0x25D)
		{
			if (v2[0x258] == 0)
				continue;

			auto v3 = *v2;
			auto decalDatumIndex = *(unsigned short *)(v3 + 8);

			if (decalDatumIndex == 0xBABA)
				continue;

			auto decalTagIndex = (*tls->decal_system)[decalDatumIndex].tag_index;

			if (decalTagIndex == -1)
				continue;

			auto *decs = Blam::Tags::TagInstance(decalTagIndex & 0xFFFF).GetDefinition<Blam::Tags::Effects::DecalSystem>();

			auto decalBlockIndex = *(DWORD *)(v3 + 4);

			if (decalBlockIndex == 0xBABA)
				continue;

			if (a1 == decs->Decal[decalBlockIndex].Unknown)
			{
				if (tls->decal_system != nullptr)
					sub_694430((void *)*v2, nullptr, 0);
			}
		}
	}

	void *__cdecl datum_get_hook(Blam::DataArrayBase *array, Blam::DatumHandle handle)
	{
		if (array == nullptr)
			return nullptr;

		return array->Get(handle);
	}
}

namespace Patches::Core
{
	void ApplyAll()
	{
		// Enable tag edits
		Patch(0x101A5B, { 0xEB }).Apply();
		Patch::NopFill(Pointer::Base(0x102874), 2);
		Patch::NopFill(Pointer::Base(0x1030AA), 2);

		// No --account args patch
		Patch(0x43731A, { 0xEB, 0x0E }).Apply();
		Patch(0x4373AD, { 0xEB, 0x03 }).Apply();

		// prevent hf2p services from being registered
		Patch(0x003B8810, { 0xC3 }).Apply();

		// Remove preferences.dat hash check
		Patch::NopFill(Pointer::Base(0x10C99A), 0x6);

		// post-processing to default preferences
		Hook(0x10A491, preferences_set_defaults_hook, HookFlags::IsCall).Apply();
		Hook(0x10C87D, preferences_set_defaults_hook, HookFlags::IsCall).Apply();
		Hook(0x10CA3C, preferences_set_defaults_hook, HookFlags::IsCall).Apply();

		// Patch to allow spawning AI through effects
		Patch::NopFill(Pointer::Base(0x1033321), 2);

		// Fix random colored lighting
		Patch(0x14F2FFC, { 0x0, 0x0, 0x0, 0x0 }).Apply();

		// Maps folder override
		Hook(0x101FC0, GetMapsFolderHook).Apply();
		SetMapsFolder("maps\\");

		// Run callbacks on engine shutdown
		Hook(0x2EBD7, ShutdownHook, HookFlags::IsCall).Apply();

		// Map loading
		Hook(0x10FC2C, LoadMapHook, HookFlags::IsCall).Apply();
		Hook(0x1671BE, LoadMapHook, HookFlags::IsCall).Apply();
		Hook(0x167B4F, LoadMapHook, HookFlags::IsCall).Apply();

		Hook(0x14C7FF, LoadLevelHook, HookFlags::IsCall).Apply();

		Hook(0x152C15, GameStartHook, HookFlags::IsCall).Apply();
		Hook(0x14EB62, GameStartHook, HookFlags::IsCall).Apply();
		Hook(0x14EB54, GameStartHook, HookFlags::IsCall).Apply();

		// Hook game ticks
		Hook(0x105ABA, GameTickHook, HookFlags::IsCall).Apply();
		Hook(0x105AD7, GameTickHook, HookFlags::IsCall).Apply();
		Hook(0x1063E6, GameTickHook, HookFlags::IsCall).Apply();

		// Used to call Patches::ApplyAfterTagsLoaded when tags have loaded
		Hook(0x1030EA, TagsLoadedHook).Apply();

#ifndef _DEBUG
		// Dirty disk error at 0x0xA9F6D0 is disabled in this build
		Hook(0x69F6C0, DirtyDiskErrorHook).Apply();
#endif

		// Prevent FOV from being overridden when the game loads
		Patch::NopFill(Pointer::Base(0x25FA79), 10);
		Patch::NopFill(Pointer::Base(0x25FA86), 5);
		Hook(0x10CA02, FovHook).Apply();
		Hook(0x663B36, ActiveCamoViewModelClipHook, HookFlags::IsCall).Apply();
		// fix active camo issue with low shadow quality (local unit shadows will still be rendered)
		Patch(0x66B0CE, 0x90, 6).Apply();

		//Fix aspect ratio not matching resolution
		Hook(0x6648C9, AspectRatioHook, HookFlags::IsCall).Apply();
		Hook(0x216487, AspectRatioHook, HookFlags::IsCall).Apply();

		//Disable converting the game's resolution to 16:9
		Patch::NopFill(Pointer::Base(0x62217D), 2);
		Patch::NopFill(Pointer::Base(0x622183), 6);

		//Allow the user to select any resolution that Windows supports in the settings screen.
		Patch::NopFill(Pointer::Base(0x10BF1B), 2);
		Patch::NopFill(Pointer::Base(0x10BF21), 6);

		// post-processing to default preferences
		//Hook(0x10A491, preferences_set_defaults_hook, HookFlags::IsCall).Apply();
		//Hook(0x10C87D, preferences_set_defaults_hook, HookFlags::IsCall).Apply();
		//Hook(0x10CA3C, preferences_set_defaults_hook, HookFlags::IsCall).Apply();

		// Prevent game variant weapons from being overridden
		Pointer::Base(0x1A315F).Write<uint8_t>(0xEB);
		Pointer::Base(0x1A31A4).Write<uint8_t>(0xEB);
		Hook(0x1A3267, GrenadeLoadoutHook).Apply();

		// Remove exception handlers
		/*Patch::NopFill(Pointer::Base(0x2EA2B), 6);
		Patch::NopFill(Pointer::Base(0x2EC10), 6);
		//Patch::NopFill(Pointer::Base(0x7FC411), 6);
		Patch(0x7FC40B, { 0xC3 }).Apply();
		Patch(0x7FC42E, { 0xC3 }).Apply();
		Patch::NopFill(Pointer::Base(0x106057), 5);*/

		Hook(0x324701, EdgeDropHook, HookFlags::IsCall).Apply();
		// Fixes an issue where biped feet are just below the zone bottom causing 
		// it not to register flag caps, teleporter usage etc..
		Hook(0x7A111B, BipedFeetZoneOffsetHook, HookFlags::IsCall).Apply();

		Hook(0x10590B, GetBinkVideoPathHook, HookFlags::IsCall).Apply();

		Hook(0x20F4AD, GetScreenshotFolderHook, HookFlags::IsCall).Apply();
		Hook(0x20F44B, GetScreenshotFolderHook, HookFlags::IsCall).Apply();

		Pointer(0x530FAA).Write<float>(7); // podium duration in seconds

		// fixes the amd freeze
		Hook(0x658061, ContrailFixHook).Apply();
		// prevent hill zone luminosity from dropping below the visible threshold
		Hook(0x5D6B1C, HillColorHook).Apply();

		// hsc print functionality
		Hook(0x32FE9A, HsPrintHook, HookFlags::IsCall).Apply();

		// level data get
		Hook(0x1322D0, LevelDataGetHook).Apply();

		//Hook(0x769CF0, unit_action_submit_hook).Apply();

		// game state reading/writing
		Hook(0x25DB10, game_state_read_file_from_storage).Apply();
		Hook(0x1265E0, read_campaign_save_file_blocking).Apply();
		Hook(0x1266E0, game_state_read_header_from_persistent_storage_blocking).Apply();
		Hook(0x1270F0, game_state_write_file_to_storage_blocking).Apply();
		Hook(0x25DBE0, game_state_write_file_to_storage).Apply();
		Hook(0x109020, hash_verification).Apply();
		Hook(0x1254A0, campaign_save_exists).Apply();

		// campaign metagame hacks
		Hook(0x2E59A0, campaign_scoring_sub_6E59A0).Apply();
		Hook(0x1332E9, campaign_metagame_update, HookFlags::IsCall).Apply();
		Hook(0x1338E7, campaign_metagame_update, HookFlags::IsCall).Apply();

		// optional single or multithreading
		Hook(0x2E2C0, is_multithreaded_sub_42E2C0).Apply();

		// decal hack
		Hook(0x2947FE, sub_6948C0_hook, HookFlags::IsCall).Apply();
		Hook(0x15B6D0, datum_get_hook).Apply();
	}

	void OnShutdown(ShutdownCallback callback)
	{
		shutdownCallbacks.push_back(callback);
	}

	void ExecuteShutdownCallbacks()
	{
		for (auto &&callback : shutdownCallbacks)
			callback();
	}

	void OnMapLoaded(MapLoadedCallback callback)
	{
		mapLoadedCallbacks.push_back(callback);
	}

	void SetMapsFolder(const std::string &path)
	{
		MapsFolder = path;
		MapFormatString = MapsFolder + "%s.map";
		StringIdsPath = MapsFolder + "string_ids.dat";
		TagsPath = MapsFolder + "tags.dat";
		TagListPath = MapsFolder + "tag_list.csv";
		ResourcesPath = MapsFolder + "resources.dat";
		ResourcesBPath = MapsFolder + "resources_b.dat";
		TexturesPath = MapsFolder + "textures.dat";
		TexturesBPath = MapsFolder + "textures_b.dat";
		AudioPath = MapsFolder + "audio.dat";
		FontsPath = MapsFolder + "fonts\\";

		Pointer::Base(0x1AC050).Write(MapFormatString.c_str());

		Pointer::Base(0x149CFEC).Write(StringIdsPath.c_str());
		Pointer::Base(0x149CFF0).Write(TagsPath.c_str());
		Pointer::Base(0x149CFF4).Write(TagListPath.c_str());
		Pointer::Base(0x149CFF8).Write(ResourcesPath.c_str());
		Pointer::Base(0x149D008).Write(ResourcesBPath.c_str());
		Pointer::Base(0x149CFFC).Write(TexturesPath.c_str());
		Pointer::Base(0x149D000).Write(TexturesBPath.c_str());
		Pointer::Base(0x149D004).Write(AudioPath.c_str());

		Pointer::Base(0x149D358).Write(FontsPath.c_str());
		Pointer::Base(0x149D35C).Write(FontsPath.c_str());

		//Update the list of maps
		Modules::ModuleGame::Instance().UpdateMapList();
		Modules::ModuleGame::Instance().UpdateCustomMapList();
	}

	void OnGameStart(GameStartCallback callback)
	{
		gameStartCallbacks.push_back(callback);
	}
}

namespace
{
	void GameTickHook()
	{
		// Tick ElDorito
		ElDorito::Instance().Tick();

		// Call replaced function
		typedef void(*sub_5547F0_Ptr)();
		auto sub_5547F0 = reinterpret_cast<sub_5547F0_Ptr>(0x5547F0);
		sub_5547F0();
	}

	void ShutdownHook()
	{
		Patches::Core::ExecuteShutdownCallbacks();

		typedef void(*EngineShutdownPtr)();
		auto EngineShutdown = reinterpret_cast<EngineShutdownPtr>(0x42E410);
		EngineShutdown();
	}

	const char* GetMapsFolderHook()
	{
		return MapsFolder.c_str();
	}

	void ApplyDefaultScreenFx()
	{
		static const auto GetLocalPlayerUnitObjectIndex = (uint32_t(*)(int playerIndex))0x589CC0;
		static const auto SpawnScreenEffect = (void(*)(uint32_t tagIndex, uint32_t unitObjectIndex, int a3, void* a4, void* a5))0x683060;

		auto scnrDefinition = Blam::Tags::Scenario::GetCurrentScenario();
		if (scnrDefinition->DefaultScreenFx.TagIndex == -1)
			return;

		SpawnScreenEffect(scnrDefinition->DefaultScreenFx.TagIndex, -1, -1, *(void **)0x189CF00, *(void **)0x189CF60);
	}

	static const auto game_is_campaign = (BOOL(*)())0x531A60;

	int get_spartan_representation_index()
	{
		static Blam::Text::StringID *campaign_name = nullptr;
		static Blam::Text::StringID *multiplayer_name = nullptr;

		if (campaign_name == nullptr)
		{
			campaign_name = new Blam::Text::StringID;
			*campaign_name = Blam::Cache::StringIDCache::Instance.GetStringID("spartan");
		}

		if (multiplayer_name == nullptr)
		{
			multiplayer_name = new Blam::Text::StringID;
			*multiplayer_name = Blam::Cache::StringIDCache::Instance.GetStringID("mp_spartan");
		}

		auto name = game_is_campaign() ? *campaign_name : *multiplayer_name;

		auto matg = Blam::Tags::TagInstance::Find('matg', "globals\\globals").GetDefinition<Blam::Tags::Game::Globals>();

		for (auto i = 0; i < matg->PlayerRepresentation.Count; i++)
			if (name == matg->PlayerRepresentation[i].Name)
				return i;

		return 2;
	}

	int get_elite_representation_index()
	{
		static Blam::Text::StringID *campaign_name = nullptr;
		static Blam::Text::StringID *multiplayer_name = nullptr;

		if (campaign_name == nullptr)
		{
			campaign_name = new Blam::Text::StringID;
			*campaign_name = Blam::Cache::StringIDCache::Instance.GetStringID("sp_elite");
		}

		if (multiplayer_name == nullptr)
		{
			multiplayer_name = new Blam::Text::StringID;
			*multiplayer_name = Blam::Cache::StringIDCache::Instance.GetStringID("mp_elite");
		}

		auto name = game_is_campaign() ? *campaign_name : *multiplayer_name;

		auto matg = Blam::Tags::TagInstance::Find('matg', "globals\\globals").GetDefinition<Blam::Tags::Game::Globals>();

		for (auto i = 0; i < matg->PlayerRepresentation.Count; i++)
			if (name == matg->PlayerRepresentation[i].Name)
				return i;

		return 3;
	}

	bool LoadMapHook(Blam::LevelData *data)
	{
		typedef bool(*LoadMapPtr)(Blam::LevelData *data);
		auto LoadMap = reinterpret_cast<LoadMapPtr>(0x566EF0);
		if (!LoadMap(data))
			return false;

		*(int *)0x537483 = get_spartan_representation_index();
		*(int *)0x53761B = get_spartan_representation_index();
		*(int *)0x539EBB = get_spartan_representation_index();
		*(int *)0x539FBE = get_spartan_representation_index();
		*(int *)0x53A739 = get_spartan_representation_index();
		*(int *)0x53A7C7 = get_spartan_representation_index();

		*(int *)0x537490 = get_elite_representation_index();
		*(int *)0x537628 = get_elite_representation_index();
		*(int *)0x539EC7 = get_elite_representation_index();
		*(int *)0x539FCB = get_elite_representation_index();
		*(int *)0x53A746 = get_elite_representation_index();
		*(int *)0x53A7BF = get_elite_representation_index();

		ApplyDefaultScreenFx();

		const auto soundSystemPtr = (uint8_t**)(0x018BC9C8);
		if (soundSystemPtr)
		{
			// unmute ambient sounds
			if (*(float *)((*soundSystemPtr) + 0x44) == 0.0f)
				*(float *)((*soundSystemPtr) + 0x44) = 1.0f;
		}

		for (auto &&callback : mapLoadedCallbacks)
			callback(data->ScenarioPath); // hax

		return true;
	}

	void LoadLevelHook(uint8_t* mapinfo, char a2, char *mapsPath, char a4)
	{
		typedef int(__cdecl *LoadLevelPtr)(uint8_t* mapinfo, char a2, char *mapsPath, char a4);
		auto LoadLevel = reinterpret_cast<LoadLevelPtr>(0x0054A6C0);

		*reinterpret_cast<uint32_t*>(mapinfo + 0x111C) = 0x08081002;
		*reinterpret_cast<uint32_t*>(mapinfo + 0x1120) = 0x08080808;
		*reinterpret_cast<uint32_t*>(mapinfo + 0x1124) = 0x08080808;

		LoadLevel(mapinfo, a2, mapsPath, a4);
	}

	void GameStartHook()
	{
		typedef void(*GameStartPtr)();
		auto GameStart = reinterpret_cast<GameStartPtr>(0x551590);

		GameStart();

		auto engineGlobalsPtr = ElDorito::GetMainTls(0x48);
		if (!engineGlobalsPtr)
			return;

		auto engineGobals = engineGlobalsPtr[0];

		// fix in-game team switching for engines that support it
		engineGobals(0x8).WriteFast(engineGobals(0x4).Read<uint32_t>());

		for (auto& callback : gameStartCallbacks)
			callback();
	}

	__declspec(naked) void TagsLoadedHook()
	{
		__asm
		{
			call Patches::ApplyAfterTagsLoaded
			push 0x6D617467
			push 0x5030EF
			ret
		}
	}

	__declspec(naked) void FovHook()
	{
		__asm
		{
			// Override the FOV that the memmove before this sets
			mov eax, ds:[0x189D42C]
			mov ds : [0x2301D98], eax
			mov ecx, [edi + 0x18]
			push 0x50CA08
			ret
		}
	}

	void ActiveCamoViewModelClipHook(float *nearPlane, float *farPlane)
	{
		// not a proper fix, but it'll work for now

		const auto view_get_clip_planes = (void(*)(float *nearPlane, float *farPlane))(0x00A25AA0);

		const auto playerIndex = Blam::Players::GetLocalPlayer(0);
		const Blam::Players::PlayerDatum *player;
		if (playerIndex != Blam::DatumHandle::Null && (player = Blam::Players::GetPlayers().Get(playerIndex)))
		{
			auto unitObject = Blam::Objects::Get(player->SlaveUnit);
			float activeCamoPower = 0.0f;

			if (unitObject)
			{
				const auto globalNear = *(float*)0x0191068C;
				const auto globalFar = *(float*)0x01910690;

				if ((activeCamoPower = *(float*)((uint8_t*)unitObject + 0x3F4)) > 0)
				{
					*nearPlane = globalNear * 3.25f;
					*farPlane = globalFar;
					return;
				}

				if (unitObject->Scale < 0.4)
				{
					*nearPlane = globalNear * std::max(2.0f, 10.0f * (unitObject->Scale / 0.4f));
					*farPlane = globalFar;
					return;
				}
			}
		}

		view_get_clip_planes(nearPlane, farPlane);
	}

	void GrenadeLoadoutHookImpl(uint8_t* unit)
	{
		// Based off of 0x8227B48C in H3 non-TU

		// TODO: Clean this up, hardcoded offsets are hacky
		const size_t GrenadeCountOffset = 0x320;
		const size_t ControllingPlayerOffset = 0x198;
		auto grenadeCounts = unit + GrenadeCountOffset; // 0 = frag, 1 = plasma, 2 = spike, 3 = firebomb
		auto playerIndex = *reinterpret_cast<int16_t*>(unit + ControllingPlayerOffset);
		if (playerIndex < 0)
		{
			memset(grenadeCounts, 0, 4);
			return;
		}

		// Get the player's grenade setting
		auto &players = Blam::Players::GetPlayers();
		auto grenadeSetting = players[playerIndex].SpawnGrenadeSetting;

		// Get the current scenario tag
		auto scenario = Blam::Tags::Scenario::GetCurrentScenario();

		// If the setting is none (2) or the scenario has invalid starting
		// profile data, set the grenade counts to 0 and return
		if (grenadeSetting == 2 || !scenario->PlayerStartingProfile)
		{
			memset(grenadeCounts, 0, 4);
			return;
		}

		// Load the grenade counts from the scenario tag
		auto profile = &scenario->PlayerStartingProfile[0];
		grenadeCounts[0] = profile->StartingFragGrenadeCount;
		grenadeCounts[1] = profile->StartingPlasmaGrenadeCount;
		grenadeCounts[2] = profile->StartingSpikeGrenadeCount;
		grenadeCounts[3] = profile->StartingFirebombGrenadeCount;
	}

	__declspec(naked) void GrenadeLoadoutHook()
	{
		__asm
		{
			push edi // Unit object data
			call GrenadeLoadoutHookImpl
			add esp, 4
			push 0x5A32C7
			ret
		}
	}

	double AspectRatioHook()
	{
		int* gameResolution = reinterpret_cast<int*>(0x19106C0);
		return ((double)gameResolution[0] / (double)gameResolution[1]);
	}

	float GetNewPhysicsRate()
	{
		auto *tls = (Blam::Memory::tls_data *)ElDorito::Instance().GetMainTls();
		auto gameGlobalsPtr = tls->game_globals;
		if (!gameGlobalsPtr || gameGlobalsPtr->level_data.FrameLimit == 60)
			return 0.5f;

		return 30.0f / gameGlobalsPtr->level_data.FrameLimit;
	}

	void __fastcall EdgeDropHook(void* thisptr, void* unused, int a2, int a3, int a4, float* a5)
	{
		static auto& modulePlayer = Modules::ModulePlayer::Instance();

		Pointer(a3)(0xAC).WriteFast<float>(GetNewPhysicsRate());

		static auto sub_724BB0 = (void(__thiscall*)(void* thisptr, int a2, int a3, int a4, float* a5))(0x724BB0);
		sub_724BB0(thisptr, a2, a3, a4, a5);
	}

	void __cdecl BipedFeetZoneOffsetHook(uint32_t bipedObjectIndex, Blam::Math::RealVector3D *position, float *height, float *radius)
	{
		const auto sub_B6E850 = (void(*)(uint32_t unitObjectIndex, Blam::Math::RealVector3D *outPosition, float *a3, float *outRadius))(0xB6E850);
		sub_B6E850(bipedObjectIndex, position, height, radius);
		auto bipedObject = Blam::Objects::Get(bipedObjectIndex);
		if (bipedObject)
			*position += bipedObject->Up * 0.05f; // offset feet
	}

	char GetBinkVideoPathHook(int p_VideoID, char *p_DestBuf)
	{
		if (Modules::ModuleGame::Instance().VarSkipIntroVideos->ValueInt == 1)
			// Tell the game that there is no video with that ID
			return 0;

		const auto GetBinkVideoPath = reinterpret_cast<char(*)(int, char*)>(0xA99120);
		return GetBinkVideoPath(p_VideoID, p_DestBuf);
	}

	void DirtyDiskErrorHookImpl()
	{
		MessageBoxA(NULL, "Dirty Disk Error! Your tags are invalid or corrupted.\nIf you have any mods installed please verify that\nthey are installed in the correct order.\n\nElDewrito will now exit.", "There once was a man from Bungie...", MB_ICONERROR);
		Patches::Core::ExecuteShutdownCallbacks();
		std::exit(0);
	}

	__declspec(naked) void DirtyDiskErrorHook()
	{
		// "There once was a man from Bungie...... Nothing rhymes with Bungie.but he got a dirty disc error for the Alpha or Profile Build and exploded.  The End."
		__asm
		{
			call DirtyDiskErrorHookImpl;
		}
	}

	int __cdecl GetScreenshotFolderHook(wchar_t *path)
	{
		std::wstring_convert<std::codecvt_utf8<wchar_t>> wstring_to_string;
		std::wstring unprocessed_path = wstring_to_string.from_bytes(Modules::ModuleGame::Instance().VarScreenshotsFolder->ValueString);

		DWORD return_code = ExpandEnvironmentStringsW(unprocessed_path.c_str(), path, MAX_PATH);

		if (return_code == 0 || return_code > MAX_PATH) {// fall back to default
			static auto GetScreenshotsFolder = (int(__cdecl*)(wchar_t *path))(0x724BB0);
			return GetScreenshotsFolder(path);
		}
		boost::filesystem::path dir(path);
		dir = boost::filesystem::weakly_canonical(dir);
		wcsncpy(path, dir.c_str(), 0x100);
		return SHCreateDirectoryExW(NULL, path, NULL);
	}

	void __cdecl HsPrintHook(const char *message)
	{
		if (message == nullptr)
			return;

		Console::WriteLine(message);
	}

	__declspec(naked) void ContrailFixHook()
	{
		__asm
		{
			add ecx, [0x00A58061]
			cmp ecx, -1
			jg render
			push 0xA580BE
			retn
			render :
			push 0xA58067
				retn
		}
	}

	void HillColor(Blam::Math::RealColorARGB &color)
	{
		// if the color luminosity is less than the visible threshold, bump it up
		auto l = 0.2126f*color.Red + 0.7152*color.Green + 0.0722*color.Blue;
		if (l < 0.1f)
		{
			color.Red += 0.1f;
			color.Green += 0.1f;
			color.Blue += 0.1f;
		}
	}

	__declspec(naked) void HillColorHook()
	{
		__asm
		{
			lea edi, [ebp - 0x1C]
			push ebx
			push ecx
			push edi
			call HillColor
			pop edi
			pop ecx
			pop ebx
			movq xmm0, qword ptr[edi]
			push 0x009D6B21
			retn
		}
	}

	bool IsForgeVariantLoaded(std::string name, char *MapVariant)
	{
		if (std::string(Utils::String::ThinString(((Blam::MapVariant *)MapVariant)->ContentHeader.Name)).find(name) == std::string::npos)
			return false;
		return true;
	}

	bool IsGameVariantLoaded(std::string name, char *GameVariant)
	{
		if (std::string(Utils::String::ThinString(((Blam::GameVariant *)GameVariant)->Name)).find(name) == std::string::npos)
			return false;
		return true;
	}

	bool IsMap(Blam::LevelData *data, std::string map_name)
	{
		if (std::string(data->ScenarioPath).find(map_name) == std::string::npos)
			return false;
		return true;
	}

	// todo: make these user configurable, campaign_prefs.cfg?
	void *NewLevelData(Blam::LevelData *data)
	{
		if (data->MapType == Blam::eMapTypeCampaign)
		{
			auto moduleCampaign = &Modules::ModuleCampaign::Instance();
			data->FrameLimit = (int16_t)moduleCampaign->VarFrameLimit->ValueInt;
			data->CampaignDifficultyLevel = (Blam::CampaignDifficultyLevel)moduleCampaign->VarDifficultyLevel->ValueInt;
			data->CampaignInsertionPoint = (Blam::CampaignInsertionPoint)moduleCampaign->VarInsertionPoint->ValueInt;
			data->CampaignMetagameScoringOption = (Blam::CampaignMetagameScoringOption)moduleCampaign->VarMetagameScoringOption->ValueInt;
			data->CampaignMetagameEnabled = (bool)moduleCampaign->VarMetagameEnabled->ValueInt;
			data->SurvivalModeEnabled = (bool)moduleCampaign->VarSurvivalModeEnabled->ValueInt;
		}
		else
			if (Pointer(0x165C83C).Read<int>() != 60)
				Pointer(0x165C83C).Write<int>(60);

		return (void *)data;
	}

	void *__cdecl LevelDataGetHook()
	{
		auto *tls = (Blam::Memory::tls_data *)ElDorito::Instance().GetMainTls();
		auto gameGlobalsPtr = tls->game_globals;
		if (!gameGlobalsPtr)
			return false;

		return NewLevelData(&gameGlobalsPtr->level_data);
	}

	void PrintLevelInfo()
	{
		auto *tls = (Blam::Memory::tls_data *)ElDorito::Instance().GetMainTls();
		auto gameGlobalsPtr = tls->game_globals;
		if (!gameGlobalsPtr)
			return;
		auto LevelData = &gameGlobalsPtr->level_data;

		std::string bool_string[2]
		{
			"False",
			"True"
		};

		std::stringstream ss;
		ss << "Level Info:" << std::endl;

		ss << "\tBSP is loaded: " << bool_string[gameGlobalsPtr->bsp_load_state] << std::endl << std::endl;

		ss << "\tFrame Limit: " << LevelData->FrameLimit << std::endl;
		ss << "\tGame Simulation: " << Blam::GameSimulationNames[LevelData->GameSimulation] << std::endl;
		ss << "\tDeterminism Version: " << LevelData->DeterminismVersion << std::endl << std::endl;

		ss << "\tGame Playback: " << Blam::GamePlaybackNames[LevelData->GamePlayback] << std::endl;
		ss << "\tTheater Mode: " << bool_string[LevelData->GamePlayback == Blam::eGamePlaybackLocal] << std::endl << std::endl;

		ss << "\tCinematic Debug Mode: " << bool_string[Pointer(0x24B0A3E).Read<bool>()] << std::endl << std::endl;

		ss << "\tMap type: " << Blam::MapTypeNames[LevelData->MapType] << std::endl;

		ss << "\tMap Id: " << LevelData->MapId << std::endl;
		ss << "\tMap Path: " << LevelData->ScenarioPath << std::endl << std::endl;

		if (LevelData->MapType == Blam::eMapTypeCampaign)
		{
			ss << "\tCampaign Id: " << LevelData->CampaignId << std::endl;
			ss << "\tRally Point: " << Blam::CampaignInsertionPointNames[LevelData->CampaignInsertionPoint] << std::endl;
			ss << "\tZoneset Index: " << LevelData->ZonesetIndex << std::endl << std::endl;

			ss << "\tCampaign Difficulty: " << Blam::GameDifficultNames[LevelData->CampaignDifficultyLevel] << std::endl;
			ss << "\tMetagame Enable: " << bool_string[LevelData->CampaignMetagameEnabled] << std::endl;
			ss << "\tMetagame Scoring: " << Blam::CampaignMetagameScoringOptionNames[LevelData->CampaignMetagameScoringOption] << std::endl;
			ss << "\tSurvival Mode Enable: " << bool_string[LevelData->SurvivalModeEnabled] << std::endl << std::endl;

			// TODO: figure out / fix
			//ss << "\tActive Primary Skulls: " << Blam::SkullNames[LevelData->CampaignSkullsPrimary] << std::endl;
			//ss << "\tActive Secondary Skulls: " << Blam::SkullNames[LevelData->CampaignSkullsSecondary] << std::endl << std::endl;
		}
		else
		{
			auto GameVariant = (Blam::GameVariant *)LevelData->GameVariant;
			ss << "\t(Game Variant)->Game Type: " << Blam::GameTypeNames[GameVariant->GameType] << std::endl;
			ss << "\t(Game Variant)->Name: " << Utils::String::ThinString(GameVariant->Name) << std::endl;
			ss << "\t(Game Variant)->Author: " << GameVariant->Author << std::endl;
			ss << "\t(Game Variant)->Description: " << GameVariant->Description << std::endl << std::endl;

			auto MapVariant = (Blam::MapVariant *)LevelData->MapVariant;
			ss << "\t(Map Variant)->Base Map Id: " << MapVariant->MapId << std::endl;
			ss << "\t(Map Variant)->Name: " << Utils::String::ThinString(MapVariant->ContentHeader.Name) << std::endl;
			ss << "\t(Map Variant)->Author: " << MapVariant->ContentHeader.Author << std::endl;
			ss << "\t(Map Variant)->Description: " << MapVariant->ContentHeader.Description << std::endl << std::endl;

			std::string TeamNames[9]
			{
				"Red",
				"Blue",
				"Green",
				"Orange",
				"Purple",
				"Gold",
				"Brown",
				"Pink",
				"Observer"
			};

			ss << "Initial Participants Array:" << std::endl;

			auto player_index = 0;
			auto participant = LevelData->InitialParticipantsArray;
			do
			{
				auto properties = (Blam::Players::PlayerProperties *)participant->PlayerProperties;
				if (participant->IsValid)
				{
					ss << "\tPlayer " << player_index + 1 << ": ";
					ss << Utils::String::ThinString(properties->DisplayName);
					ss << ", " << Utils::String::ThinString(properties->ServiceTag);

					switch (GameVariant->GameType)
					{
					case Blam::eGameTypeInfection:
						ss << ", " << (properties->TeamIndex ? "Human" : "Zombie");
					default:
						ss << ", " << TeamNames[properties->TeamIndex];
						break;
					}

					//ss << ", " << (properties->PlayerRepresentation ? "Spartan" : "Monitor");
					//ss << ", " << (properties->Gender ? "Female" : "Male");
					//ss << ", " << std::hex << properties->Uid;

					ss << std::endl;
				}
				++player_index;
				++participant;
			} while (player_index < 16);
		}
		ss << std::endl << std::endl;

		if (!IsMap(LevelData, "mainmenu"))
			Console::WriteLine(ss.str());
	}
}