# TODO: 剧情汉化 JSON 结构扩展

当前项目已完成**字幕（Subtitle）**汉化，还需扩展以支持**标题（Title）**和**选项（Choice）**的完整剧情汉化。

---

## 待扩展的数据模块

| 模块 | 对应内存组件 | 需汉化字段 | 定位键 (Key) |
|------|--------------|------------|--------------|
| 标题 | AdvTitleViewModel | MainTitle, SubTitle | ScenarioId (如 `s44_01020199_00`) |
| 选项 | DramaChoiceData | text | label 或日文原文 |
| 字幕 | DramaSubtitlePlayableBehaviour | text, displayTalkerName | uuid (已实现) |
| 角色名 | (可选) 全局字典 | 角色显示名 | characterId / internalName |

---

## JSON 结构方案对比

### 方案 A：按场景聚合（树状）

一个 JSON 文件对应一个剧情，包含该剧情的标题、选项、对话。

```json
{
  "scenario_id": "s44_01020199_00",
  "title": { "main_jp": "第1話", "main_cn": "第1话", "sub_jp": "出会い", "sub_cn": "相遇" },
  "choices": { "choice_1_a": { "jp": "おはよう！", "cn": "早上好！" } },
  "subtitles": [ { "uuid": "...", "jp": "...", "cn": "..." } ]
}
```

- 优点：结构清晰，按章节分配翻译
- 缺点：C++ 需定义更复杂的 `ScenarioData` 结构体

### 方案 B：按数据类型分离（扁平）

不同类型数据分文件或分节点管理。

- `titles.json`：`{ "s44_01020199_00": { "main": "第1话", "sub": "相遇" } }`
- `choices.json`：`{ "おはよう！": "早上好！" }`（按原文匹配，可复用）
- `subtitles/s44_xxx.json`：保持现有结构

- 优点：C++ 解析简单，`unordered_map` 即可
- 缺点：翻译时缺乏上下文

---

## C++ 端 (local.hpp) 待增接口

```cpp
struct TitleData {
    std::string mainTitle;
    std::string subTitle;
};

bool getTitle(const std::string& scenarioId, TitleData& outData);
bool getChoice(const std::string& originalText, std::string& outTranslatedText);
// 或按 scenarioId + label: bool getChoice(const std::string& scenarioId, const std::string& label, std::string& outText);
```

---

## Hook 注入点（待实现）

| 数据类型 | 注入点 | 说明 |
|----------|--------|------|
| 标题 | AdvTitleView.InitializeAsync / _initializeTitle | 在设置 MainTitle/SubTitle 前替换 |
| 选项 | DramaChoiceMixerBehaviour.Apply 或 DramaChoiceButtonGroupView.Show | 在显示选项文本前替换 |

---

## 决策点

- [ ] 选择方案 A 或 B（或混合）
- [ ] 角色名是否抽离为全局字典
- [ ] 选项 Key 用 label 还是日文原文
