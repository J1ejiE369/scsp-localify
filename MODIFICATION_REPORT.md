# Modification Report / 修改报告

## 1. 概述 (Overview)
本 PR 旨在解决游戏新版本 Timeline 系统下的汉化注入稳定性问题，并新增了强大的文本导出（Dump）功能，支持运行时动态捕获和静态数据转换。同时修复了多个导致游戏崩溃的严重 Bug，并增强了开发体验（热重载）。

## 2. 核心改动 (Key Changes)

### 2.1 Timeline 汉化注入重构
- **Hook 点变更**: 最终锁定 `DepthOfFieldClip_CreatePlayable_hook` (DramaSubtitlePlayableAsset) 作为最佳注入点。
- **签名修正**: 修复了 Hook 函数签名的严重错误（补充了隐式的 `retstr` 返回值指针），彻底解决了此前导致的 `0xc0000005` 内存访问违规崩溃。
- **稳定性增强**: 在 `il2cpp` 反射层增加了对 `IEnumerable`、`MethodInfo` 和 `enumerator` 的全面空指针检查，防止因 Unity 对象生命周期问题导致的闪退。

### 2.2 智能导出模块 (Smart Dump Module)
- **V2 数据格式**: 实现了全新的 `timeline_json` 导出格式，基于 `UUID` 索引，支持附加配置（字号、行间距）。
- **动态导出**: 游戏运行时自动检测未翻译的文本，并将其导出到 `timeline_json` 目录。
- **静态导出 (Static Dump)**: 新增 `dumpStaticEntries` 配置项。启用后，可将原始的 Scenario JSON 数据直接解析并转换为标准的 V2 格式，自动生成合成 UUID (`sXX_XXXX_static_XXXX`)，无需人工干预。
- **路径路由优化**: 解决了 `Scenario_Home` 等非标准 ID 导致的文件路径混乱问题，优先解析 UUID 中的章节信息进行归档。
- **防重机制**: 引入 `loadedScenarios` 内存缓存和文件存在性检查，避免重复导出相同的文本段。

### 2.3 工程化改进
- **热重载修复**: 将 F5 热重载机制从不稳定的 `WndProc` Hook 改为在 `Update` 循环中轮询 (`GetAsyncKeyState`)，确保在任何场景下都能稳定触发。
- **文件组织**: 废弃了旧的 `Output_JSON` 目录，统一使用 `timeline_json`，并按 `sXX/XXXX/` 结构清晰分层。

## 3. 文件变动清单 (File Changes)

- `src/hook.cpp`: 
    - 修正 `CreatePlayable` Hook 逻辑与签名。
    - 集成 F5 热重载轮询。
    - 实现 `fmtAndDumpJsonBytesData` 对静态导出的支持。
- `src/local/local.cpp` / `local.hpp`: 
    - 新增 `processStaticDump`, `hasJapanese`, `extractStrings` 等静态处理函数。
    - 重构 `appendDumpEntry` 以支持 V2 格式和 UUID 路由。
    - 实现 `loadTimelineTrans` 加载新格式数据。
- `src/il2cpp/il2cpp_symbols.cpp`:
    - 增强 `iterate_IEnumerable` 的健壮性，添加多重空指针防御。
- `resources/scsp-config.json`:
    - 新增 `"dumpStaticEntries": false` 配置项。

## 4. 测试情况 (Testing)
- **剧情播放**: 已测试主线、活动剧情，汉化文本注入正常，无闪退。
- **导出功能**: 
    - 动态导出：首次观看未汉化剧情时，能正确生成 JSON 文件。
    - 静态导出：开启配置后，能正确将旧数据转换为新格式。
- **热重载**: 游戏运行中按 F5，能成功重新加载修改后的翻译文件。

