#include <stdinclude.hpp>

#include <minizip/unzip.h>
#include <TlHelp32.h>

#include <unordered_set>
#include <charconv>
#include <cassert>
#include <format>
#include <cpprest/uri.h>
#include <cpprest/http_listener.h>
#include <ranges>
#include <mhotkey.hpp>

extern bool init_hook();
extern void uninit_hook();
extern void start_console();

using namespace web;
using namespace http;
using namespace utility;
using namespace http::experimental::listener;


bool g_enable_plugin = true;
bool g_enable_console = true;
bool g_auto_dump_all_json = false;
bool g_dump_untrans_lyrics = false;
bool g_dump_untrans_unlocal = false;
SubtitleConfig g_subtitle_config;
int g_max_fps = 60;
int g_vsync_count = 0;
float g_3d_resolution_scale = 1.0f;
std::string g_custom_font_path = "";
char hotKey = 'u';
int reloadKey = VK_F5;  // Default F5. Set to 0 to disable; non-zero = virtual key code for hot-reload
float g_font_size_offset = -3.0f;

bool g_enable_free_camera = false;
bool g_block_out_of_focus = false;
float g_free_camera_mouse_speed = 35;
bool g_allow_use_tryon_costume = false;
bool g_allow_same_idol = false;
bool g_unlock_all_dress = false;
bool g_unlock_all_headwear = false;
bool g_show_hidden_costumes = false;
bool g_save_and_replace_costume_changes = false;
bool g_overrie_mv_unit_idols = false;
bool g_apply_costumes_automatically = false;
bool g_override_isVocalSeparatedOn = false;
bool g_enable_chara_param_edit = false;
bool g_unlock_PIdol_and_SChara_events = false;
int g_start_resolution_w = -1;
int g_start_resolution_h = -1;
bool g_start_resolution_fullScreen = false;
bool g_reenable_clipPlane = true;
float g_nearClipPlane = 0;
float g_farClipPlane = 2;
bool g_shader_quickprobing = true;
bool g_loadasset_output = false;
bool g_extract_asset = false;
bool g_extract_asset_image = false;
bool g_extract_asset_rawimage = false;
bool g_extract_asset_renderer = false;
bool g_extract_asset_sprite = false;
bool g_extract_asset_texture2d = false;
bool g_extract_asset_log_unknown_asset = false;
bool g_magicacloth_override = false;
bool g_magicacloth_output_cloth = false;
bool g_magicacloth_output_controller = false;
float g_magicacloth_inertia_min = 1.0f;
float g_magicacloth_inertia_max = 1.0f;
float g_magicacloth_radius_min = 0.002f;
float g_magicacloth_radius_max = 0.028f;
float g_magicacloth_damping = 0.01f;
float g_magicacloth_movementSpeedLimit = 10.0f;
float g_magicacloth_rotationSpeedLimit = 1440.0f;
float g_magicacloth_localMovementSpeedLimit = 10.0f;
float g_magicacloth_localRotationSpeedLimit = 1440.0f;
float g_magicacloth_particleSpeedLimit = 40.0f;
float g_magicacloth_limitAngle = 90.0f;
float g_magicacloth_springLimitDistance = 0.5f;
float g_magicacloth_springNoise = 0.1f;


std::filesystem::path g_localify_base("scsp_localify");
constexpr const char ConfigJson[] = "scsp-config.json";

const auto CONSOLE_TITLE = L"iM@S SCSP Tools By chinosk";
bool showStartCommand = false;

namespace
{
	void create_debug_console()
	{
		AllocConsole();

		// open stdout stream
		auto _ = freopen("CONOUT$", "w+t", stdout);
		_ = freopen("CONOUT$", "w", stderr);
		_ = freopen("CONIN$", "r", stdin);

		SetConsoleTitleW(CONSOLE_TITLE);

		// set this to avoid turn japanese texts into question mark
		SetConsoleOutputCP(65001);
		std::locale::global(std::locale(""));

		wprintf(L"THEiDOLM@STER ShinyColors Song for Prism tools loaded! - By chinosk\n");
	}
}



