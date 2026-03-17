#include <stdinclude.hpp>

namespace SCLocal {
	// =============================================================================
	// 1. Dump JSON data structure definition
	// =============================================================================
	// Timeline dump files (translate_data/{prefix}/{subFolder}/*.json) use this format:
	//
	// [
	//   { "_subtitleConfig": true, "zhSize": 38, "jpSize": 24, "lineSpacing": -10,
	//     "zhLineHeight": 100, "jpLineHeight": 100, "dualMode": true },  // optional, first element
	//   {
	//     "uuid": "s44_01010100_00_124845",
	//     "original": "日文原文",
	//     "translation": "中文翻译",
	//     "speaker": { "name": "角色名", "internalName": "内部名", "id": 1 },
	//     "voice": { "cueName": "语音cue名" }
	//   },
	//   ...
	// ]
	//
	// SubtitleData (in-memory) maps: uuid -> { original, translation } for display.
	// =============================================================================

	namespace {
		std::unordered_map<std::string, std::unordered_map<int, std::string>> localTrans{};
		std::unordered_map<std::string, std::string> lrcTrans{};
		std::unordered_map<std::string, std::string> unLocalTrans_Legacy{};
		std::unordered_map<std::string, SubtitleData> unLocalTrans{};
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

	// =============================================================================
	// 2. Load translated data and write to bubbles
	// =============================================================================
	// Flow: loadTimelineTrans -> loadSingleTimelineFile -> parseSubtitleData -> unLocalTrans
	//       Hook requests text by uuid -> getSubtitle -> formatSubtitle -> inject into game bubble
	// =============================================================================

	// Helper: Parse SubtitleData from JSON
	SubtitleData parseSubtitleData(const nlohmann::json& item) {
		SubtitleData data;

		// Translation: try 'translation' then 'cn_text'
		if (item.contains("translation")) data.translation = item["translation"];
		else if (item.contains("cn_text")) data.translation = item["cn_text"];

		// Original: try 'original' then 'jp_text'
		if (item.contains("original")) data.original = item["original"];
		else if (item.contains("jp_text")) data.original = item["jp_text"];

		// [Fix] If dualMode is disabled globally, clear the original text in memory
		// This ensures formatSubtitle won't even see the original text
		if (!g_subtitle_config.dualMode) {
			data.original.clear();
		}

		return data;
	}

	// Helper: Load single file
	int loadSingleTimelineFile(const std::filesystem::path& path) {
		int count = 0;
		try {
			std::ifstream file(path);
			nlohmann::json jsonArray;
			file >> jsonArray;

			if (jsonArray.is_array()) {
				for (const auto& item : jsonArray) {
					if (item.contains("_subtitleConfig")) continue;  // Skip config block (dual-mode WIP)
					if (item.contains("uuid")) {
						std::string uuid = item["uuid"];
						if (!uuid.empty()) {
							SubtitleData data = parseSubtitleData(item);
							if (!data.translation.empty()) {
								unLocalTrans[uuid] = data;
								count++;
							}
						}
					}
				}
			}
		}
		catch (std::exception& e) {
			printf("Error loading timeline file %ls: %s\n", path.c_str(), e.what());
		}
		return count;
	}

