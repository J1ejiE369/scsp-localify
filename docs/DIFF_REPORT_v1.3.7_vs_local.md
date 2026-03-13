# 本地版本 vs 远端 v1.3.7 差异报告

**基准版本**：chinosk6/scsp-localify @ `b5d3611d5a0c9515393f48f616ae6b122ea167a0` (v1.3.7)  
**对比时间**：基于本地当前代码快照

---

## 一、差异总览

| 模块 | 远端 v1.3.7 | 本地 | 差异类型 |
|------|-------------|------|----------|
| main.cpp | 约 330 行 | 约 455 行 | 配置逻辑大幅扩展 |
| stdinclude.hpp | 无 SubtitleConfig、reloadKey | 新增 SubtitleConfig、reloadKey、g_subtitle_config | 新增类型与全局变量 |
| local/local.hpp | 仅基础接口 | 新增 SubtitleData、getSubtitle、formatSubtitle、dumpSubtitle 等 | 剧情字幕模块 |
| local/local.cpp | 基础实现 | 剧情翻译、Dump、appendDumpEntry 等 | 剧情字幕实现 |
| mhotkey | 无 RegisterHotkey | 新增 RegisterHotkey、g_hotkey_callbacks | 热键回调扩展 |
| hook.cpp | 无 ApplyTranslation、DramaSubtitleContext | 新增 ApplyTranslation、ExtractSubtitleContext、DramaSubtitleContext | 剧情翻译 Hook |

---

## 二、逐文件差异详情

### 2.1 `src/main.cpp`

#### 2.1.1 新增全局变量
| 变量 | 类型 | 说明 |
|------|------|------|
| `SubtitleConfig g_subtitle_config` | 结构体 | 字幕配置（zhSize、jpSize、dualMode 等） |
| `int reloadKey` | int | 热重载键码，0 表示禁用 |

**远端**：无上述变量；无 `reloadKey` 相关逻辑。

#### 2.1.2 配置读取逻辑（read_config）

**远端**：
- 仅读取配置，不写回
- 配置文件不存在时直接 `return dicts`
- 无 `autoDumpSubtitle`、`dualMode`、`reloadKey` 解析
- 无 `configModified`、无自动补全缺失字段

**本地**：
- 配置文件不存在时：`document.SetObject()`，视为空对象继续处理
- 使用 `CONFIG_BOOL`、`CONFIG_INT`、`CONFIG_FLOAT`、`CONFIG_STRING` 宏统一「读 + 缺则补」
- 新增 `reloadKey` 解析（支持 int、单字符、F1–F12 字符串）
- 新增 `autoDumpSubtitle`、`dualMode` 解析
- 新增 `baseFreeCamera`、`startResolution` 等对象的缺省补全
- `configModified == true` 时写回 `scsp-config.json`

#### 2.1.3 DllMain 初始化流程

**远端**：
- 无 `InstallCrashHandler()` 调用
- 使用 `g_on_hook_ready` + `cond.wait` 等待 Hook 就绪
- 注释为「依赖检查游戏版本的指针加载，因此在 hook 完成后再加载翻译数据」

**本地**：
- 新增 `InstallCrashHandler()` 调用（在 `init_hook()` 之前）
- 同样使用 `g_on_hook_ready` + `cond.wait` 等待
- 注释为「Translation loading depends on game-version-specific pointers; load after hook is ready」

---

### 2.2 `src/stdinclude.hpp`

#### 2.2.1 新增内容
| 内容 | 说明 |
|------|------|
| `struct SubtitleConfig` | 字幕配置结构体（zhSize、jpSize、lineSpacing、dualMode 等） |
| `extern SubtitleConfig g_subtitle_config` | 全局字幕配置 |
| `extern int reloadKey` | 热重载键码 |
| `void InstallCrashHandler()` | 崩溃处理声明 |

**远端**：无 `SubtitleConfig`、`g_subtitle_config`、`reloadKey`、`InstallCrashHandler`。  
**远端**：无 `g_isDumping`、`g_dumpingScenarioId`、`g_dumpedUUIDs`、`g_debugMode`（这些在本地 `stdinclude.hpp` 中存在）。

---

### 2.3 `src/local/local.hpp`

#### 2.3.1 远端内容（精简）
```cpp
namespace SCLocal {
    void loadLocalTrans();
    bool getLocalifyText(...);
    std::filesystem::path getFilePathByName(...);
    bool getLocalFileName(...);
    std::string getLyricsTrans(...);
    bool getGameUnlocalTrans(...);
}
```

