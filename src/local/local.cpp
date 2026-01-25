#include <stdinclude.hpp>
#include "local.hpp"
#include <fstream>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <unordered_set>
#include <cctype>

using namespace std;

namespace SCLocal {
	namespace {
		std::unordered_map<std::string, std::unordered_map<int, std::string>> localTrans{};
		std::unordered_map<std::string, std::string> lrcTrans{};
		std::unordered_map<std::string, std::string> unLocalTrans_Legacy{}; // 用于存储 local2.json 等旧数据
		std::unordered_map<std::string, SubtitleData> unLocalTrans{}; // 用于存储 Timeline 数据 (UUID -> SubtitleData)
		std::unordered_set<std::string> loadedScenarios{}; // 已加载的剧情ID列表
	}

	void loadGenericTrans(const char* fileName, std::unordered_map<std::string, std::string>& transDict) {
		try {
			transDict.clear();
			std::ifstream file(g_localify_base / fileName);
			if (!file.is_open()) {
				printf("Load %s failed: file not found.\n", fileName);
				return;
			}
			std::string fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
			file.close();
			auto fileData = nlohmann::json::parse(fileContent);
			for (auto& i : fileData.items()) {
				const auto& key = i.key();
				const std::string value = i.value();
				transDict[key] = value;
			}
		}
		catch (std::exception& e) {
			printf("Load %s failed: %s\n", fileName, e.what());
		}
	}

	void loadLrcTrans() {
		loadGenericTrans("lyrics.json", lrcTrans);
	}
	void loadUnlocalTrans() {
		loadGenericTrans("local2.json", unLocalTrans_Legacy);
	}

	void loadTimelineTrans() {
		std::filesystem::path timelinePath = g_localify_base / "timeline_json";

		if (!std::filesystem::exists(timelinePath) || !std::filesystem::is_directory(timelinePath)) {
			printf("Timeline translation directory not found: %ls\n", timelinePath.c_str());
			return;
		}

		printf("Loading timeline translations (V2) from %ls...\n", timelinePath.c_str());
		int fileCount = 0;
		int itemCount = 0;
		loadedScenarios.clear();

		try {
			for (const auto& entry : std::filesystem::recursive_directory_iterator(timelinePath)) {
				if (entry.is_regular_file()) {
					auto ext = entry.path().extension().string();
					std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
					if (ext == ".json") {
						loadedScenarios.insert(entry.path().stem().string());
						try {
							std::ifstream file(entry.path());
						std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
						file.close();

						auto jsonArray = nlohmann::json::parse(content);
						if (jsonArray.is_array()) {
							for (const auto& item : jsonArray) {
								if (item.contains("uuid")) {
									std::string uuid = item["uuid"];
									SubtitleData data;

									// 1. 读取译文
									if (item.contains("translation")) {
										data.translation = item["translation"];
									}
									else if (item.contains("cn_text")) { // 兼容旧格式
										data.translation = item["cn_text"];
									}
									
									// 2. 读取原文
									if (item.contains("original")) {
										data.original = item["original"];
									}
									else if (item.contains("jp_text")) { // 兼容旧格式
										data.original = item["jp_text"];
									}

									// 3. 读取配置
									if (item.contains("config")) {
										auto& cfg = item["config"];
										if (cfg.contains("zhSize")) data.config.zhSize = cfg["zhSize"];
										if (cfg.contains("jpSize")) data.config.jpSize = cfg["jpSize"];
										if (cfg.contains("lineSpacing")) data.config.lineSpacing = cfg["lineSpacing"];
										if (cfg.contains("dualMode")) data.config.dualMode = cfg["dualMode"];
									}

									if (!uuid.empty() && !data.translation.empty()) {
										unLocalTrans[uuid] = data;
										itemCount++;
									}
								}
							}
							fileCount++;
						}
					}
					catch (std::exception& e) {
						printf("Error loading timeline file %ls: %s\n", entry.path().c_str(), e.what());
					}
				}
			}
		}
		}
		catch (std::exception& e) {
			printf("Error iterating timeline directory: %s\n", e.what());
		}

		printf("Loaded %d timeline files with %d entries.\n", fileCount, itemCount);
	}

	void loadLocalTrans() {
		loadLrcTrans();
		loadUnlocalTrans();
		loadTimelineTrans();
		localTrans.clear();
		printf("Loading localify.json...\n");
		int totalItemCount = 0;
		try {
			std::ifstream file(g_localify_base / "localify.json");
			if (!file.is_open()) {
				printf("Load localify.json failed: file not found.\n");
				return;
			}
			std::string fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
			file.close();
			auto fileData = nlohmann::json::parse(fileContent);
			for (auto& i : fileData.items()) {
				const auto& key = i.key();
				localTrans[key] = {};
				for (auto& v : i.value().items()) {
					const auto& subIdStr = v.key();
					const auto subId = std::stoi(subIdStr);
					std::string localText = v.value();
					if (auto it = unLocalTrans_Legacy.find(localText); it != unLocalTrans_Legacy.end()) {
						localText = it->second;
					}
					localTrans[key][subId] = localText;
					totalItemCount++;
				}
			}
		}
		catch (std::exception& e) {
			printf("Load localify.json failed: %s\n", e.what());
		}
		printf("%d items in localify.json loaded.\n", totalItemCount);
	}