	void loadTimelineTrans() {
		std::filesystem::path timelinePath = g_localify_base / "translate_data";

		if (!std::filesystem::exists(timelinePath) || !std::filesystem::is_directory(timelinePath)) {
			printf("Timeline translation directory not found: %ls\n", timelinePath.c_str());
			return;
		}

		printf("Loading timeline translations (V2) from %ls...\n", timelinePath.c_str());
		int fileCount = 0;
		int itemCount = 0;

		try {
			for (const auto& entry : std::filesystem::recursive_directory_iterator(timelinePath)) {
				if (entry.is_regular_file()) {
					auto ext = entry.path().extension().string();
					std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
					if (ext == ".json") {
						itemCount += loadSingleTimelineFile(entry.path());
						fileCount++;
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

	template <typename T>
	std::vector<std::basic_string<T>> split(const std::basic_string<T>& text, T delimiter) {
		std::vector<std::basic_string<T>> parts;
		typename std::basic_string<T>::size_type start = 0;
		typename std::basic_string<T>::size_type end = text.find(delimiter);
		while (end != std::basic_string<T>::npos) {
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

	// Lookup by uuid; used by hook to get text for subtitle bubble
	bool getSubtitle(const std::string& key, SubtitleData& outData) {
		if (auto iter = unLocalTrans.find(key); iter != unLocalTrans.end()) {
			outData = iter->second;
			return true;
		}
		return false;
	}

	bool isScenarioTranslated(const std::string& scenarioId) {
		// Check if scenario file exists by parsing ID
		auto firstUnderscore = scenarioId.find('_');
		if (firstUnderscore == std::string::npos) return false;

		std::string prefix = scenarioId.substr(0, firstUnderscore);
		std::string idBody = scenarioId.substr(firstUnderscore + 1);

		if (idBody.length() < 4) return false;
		std::string subFolder = idBody.substr(0, 4);

		std::filesystem::path filePath = g_localify_base / "translate_data" / prefix / subFolder / (scenarioId + ".json");
		return std::filesystem::exists(filePath);
	}

	void addToMissingList(const std::string& scenarioId) {
		// Cache missing IDs to avoid frequent file I/O
		static std::set<std::string> missingCache;
		if (missingCache.contains(scenarioId)) return;

		std::filesystem::path listPath = g_localify_base / "missing_scenarios.json";
		nlohmann::json jsonList;

		if (std::filesystem::exists(listPath)) {
			try {
				std::ifstream file(listPath);
				jsonList = nlohmann::json::parse(file);
			} catch (...) {
				jsonList = nlohmann::json::array();
			}
		} else {
			jsonList = nlohmann::json::array();
		}

		// Sync cache with file content
		bool exists = false;
		for (const auto& item : jsonList) {
			if (item.is_string()) {
				std::string s = item.get<std::string>();
				missingCache.insert(s);
				if (s == scenarioId) exists = true;
			}
		}

		if (!exists) {
			jsonList.push_back(scenarioId);
			missingCache.insert(scenarioId);
			
			std::ofstream file(listPath);
			file << jsonList.dump(4);
			file.close();
			printf("[Dump] Added %s to missing list.\n", scenarioId.c_str());
		}
	}

	// Format SubtitleData to Rich Text for game bubble (dual-mode or translation-only)
	std::string formatSubtitle(const SubtitleData& data) {
		if (g_subtitle_config.dualMode && !data.original.empty()) {
			auto zhLines = split(data.translation, '\n');
			auto jpLines = split(data.original, '\n');

			std::string combinedText;
			combinedText.reserve(data.translation.size() + data.original.size() + 100);

			// 1. Format Original Text Block
			for (size_t i = 0; i < jpLines.size(); i++) {
				if (i > 0) combinedText += "\n";

				std::string jpLine = jpLines[i];
				jpLine.erase(std::remove(jpLine.begin(), jpLine.end(), '\r'), jpLine.end());

				combinedText += std::format("<line-height={}%><nobr><size={}><color=#CCCCCC>{}</color></size></nobr></line-height>",
					g_subtitle_config.jpLineHeight, g_subtitle_config.jpSize, jpLine);
			}

			// 2. Format Translation Text Block
			for (size_t i = 0; i < zhLines.size(); i++) {
				if (i > 0 || !jpLines.empty()) combinedText += "\n";

				std::string zhLine = zhLines[i];
				zhLine.erase(std::remove(zhLine.begin(), zhLine.end(), '\r'), zhLine.end());

				// Apply line spacing as negative offset for the first line
				float currentVOffset = (i == 0) ? -((float)g_subtitle_config.lineSpacing / 100.0f) : 0.0f;

				combinedText += std::format("<voffset={}em><line-height={}%><nobr><size={}>{}</size></nobr></line-height></voffset>",
					currentVOffset, g_subtitle_config.zhLineHeight, g_subtitle_config.zhSize, zhLine);
			}

			return combinedText;
		}
		return data.translation;
	}

	// =============================================================================
	// 3. No translation exists -> dump to disk and create new JSON
	// =============================================================================
	// Flow: Hook detects untranslated uuid -> dumpSubtitle (filtered by scenario)
	//       -> appendDumpEntry creates/updates translate_data/*.json on disk
	//       addToMissingList records scenario in missing_scenarios.json
	// =============================================================================

	bool appendDumpEntry(
		const std::string& scenarioId,
		const std::string& uuid,
		const std::string& original,
		const std::string& name,
		const std::string& internalName,
		int characterId,
		const std::string& cueName
	) {
		if (uuid.empty()) return false;

		// Determine dump path based on UUID format (dynamic split by '_', no hardcoded indices)
		std::string dumpFileName = "dump_unknown";
		std::filesystem::path dumpPath = g_localify_base / "translate_data" / "misc";

		size_t pos1 = uuid.find('_');
		if (pos1 != std::string::npos && pos1 > 0) {
			std::string prefix = uuid.substr(0, pos1);
			size_t pos2 = uuid.find('_', pos1 + 1);
			if (pos2 != std::string::npos) {
				std::string subFolder = uuid.substr(pos1 + 1, pos2 - pos1 - 1);
				if (subFolder.length() >= 4) {
					dumpPath = g_localify_base / "translate_data" / prefix / subFolder.substr(0, 4);
					dumpFileName = uuid.substr(0, pos2);
				}
			} else {
				std::string afterFirst = uuid.substr(pos1 + 1);
				if (afterFirst.length() >= 4) {
					dumpPath = g_localify_base / "translate_data" / prefix / afterFirst.substr(0, 4);
					dumpFileName = uuid;
				}
			}
		}

		if (dumpFileName == "dump_unknown" && !scenarioId.empty()) {
			dumpFileName = scenarioId;
		}

		std::filesystem::create_directories(dumpPath);
		std::filesystem::path filePath = dumpPath / (dumpFileName + ".json");

		if (g_debugMode) {
			printf("[Dump] Target Path: %ls\n", filePath.c_str());
		}

		nlohmann::ordered_json jsonArray;
		if (std::filesystem::exists(filePath)) {
			try {
				std::ifstream file(filePath);
				jsonArray = nlohmann::ordered_json::parse(file);
			} catch (std::exception& e) {
				printf("[Dump] Failed to parse %ls: %s\n", filePath.c_str(), e.what());
				return false;
			}
		} else {
			jsonArray = nlohmann::ordered_json::array();
		}

		bool modified = false;
		bool found = false;

		// Ensure subtitle config block exists for dual-language subtitle feature (WIP)
		{
			auto ensureConfigBlock = [&]() -> bool {
				if (!jsonArray.empty() && jsonArray[0].contains("_subtitleConfig")) return false;
				nlohmann::ordered_json configObj;
				configObj["_subtitleConfig"] = true;
				configObj["zhSize"] = g_subtitle_config.zhSize;
				configObj["jpSize"] = g_subtitle_config.jpSize;
				configObj["lineSpacing"] = g_subtitle_config.lineSpacing;
				configObj["zhLineHeight"] = g_subtitle_config.zhLineHeight;
				configObj["jpLineHeight"] = g_subtitle_config.jpLineHeight;
				configObj["dualMode"] = g_subtitle_config.dualMode;
				jsonArray.insert(jsonArray.begin(), configObj);
				return true;
			};
			if (ensureConfigBlock()) modified = true;
		}

		for (auto& item : jsonArray) {
			if (item.value("uuid", "") == uuid) {
				found = true;
				
				// Helper to update field if changed
				auto updateIfChanged = [&](nlohmann::ordered_json& obj, const char* key, const auto& val) {
					if (!obj.contains(key) || obj[key] != val) {
						obj[key] = val;
						modified = true;
					}
				};

				updateIfChanged(item, "original", original);

				// Ensure structure exists
				if (!item.contains("speaker")) item["speaker"] = nlohmann::ordered_json::object();
				if (!item.contains("voice")) item["voice"] = nlohmann::ordered_json::object();

				updateIfChanged(item["speaker"], "name", name);
				updateIfChanged(item["speaker"], "internalName", internalName);
				updateIfChanged(item["speaker"], "id", characterId);
				updateIfChanged(item["voice"], "cueName", cueName);
				
				// Cleanup legacy fields
				if (item.contains("name")) { item.erase("name"); modified = true; }
				if (item.contains("config")) { item.erase("config"); modified = true; } // Cleanup old config field

				break;
			}
		}

		if (!found) {
			nlohmann::ordered_json newItem;
			newItem["uuid"] = uuid;
			newItem["speaker"] = {
				{"name", name},
				{"internalName", internalName},
				{"id", characterId}
			};
			newItem["voice"] = { {"cueName", cueName} };
			newItem["original"] = original;
			newItem["translation"] = "";
			
			jsonArray.push_back(newItem);
			modified = true;
		}

		if (modified) {
			try {
				std::ofstream outFile(filePath, std::ios::binary);
				if (!outFile.is_open()) {
					printf("[Dump] ERROR: Could not open file for writing: %ls\n", filePath.c_str());
					return false;
				}
				outFile << jsonArray.dump(2);
				outFile.close();
				if (g_debugMode) printf("[Dump] Successfully wrote to: %ls\n", filePath.c_str());
				return true;
			} catch (std::exception& e) {
				printf("[Dump] Failed to write %s: %s\n", filePath.string().c_str(), e.what());
			}
		}

		return modified;
	}

	std::mutex g_dumpMutex;
	std::mutex g_scenarioIdMutex;

	void SetDumpingScenarioId(const std::string& scenarioId) {
		std::lock_guard<std::mutex> lock(g_scenarioIdMutex);
		g_dumpingScenarioId = scenarioId;
	}

	std::string GetDumpingScenarioId() {
		std::lock_guard<std::mutex> lock(g_scenarioIdMutex);
		return g_dumpingScenarioId;
	}

	// Entry point: called by hook when subtitle has no translation; dumps to JSON
	bool dumpSubtitle(
		const std::string& scenarioId,
		const std::string& uuid,
		const std::string& originalText,
		const std::string& speakerName,
		const std::string& internalName,
		int characterId,
		const std::string& cueName
	) {
		if (!g_isDumping) return false;

		std::lock_guard<std::mutex> lock(g_dumpMutex);

		std::string currentGlobalId = GetDumpingScenarioId();
		
		// Only dump if the current scenario matches the global target
		if (!currentGlobalId.empty() && scenarioId == currentGlobalId) {
			if (!g_dumpedUUIDs.contains(uuid)) {
				g_dumpedUUIDs.insert(uuid);

				bool dumped = appendDumpEntry(
					currentGlobalId,
					uuid,
					originalText,
					speakerName,
					internalName,
					characterId,
					cueName
				);

				if (dumped && g_debugMode) {
					printf("[Dump] Synced/Dumped %s (%s)\n", uuid.c_str(), speakerName.c_str());
				}
				return dumped;
			}
		}
		return false;
	}
}