#### 2.3.2 本地新增
| 新增项 | 说明 |
|--------|------|
| `struct SubtitleData` | 剧情字幕数据结构 |
| `bool getSubtitle(const std::string& key, SubtitleData& outData)` | 按 UUID 获取字幕 |
| `void SetDumpingScenarioId` / `GetDumpingScenarioId` | Dump 用 scenarioId |
| `std::string formatSubtitle(const SubtitleData& data)` | 格式化字幕文本 |
| `bool dumpSubtitle(...)` | 导出未翻译条目 |

**说明**：远端 v1.3.7 的 `local.hpp` 不包含剧情字幕相关接口，本地为完整剧情字幕模块。

---

### 2.4 `src/local/local.cpp`

**远端**：获取失败（500），但由 `local.hpp` 可推断远端无剧情字幕实现。

**本地**：
- `parseSubtitleData`、`loadSingleTimelineFile`：加载剧情翻译 JSON
- `appendDumpEntry`：按 UUID 动态解析路径（`pos1`、`pos2` 查找 `_`），写入 `translate_data` 目录
- `dumpSubtitle`、`SetDumpingScenarioId`、`GetDumpingScenarioId`
- `formatSubtitle`：根据 `g_subtitle_config` 生成带 Rich Text 的字幕
- `getSubtitle`：从 `unLocalTrans` 查找翻译

**appendDumpEntry 路径逻辑**（本地）：
- 使用 `uuid.find('_')` 动态分割，无 `uuid.substr(0, 3)` 等硬编码
- 支持 `s44`、`s104` 等任意长度前缀

---

### 2.5 `src/mhotkey.hpp` & `src/mhotkey.cpp`

#### 2.5.1 新增接口
```cpp
void RegisterHotkey(int key, std::function<void()> callback);
```

#### 2.5.2 新增实现
- `std::map<int, std::function<void()>> g_hotkey_callbacks`
- 在 `WndProcCallback` 的 `WM_KEYDOWN` 分支中：`g_hotkey_callbacks.find(key)` 并执行回调

**远端**：无 `RegisterHotkey` 及回调映射。

---

### 2.6 `src/hook.cpp`

#### 2.6.1 新增全局变量（本地）
- `g_debugMode`、`g_isDumping`、`g_dumpingScenarioId`、`g_dumpedUUIDs`

#### 2.6.2 新增结构体与函数
```cpp
struct DramaSubtitleContext {
    bool isValid;
    std::string uidStr, originalText, charName, internalName, cueName;
    int charId;
    void* behaviour;
    FieldInfo* textField;
};

DramaSubtitleContext ExtractSubtitleContext(void* _this);
void ApplyTranslation(void* _this);
```

#### 2.6.3 ApplyTranslation 调用链
- `Shared_CreatePlayable_hook` 中调用 `ApplyTranslation(_this)`
- `ApplyTranslation` 调用 `ExtractSubtitleContext`，再执行 Dump 与翻译逻辑

#### 2.6.4 热重载注册（init_hook）
```cpp
if (reloadKey != 0) {
    MHotkey::RegisterHotkey(reloadKey, []() {
        mainThreadTasks.push_back([]() {
            printf("[HotKey] Reload key pressed. Reloading translations...\n");
            SCLocal::loadLocalTrans();
            return true;
        });
    });
}
```

**远端**：无 `ApplyTranslation`、`ExtractSubtitleContext`、`DramaSubtitleContext`，无热重载注册逻辑。

---

## 三、差异汇总表（按功能）

| 功能 | 远端 v1.3.7 | 本地 |
|------|-------------|------|
| 剧情字幕翻译 | 无 | 有（ApplyTranslation、getSubtitle、formatSubtitle） |
| 剧情字幕 Dump | 无 | 有（dumpSubtitle、appendDumpEntry） |
| 热重载键 | 无 | 有（reloadKey + RegisterHotkey） |
| 配置自动补全 | 无 | 有（CONFIG_* 宏 + 写回） |
| 配置文件缺失时 | 直接返回 | 创建空对象并补全默认值 |
| 崩溃处理 | 无 | InstallCrashHandler |
| Hook 就绪等待 | 无显式等待 | g_on_hook_ready + cond.wait |
| 字幕配置 | 无 | SubtitleConfig、g_subtitle_config、dualMode |
| Dump 路径解析 | N/A | 动态按 `_` 分割，无硬编码索引 |

---

## 四、结论

1. **本地相对远端 v1.3.7 的增量**：
   - 剧情字幕翻译与 Dump 模块（从无到有）
   - 热重载（reloadKey + mhotkey 回调）
   - 配置自动补全与写回
   - 崩溃处理与 Hook 就绪等待

2. **可能来源**：
   - 远端 v1.3.7 可能为较早版本，不包含剧情字幕功能
   - 本地可能基于后续分支或 fork，或自行合并了其他改动

3. **后续步骤**：
   - 步骤三：对上述差异做模块化与代码风格审查
   - 步骤四：输出审查结论与优化建议