	bool getLocalifyText(const std::string& category, int id, std::string* getStr) {
		if (auto it = localTrans.find(category); it != localTrans.end()) {
			const auto& value = it->second;
			if (auto vIt = value.find(id); vIt != value.end()) {
				*getStr = vIt->second;
				return true;
			}
		}
		return false;
	}

	/*
	Category: mlStory_MainStoryEpisode, mlMusic_CueSheet, mlMusic_MVScene
	Wait for game structure to modify these values (Placeholder)
	*/
	bool getLocalifyText(const std::wstring& category, int id, std::wstring* getStr) {
		const auto categoryS = utility::conversions::to_utf8string(category);
		std::string resultS = "";
		if (getLocalifyText(categoryS, id, &resultS)) {
			const auto resultWs = utility::conversions::to_utf16string(resultS);
			*getStr = resultWs;
			return true;
		}
		return false;
	}

	std::vector<std::wstring> split(const std::wstring& text, wchar_t delimiter) {
		std::vector<std::wstring> parts;
		std::wstring::size_type start = 0;
		std::wstring::size_type end = text.find(delimiter);
		while (end != std::wstring::npos) {
			parts.push_back(text.substr(start, end - start));
			start = end + 1;
			end = text.find(delimiter, start);
		}
		parts.push_back(text.substr(start));
		return parts;
	}

	std::filesystem::path splitFatherDirectoryByUnderline(const std::wstring& name) {
		auto parts = split(name, L'_');
		if (parts.size() == 1) {
			return ".";
		}

		std::filesystem::path filePath;
		for (int i = 0; i < 2; i++) {
			filePath /= parts[i];
		}
		return filePath;
	}

	std::filesystem::path getFilePathByName(const std::wstring& gamePath, bool createFatherPath, const std::filesystem::path& fatherBase) {
		std::filesystem::path localFileName;
		if (gamePath.starts_with(L"s")) {
			localFileName /= L"scenario";
		}
		const auto fatherPath = localFileName / splitFatherDirectoryByUnderline(gamePath);
		if (createFatherPath) {
			if (!std::filesystem::exists(fatherBase / fatherPath)) {
				std::filesystem::create_directories(fatherBase / fatherPath);
			}
		}
		return fatherPath / gamePath;
	}

	bool getLocalFileName(const std::wstring& gamePath, std::filesystem::path* localPath, bool checkExists) {
		auto localFileName = g_localify_base / getFilePathByName(gamePath, !checkExists, g_localify_base);
		if (std::filesystem::exists(localFileName) || !checkExists) {
			*localPath = localFileName;
			return true;
		}
		return false;
	}

	void dumpGenericText(const std::string& dumpStr, const char* fileName, bool withOrigText = false) {
		try {
			const std::filesystem::path dumpBasePath("dumps");
			const auto dumpFilePath = dumpBasePath / fileName;

			if (!std::filesystem::is_directory(dumpBasePath)) {
				std::filesystem::create_directories(dumpBasePath);
			}
			if (!std::filesystem::exists(dumpFilePath)) {
				std::ofstream dumpWriteLrcFile(dumpFilePath, std::ofstream::out);
				dumpWriteLrcFile << "{}";
				dumpWriteLrcFile.close();
			}

			std::ifstream dumpLrcFile(dumpFilePath);
			std::string fileContent((std::istreambuf_iterator<char>(dumpLrcFile)), std::istreambuf_iterator<char>());
			dumpLrcFile.close();
			auto fileData = nlohmann::ordered_json::parse(fileContent);
			fileData[dumpStr] = withOrigText ? dumpStr : "";
			const auto newStr = fileData.dump(4, 32, false);
			std::ofstream dumpWriteLrcFile(dumpFilePath, std::ofstream::out);
			dumpWriteLrcFile << newStr.c_str();
			dumpWriteLrcFile.close();
		}
		catch (std::exception& e) {
			printf("Dump text to %s error: %s\n", fileName, e.what());
		}

	}

	std::string replaceAll(const std::string& str, const std::string& oldStr, const std::string& newStr) {
		std::string result = str;
		size_t pos = 0;
		while ((pos = result.find(oldStr, pos)) != std::string::npos) {
			result.replace(pos, oldStr.length(), newStr);
			pos += newStr.length();
		}
		return result;
	}

	std::string getLyricsTrans(const std::wstring& orig) {
		// const auto lrcStr = replaceAll(replaceAll(utility::conversions::to_utf8string(orig), "\n", "\\n"), "\r", "\\r");
		const auto lrcStr = utility::conversions::to_utf8string(orig);
		if (auto iter = lrcTrans.find(lrcStr); iter != lrcTrans.end()) {
			return iter->second;
		}
		else {
			if (g_dump_untrans_lyrics) {
				dumpGenericText(lrcStr, "lyrics.json", true);
			}
		}
		return lrcStr;
	}

