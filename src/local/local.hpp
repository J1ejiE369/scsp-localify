#pragma once
#include <string>
#include <unordered_map>
#include <optional>
#include <vector>
#include <filesystem>



namespace SCLocal {
	// JSON data structure definitions
	struct SubtitleData
	{
		std::string original;
		std::string translation;
		std::string name;
		std::string internalName;
		int characterId = 0;
		std::string cueName;
	};

	void loadLocalTrans();
	bool getLocalifyText(const std::string& category, int id, std::string* getStr);
	bool getLocalifyText(const std::wstring& category, int id, std::wstring* getStr);
	std::filesystem::path getFilePathByName(const std::wstring& gamePath, bool createFatherPath, const std::filesystem::path& fatherBase);
	bool getLocalFileName(const std::wstring& gamePath, std::filesystem::path* localPath, bool checkExists = true);
	std::string getLyricsTrans(const std::wstring& orig);
	bool getGameUnlocalTrans(const std::wstring& orig, std::string* newStr);


		bool getSubtitle(const std::string& key, SubtitleData& outData);

		void SetDumpingScenarioId(const std::string& scenarioId);
		std::string GetDumpingScenarioId();

	/**
	 * @brief Format subtitle text
	 *
	 * Concatenates original and translated text with Unity Rich Text tags
	 * according to config (font size, line height, dual mode, etc.).
	 */
	std::string formatSubtitle(const SubtitleData& data);

	/**
	 * @brief Dump subtitle data (export untranslated entries)
	 */
	bool dumpSubtitle(
		const std::string& scenarioId,
		const std::string& uuid,
		const std::string& originalText,
		const std::string& speakerName,
		const std::string& internalName,
		int characterId,
		const std::string& cueName
	);

	bool isScenarioTranslated(const std::string& scenarioId);
	void addToMissingList(const std::string& scenarioId);
}
