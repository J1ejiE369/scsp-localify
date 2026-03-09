#pragma once
#include <string>
#include <unordered_map>
#include <optional>
#include <vector>
#include <filesystem>



namespace SCLocal {
	// 定义josn数据结构
	struct SubtitleConfig
	{
		int zhSize = 38;
		int jpSize = 24;
		int lineSpacing = -10;
		int zhLineHeight = 100;
		int jpLineHeight = 100;
		bool dualMode = true;
	};

	struct SubtitleData
	{
		std::string original;
		std::string translation;
		std::string name;
		std::string internalName;
		int characterId = 0;
		std::string cueName;
	};

	// Global configuration for subtitles
	extern SubtitleConfig g_subtitle_config;

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
	 * @brief 格式化字幕文本
	 * 
	 * 根据 SubtitleData 中的配置（字号、行高、双语模式等）将原文和译文拼接成
	 * 带有 Unity Rich Text 标签的最终字符串。
	 */
	std::string formatSubtitle(const SubtitleData& data);

	/**
	 * @brief 尝试导出字幕数据 (Dump)
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
