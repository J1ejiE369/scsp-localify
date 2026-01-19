#include <stdinclude.hpp>


namespace SCLocal {
	namespace {
		std::unordered_map<std::string, std::unordered_map<int, std::string>> localTrans{};
		std::unordered_map<std::string, std::string> lrcTrans{};
		std::unordered_map<std::string, std::string> unLocalTrans{};
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
		loadGenericTrans("local2.json", unLocalTrans);
	}

	void loadTimelineTrans() {
		std::vector<std::filesystem::path> searchPaths = {
			g_localify_base / "Output_JSON",
			g_localify_base / "../scsp_data/Output_JSON",
			"scsp_data/Output_JSON",
			"Output_JSON"
		};

		std::filesystem::path timelinePath;
		bool found = false;

		for (const auto& path : searchPaths) {
			if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
				timelinePath = path;
				found = true;
				break;
			}
		}

		if (!found) {
			printf("Timeline translation directory not found. Searched in:\n");
			for (const auto& path : searchPaths) {
				// printf(" - %ls\n", std::filesystem::absolute(path).c_str());
			}
			return;
		}

		printf("Loading timeline translations from %ls...\n", timelinePath.c_str());
		int fileCount = 0;
		int itemCount = 0;

		try {
			for (const auto& entry : std::filesystem::recursive_directory_iterator(timelinePath)) {
				if (entry.is_regular_file() && entry.path().extension() == ".json") {
					try {
						std::ifstream file(entry.path());
						std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
						file.close();

						auto jsonArray = nlohmann::json::parse(content);
						if (jsonArray.is_array()) {
							for (const auto& item : jsonArray) {
								if (item.contains("uuid") && item.contains("cn_text")) {
									std::string uuid = item["uuid"];
									std::string cnText = item["cn_text"];
									if (!uuid.empty() && !cnText.empty()) {
										unLocalTrans[uuid] = cnText;
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
					if (auto it = unLocalTrans.find(localText); it != unLocalTrans.end()) {
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
	ע�⼸������� category: mlStory_MainStoryEpisode, mlMusic_CueSheet, mlMusic_MVScene
	�����˽���Ϸ�ļ��ṹ������Ҫ�޸��⼸����ֵ (���ų�����������)
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
		if (auto iter = unLocalTrans.find(origStr); iter != unLocalTrans.end()) {
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

	bool getSubtitle(const std::string& key, std::string& outText) {
		if (auto iter = unLocalTrans.find(key); iter != unLocalTrans.end()) {
			outText = iter->second;
			return true;
		}
		return false;
	}
}
