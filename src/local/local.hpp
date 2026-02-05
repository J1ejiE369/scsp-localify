#pragma once
#include <string>
#include <unordered_map>
#include <optional>
#include <vector>
#include <filesystem>



namespace SCLocal {
	// 定义 V2 数据结构
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
		std::string original;    // 日文原文
		std::string translation; // 中文译文
		// Speaker Info (V3)
		std::string name;        // displayTalkerName (GUI显示名)
		std::string internalName;// talkerName (内部标识名)
		int characterId = 0;     // mstCharacterInfoId (角色ID)
		// Voice Info (V3)
		std::string cueName;     // 语音文件名

		SubtitleConfig config;   // 样式配置
	};

	void loadLocalTrans();
	bool getLocalifyText(const std::string& category, int id, std::string* getStr);
	bool getLocalifyText(const std::wstring& category, int id, std::wstring* getStr);
	std::filesystem::path getFilePathByName(const std::wstring& gamePath, bool createFatherPath, const std::filesystem::path& fatherBase);
	bool getLocalFileName(const std::wstring& gamePath, std::filesystem::path* localPath, bool checkExists = true);
	std::string getLyricsTrans(const std::wstring& orig);
	bool getGameUnlocalTrans(const std::wstring& orig, std::string* newStr);

		// 新的接口：返回完整的 SubtitleData 对象
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
	bool tryDumpSubtitle(
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