namespace
{
	bool ReadCameraKey(std::vector<std::string>& logs, std::string configKeyName, int mapKey, rapidjson::Value& value) {
		if (value.IsString()) {
			const char* s = value.GetString();
			if (s && value.GetStringLength() == 1) {
				SCCamera::CameraControlKeyMapping[s[0]] = mapKey;
				logs.emplace_back("Key binding changed: '" + configKeyName + "' = " + std::to_string(s[0]) + "\n");
				return true;
			}
			else {
				logs.emplace_back("Invalid string input for key '" + configKeyName + "'.\n");
				return false;
			}
		}
		else if (value.IsInt()) {
			int i = value.GetInt();
			if (i >= 0 && i <= 255) {
				SCCamera::CameraControlKeyMapping[i] = mapKey;
				logs.emplace_back("Key binding changed: '" + configKeyName + "' = " + std::to_string(i) + "\n");
				return true;
			}
			else {
				logs.emplace_back("Invalid int input for key '" + configKeyName + "'.\n");
				return false;
			}
		}
		else {
			logs.emplace_back("Invalid input for key '" + configKeyName + "'.\n");
			return false;
		}
	}
#define ReadJsonKeyBinding(_STR_CONFIG_KEY_NAME_, _VAL_MAP_KEY_) \
	if (document.HasMember(_STR_CONFIG_KEY_NAME_)) { \
		ReadCameraKey(logs, _STR_CONFIG_KEY_NAME_, _VAL_MAP_KEY_, document[_STR_CONFIG_KEY_NAME_]); \
	} else { \
		SCCamera::CameraControlKeyMapping[_VAL_MAP_KEY_] = _VAL_MAP_KEY_; \
	}

#define READ_JSON_FLOAT(_txt_var_name_no_prefix_) \
	if (document.HasMember(#_txt_var_name_no_prefix_)) \
	{ g_##_txt_var_name_no_prefix_ = document[#_txt_var_name_no_prefix_].GetFloat(); }

	std::vector<std::string> read_config(std::vector<std::string>& logs)
	{
		std::ifstream config_stream{ ConfigJson };
		std::vector<std::string> dicts{};
		rapidjson::Document document;

		// If the config file does not exist, the plugin auto-creates it. When adding new config fields
		// in future updates, we won't need to bundle the config file in the release.
		if (!config_stream.is_open()) {
			document.SetObject();
		} else {
			rapidjson::IStreamWrapper wrapper{ config_stream };
			document.ParseStream(wrapper);
			config_stream.close();
		}

		if (!document.HasParseError())
		{
			bool configModified = false;
			if (document.ObjectEmpty()) configModified = true;

#define CONFIG_BOOL(key, var) \
			if (document.HasMember(key)) { \
				var = document[key].GetBool(); \
			} else { \
				document.AddMember(key, var, document.GetAllocator()); \
				configModified = true; \
			}

			CONFIG_BOOL("showStartCommand", showStartCommand);

#define CONFIG_INT(key, var) \
			if (document.HasMember(key)) { \
				var = document[key].GetInt(); \
			} else { \
				document.AddMember(key, var, document.GetAllocator()); \
				configModified = true; \
			}

#define CONFIG_FLOAT(key, var) \
			if (document.HasMember(key)) { \
				var = document[key].GetFloat(); \
			} else { \
				document.AddMember(key, var, document.GetAllocator()); \
				configModified = true; \
			}

#define CONFIG_STRING(key, var) \
			if (document.HasMember(key)) { \
				var = document[key].GetString(); \
			} else { \
				document.AddMember(key, rapidjson::Value(var.c_str(), document.GetAllocator()), document.GetAllocator()); \
				configModified = true; \
			}

			if (document.HasMember("enableVSync")) {
				bool enableVSync = document["enableVSync"].GetBool();
				g_vsync_count = enableVSync ? 1 : 0;
			} else {
				document.AddMember("enableVSync", g_vsync_count > 0, document.GetAllocator());
				configModified = true;
			}
			
			CONFIG_INT("vSyncCount", g_vsync_count);
			CONFIG_INT("maxFps", g_max_fps);
			CONFIG_FLOAT("3DResolutionScale", g_3d_resolution_scale);
			CONFIG_BOOL("enableConsole", g_enable_console);
			
			std::string localifyBasePathStr = g_localify_base.string();
			CONFIG_STRING("localifyBasePath", localifyBasePathStr);
			g_localify_base = localifyBasePathStr;

			std::string hotKeyStr(1, hotKey);
			CONFIG_STRING("hotKey", hotKeyStr);
			if (!hotKeyStr.empty()) hotKey = hotKeyStr[0];
			
			// reloadKey: default F5. Set to 0 to disable hot-reload.
			if (document.HasMember("reloadKey")) {
				const auto& v = document["reloadKey"];
				if (v.IsInt()) {
					reloadKey = v.GetInt();
				} else if (v.IsString()) {
					const char* s = v.GetString();
					size_t len = v.GetStringLength();
					if (!s || len == 0) {
						reloadKey = 0;
					} else if (len == 1) {
						reloadKey = (unsigned char)s[0];
					} else if (len == 3 && (s[0] == 'F' || s[0] == 'f') && s[1] == '1' && s[2] >= '0' && s[2] <= '2') {
						reloadKey = VK_F10 + (s[2] - '0');
					} else if (len == 2 && (s[0] == 'F' || s[0] == 'f') && s[1] >= '1' && s[1] <= '9') {
						reloadKey = VK_F1 + (s[1] - '1');
					} else {
						reloadKey = 0;
					}
				} else {
					reloadKey = 0;
				}
			} else {
				reloadKey = VK_F5;
				document.AddMember("reloadKey", rapidjson::Value("F5", document.GetAllocator()), document.GetAllocator());
				configModified = true;
			}

			CONFIG_BOOL("autoDumpAllJson", g_auto_dump_all_json);
			CONFIG_BOOL("autoDumpSubtitle", g_isDumping);
			CONFIG_BOOL("dumpUntransLyrics", g_dump_untrans_lyrics);
			CONFIG_BOOL("dumpUntransLocal2", g_dump_untrans_unlocal);
			
			CONFIG_BOOL("dualMode", g_subtitle_config.dualMode);

			if (document.HasMember("extraAssetBundlePath")) {
				logs.push_back("[WARNING] Option `extraAssetBundlePath` is obsolete. Use `asset_bundle_path::asset_path` to specify an asset.\n");
			}
			if (document.HasMember("extraAssetBundlePaths")) {
				logs.push_back("[WARNING] Option `extraAssetBundlePaths` is obsolete. Use `asset_bundle_path::asset_path` to specify an asset.\n");
			}
			
			CONFIG_STRING("customFontPath", g_custom_font_path);
			if (g_custom_font_path.find("::") == std::string::npos && !g_custom_font_path.empty()) {
				logs.push_back("[WARNING] Option `customFontPath` is set by old style; the font is assumed to be inside the default bundle. Use `asset_bundle_path::asset_path` to specify an asset.\n");
				g_custom_font_path = "scsp_localify/scsp-bundle::" + g_custom_font_path;
			}

			CONFIG_FLOAT("fontSizeOffset", g_font_size_offset);
			CONFIG_BOOL("blockOutOfFocus", g_block_out_of_focus);

			if (document.HasMember("baseFreeCamera")) {
				const auto& freeCamConfig = document["baseFreeCamera"];
				if (freeCamConfig.IsObject()) {
					if (freeCamConfig.HasMember("enable")) {
						g_enable_free_camera = freeCamConfig["enable"].GetBool();
					}
					if (freeCamConfig.HasMember("moveStep")) {
						BaseCamera::moveStep = freeCamConfig["moveStep"].GetFloat() / 1000;
					}
					if (freeCamConfig.HasMember("mouseSpeed")) {
						g_free_camera_mouse_speed = freeCamConfig["mouseSpeed"].GetFloat();
					}
				}
			} else {
				rapidjson::Value freeCamConfig(rapidjson::kObjectType);
				freeCamConfig.AddMember("enable", g_enable_free_camera, document.GetAllocator());
				freeCamConfig.AddMember("moveStep", BaseCamera::moveStep * 1000, document.GetAllocator());
				freeCamConfig.AddMember("mouseSpeed", g_free_camera_mouse_speed, document.GetAllocator());
				document.AddMember("baseFreeCamera", freeCamConfig, document.GetAllocator());
				configModified = true;
			}

			CONFIG_BOOL("allowUseTryOnCostume", g_allow_use_tryon_costume);
			CONFIG_BOOL("allowSameIdol", g_allow_same_idol);
			CONFIG_BOOL("saveAndReplaceCostumeChanges", g_save_and_replace_costume_changes);
			CONFIG_BOOL("unlockPIdolAndSCharaEvents", g_unlock_PIdol_and_SChara_events);

			if (document.HasMember("startResolution")) {
				auto& startResolution = document["startResolution"];
				g_start_resolution_w = startResolution["w"].GetInt();
				g_start_resolution_h = startResolution["h"].GetInt();
				g_start_resolution_fullScreen = startResolution["isFull"].GetBool();
			} else {
				rapidjson::Value startResolution(rapidjson::kObjectType);
				startResolution.AddMember("w", 1280, document.GetAllocator());
				startResolution.AddMember("h", 720, document.GetAllocator());
				startResolution.AddMember("isFull", false, document.GetAllocator());
				document.AddMember("startResolution", startResolution, document.GetAllocator());
				configModified = true;
				// Update globals just in case they were -1
				if (g_start_resolution_w == -1) g_start_resolution_w = 1280;
				if (g_start_resolution_h == -1) g_start_resolution_h = 720;
			}

			ReadJsonKeyBinding("key_w_camera_forward", KEY_W);
			ReadJsonKeyBinding("key_s_camera_back", KEY_S);
			ReadJsonKeyBinding("key_a_camera_left", KEY_A);
			ReadJsonKeyBinding("key_d_camera_right", KEY_D);
			ReadJsonKeyBinding("key_ctrl_camera_down", KEY_CTRL);
			ReadJsonKeyBinding("key_space_camera_up", KEY_SPACE);
			ReadJsonKeyBinding("key_up_cameralookat_up", KEY_UP);
			ReadJsonKeyBinding("key_down_cameralookat_down", KEY_DOWN);
			ReadJsonKeyBinding("key_left_cameralookat_left", KEY_LEFT);
			ReadJsonKeyBinding("key_right_cameralookat_right", KEY_RIGHT);
			ReadJsonKeyBinding("key_q_camera_fov_increase", KEY_Q);
			ReadJsonKeyBinding("key_e_camera_fov_decrease", KEY_E);
			ReadJsonKeyBinding("key_r_camera_reset", KEY_R);
			ReadJsonKeyBinding("key_192_camera_mouseMove", KEY_192);

			CONFIG_BOOL("magicacloth_override", g_magicacloth_override);
			CONFIG_FLOAT("magicacloth_inertia_min", g_magicacloth_inertia_min);
			CONFIG_FLOAT("magicacloth_inertia_max", g_magicacloth_inertia_max);
			CONFIG_FLOAT("magicacloth_radius_min", g_magicacloth_radius_min);
			CONFIG_FLOAT("magicacloth_radius_max", g_magicacloth_radius_max);
			CONFIG_FLOAT("magicacloth_damping", g_magicacloth_damping);
			CONFIG_FLOAT("magicacloth_movementSpeedLimit", g_magicacloth_movementSpeedLimit);
			CONFIG_FLOAT("magicacloth_rotationSpeedLimit", g_magicacloth_rotationSpeedLimit);
			CONFIG_FLOAT("magicacloth_localMovementSpeedLimit", g_magicacloth_localMovementSpeedLimit);
			CONFIG_FLOAT("magicacloth_localRotationSpeedLimit", g_magicacloth_localRotationSpeedLimit);
			CONFIG_FLOAT("magicacloth_particleSpeedLimit", g_magicacloth_particleSpeedLimit);
			CONFIG_FLOAT("magicacloth_limitAngle", g_magicacloth_limitAngle);
			CONFIG_FLOAT("magicacloth_springLimitDistance", g_magicacloth_springLimitDistance);
			CONFIG_FLOAT("magicacloth_springNoise", g_magicacloth_springNoise);

			if (configModified) {
				std::ofstream ofs(ConfigJson);
				rapidjson::OStreamWrapper osw(ofs);
				rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
				document.Accept(writer);
			}
		}

		return dicts;
	}
}

void reloadTransData() {
	SCLocal::loadLocalTrans();
}

void reload_all_data() {
	std::vector<std::string> logs{};
	read_config(logs);
	for (const auto& log : logs) {
		printf("%s", log.c_str());
	}
	reloadTransData();
}

extern std::function<void()> g_on_hook_ready;
std::function<void()> g_reload_all_data = reload_all_data;

int __stdcall DllMain(HINSTANCE dllModule, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		// the DMM Launcher set start path to system32 wtf????
		std::string module_name;
		module_name.resize(MAX_PATH);
		module_name.resize(GetModuleFileName(nullptr, module_name.data(), MAX_PATH));

		std::filesystem::path module_path(module_name);

		// check name
		if (module_path.filename() != "imasscprism.exe")
			return 1;

		std::filesystem::current_path(
			module_path.parent_path()
		);


		std::vector<std::string> logs{};
		auto dicts = read_config(logs);

		if (g_enable_console) {
			create_debug_console();
			if (showStartCommand) {
				printf("Command: %s\n", GetCommandLineA());
			}
		}

		for (const auto& log : logs) {
			printf("%s", log.c_str());
		}

		std::thread init_thread([dicts = std::move(dicts)] {

			if (g_enable_console)
			{
				start_console();
			}

			InstallCrashHandler();

			init_hook();

			std::mutex mutex;
			std::condition_variable cond;
			std::atomic<bool> hookIsReady(false);
			g_on_hook_ready = [&]
				{
					hookIsReady.store(true, std::memory_order_release);
					cond.notify_one();
				};

			// Translation loading depends on game-version-specific pointers; load after hook is ready
			std::unique_lock lock(mutex);
			cond.wait(lock, [&] {
				return hookIsReady.load(std::memory_order_acquire);
				});
			if (g_enable_console)
			{
				auto _ = freopen("CONOUT$", "w+t", stdout);
				_ = freopen("CONOUT$", "w", stderr);
				_ = freopen("CONIN$", "r", stdin);
			}

			reloadTransData();
			SetConsoleTitleW(CONSOLE_TITLE);  // Keep console title
			});
		init_thread.detach();
	}
	else if (reason == DLL_PROCESS_DETACH)
	{
		uninit_hook();
	}
	return 1;
}