	bool getGameUnlocalTrans(const std::wstring& orig, std::string* newStr) {
		// const auto origStr = replaceAll(replaceAll(utility::conversions::to_utf8string(orig), "\n", "\\n"), "\r", "\\r");
		const auto origStr = utility::conversions::to_utf8string(orig);
		if (auto iter = unLocalTrans_Legacy.find(origStr); iter != unLocalTrans_Legacy.end()) {
			*newStr = iter->second;
			return true;
		}
		else {
			if (g_dump_untrans_unlocal) {
				dumpGenericText(origStr, "local2.json");
			}
		}
		return false;
	}

	bool getSubtitle(const std::string& key, SubtitleData& outData) {
		if (auto iter = unLocalTrans.find(key); iter != unLocalTrans.end()) {
			outData = iter->second;
			return true;
		}
		return false;
	}

	bool isScenarioTranslated(const std::string& scenarioId) {
		return loadedScenarios.contains(scenarioId);
	}

	void addLoadedScenario(const std::string& scenarioId) {
		loadedScenarios.insert(scenarioId);
	}

	void addToMissingList(const std::string& scenarioId) {
		std::filesystem::path missingListPath = g_localify_base / "missing_scenarios.json";
		nlohmann::json missingList;

		if (std::filesystem::exists(missingListPath)) {
			try {
				std::ifstream i(missingListPath);
				i >> missingList;
			}
			catch (...) {
				missingList = nlohmann::json::array();
			}
		}
		else {
			missingList = nlohmann::json::array();
		}

		bool found = false;
		if (missingList.is_array()) {
			for (const auto& item : missingList) {
				if (item == scenarioId) {
					found = true;
					break;
				}
			}
		}

		if (!found) {
			missingList.push_back(scenarioId);
			std::ofstream o(missingListPath);
			o << missingList.dump(4);
		}
	}

	bool appendDumpEntry(const std::string& scenarioId, const std::string& uuid, const std::string& original) {
		// Try to derive file path from UUID first (e.g. s44_01010105_00_169646 -> s44_01010105)
		std::string targetFileName = scenarioId;
		std::string sXX, XXXX;
		bool standardFormat = false;

		// UUID format check: sXX_XXXXXXXX_...
		// Minimum length check: s44_01010105 (12 chars)
		if (uuid.length() >= 12 && uuid[0] == 's' && isdigit(uuid[1]) && isdigit(uuid[2]) && uuid[3] == '_') {
			// Extract sXX_XXXXXXXX
			// s44_01010105 -> sXX=s44, XXXX=0101
			sXX = uuid.substr(0, 3);
			XXXX = uuid.substr(4, 4);
			targetFileName = uuid.substr(0, 12); // s44_01010105
			standardFormat = true;
		}
		// Fallback to scenarioId if UUID is non-standard
		else if (scenarioId.length() >= 8 && scenarioId[0] == 's' && isdigit(scenarioId[1]) && isdigit(scenarioId[2])) {
			sXX = scenarioId.substr(0, 3);
			XXXX = scenarioId.substr(4, 4);
			targetFileName = scenarioId;
			standardFormat = true;
		}
		else {
			// Fallback for non-standard IDs (e.g. Scenario_Op_01_01)
			sXX = "misc";
			XXXX = "others";
			targetFileName = scenarioId;
		}

		std::filesystem::path dumpDir = g_localify_base / "timeline_json" / sXX / XXXX;
		if (!std::filesystem::exists(dumpDir)) {
			std::filesystem::create_directories(dumpDir);
		}
		std::filesystem::path dumpFile = dumpDir / (targetFileName + ".json");

		nlohmann::ordered_json jArray;
		if (std::filesystem::exists(dumpFile)) {
			try {
				std::ifstream i(dumpFile);
				i >> jArray;
			}
			catch (...) {
				jArray = nlohmann::ordered_json::array();
			}
		}
		else {
			jArray = nlohmann::ordered_json::array();
		}

		// Check for duplicate UUID
		bool exists = false;
		for (const auto& item : jArray) {
			if (item.contains("uuid") && item["uuid"] == uuid) {
				exists = true;
				break;
			}
		}

		if (!exists) {
			nlohmann::ordered_json entry;
			entry["uuid"] = uuid;
			entry["name"] = "Unknown";
			entry["original"] = original;
			entry["translation"] = "";
			entry["config"] = {
				{"zhSize", 38},
				{"jpSize", 24},
				{"lineSpacing", -10},
				{"dualMode", true}
			};
			jArray.push_back(entry);

			std::ofstream o(dumpFile);
			o << jArray.dump(4);
			
			// Mark this scenario as loaded/processed so we don't dump it again in this session
			// unless F5 is pressed (which reloads loadedScenarios)
			// Use targetFileName because that's the actual ID we used for the file
			addLoadedScenario(targetFileName);
			
			return true;
		}
		return false;
	}
}
