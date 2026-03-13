# SCSP 原生剧情实现 - IDA 地址索引

基于 `GameAssembly_dump.dll` (base: 0x180000000) 的 IDA 分析，列出原生剧情（Drama）完整实现的关键函数地址。**不含汉化相关部分**。

---

## 一、入口与加载链

| 函数名 | 地址 | 说明 |
|--------|------|------|
| `PRISM_Adapters_Drama_DramaLauncherPresenter_InitializeAsync` | **0x183ee12e0** | 剧情启动入口，调用 `InitializeAsync_d_6_MoveNext_13` |
| `PRISM_Adapters_Drama_DramaLauncherPresenter_OnBeginIdleAsync` | 0x183ee13d0 | 进入空闲时 |
| `PRISM_Adapters_Drama_DramaLauncherPresenter_TerminateAsync` | 0x183ee14c0 | 终止 |
| `PRISM_Adapters_Drama_DramaLauncherPresenter__ctor` | 0x183ee1650 | 构造函数 |
| `Locator_CreateDramaSceneLoader` | **0x1840cbe60** | 创建 Drama 场景加载器，返回 `DramaSceneLoader` 实例 |

---

## 二、DramaSceneLoader（场景加载与播放）

| 函数名 | 地址 | 说明 |
|--------|------|------|
| `PRISM_Interactions_Drama_DramaSceneLoader_SetupAsync` | **0x184186350** | 4 参数，加载场景，调用 `SetupAsync_d_3_MoveNext_1` |
| `PRISM_Interactions_Drama_DramaSceneLoader_PlayAsync` | **0x184186030** | 2 参数，开始播放，内部用 `UniTask_WaitUntil` 等待 |
| `PRISM_Interactions_Drama_DramaSceneLoader_ReleaseAsync` | 0x184186290 | 释放资源 |
| `_SetupAsync_d_3_MoveNext_1` | **0x1841929d0** | SetupAsync 状态机：`PRISM_SceneLoader_LoadAsync` → `InitializeAsync_d_54_MoveNext_1` |
| `_SetupAsync_d_3_MoveNext` | 0x1843503a0 | 另一阶段状态机 |

---

## 三、ScenarioManager（剧情核心）

| 函数名 | 地址 | 说明 |
|--------|------|------|
| `PRISM_Scenario_ScenarioManager__initializeAsync` | **0x1847aade0** | 1 参数 scrName，thunk 跳转 |
| `_initializeAsync_d_156_MoveNext` | **0x1847c6e90** | 初始化状态机，执行：DataFile_GetBytes、ScenarioScriptParser_Parse、LoadTableData、ReplaceCutTable、语音 CueSheet 加载等 |
| `PRISM_Scenario_ScenarioManager_SetupAsync` | **0x1847a32a0** | 5 参数，完整设置 |
| `PRISM_Scenario_ScenarioManager_LoadTableData` | **0x1847a0a30** | 加载表数据 |
| `PRISM_Scenario_ScenarioManager_ReplaceCutTable` | 0x1847a2490 | 替换 Cut 表 |
| `PRISM_Scenario_ScenarioManager_GetScenarioText` | 0x1847a0330 | 获取剧情文本 |
| `PRISM_Scenario_ScenarioManager_GetTrackAsset` | 0x1847a0530 | 获取 Timeline 轨道 |
| `PRISM_Scenario_ScenarioManager_UpdateInner` | 0x1847a3bb0 | 每帧更新 |
| `PRISM_Scenario_ScenarioManager_PlayVoice` | 0x1847a0eb0 | 播放语音 |
| `PRISM_Scenario_ScenarioManager_ScriptProc` | 0x1847a2860 | 脚本处理 |
| `PRISM_Scenario_ScenarioManager_ForwardToNextWait` | 0x1847a0f7a0 | 快进到下一等待 |
| `PRISM_Scenario_ScenarioManager_JumpScript` | 0x1847a08e0 | 跳转脚本 |
| `PRISM_Scenario_ScenarioManager_ExistChoice` | 0x18479f4c0 | 是否存在选项 |
| `PRISM_Scenario_ScenarioManager_GetChoicesText` | 0x18479fb80 | 获取选项文本 |
| `PRISM_Scenario_ScenarioManager_AttachCharacter` | 0x18479ee50 | 附加角色 |
| `PRISM_Scenario_ScenarioManager_AttachCharacterToTimeline` | 0x18479ed50 | 角色绑定到 Timeline |
| `PRISM_Scenario_ScenarioManager_Terminate` | 0x1847a39a0 | 终止 |
| `PRISM_Scenario_ScenarioManager_ReleaseAssets` | 0x1847a1f80 | 释放资源 |

---

## 四、Model3dLoaderCore（3D 场景与 ScenarioManager 实例化）

| 函数名 | 地址 | 说明 |
|--------|------|------|
| `PRISM_Scenario_Model3dLoaderCore__instantiateScenarioManagerIn` | **0x1847882b0** | 在场景中实例化 ScenarioManager |
| `PRISM_Scenario_Model3dLoaderCore_FinishScenarioManager` | 0x184787d30 | 结束 ScenarioManager |

---

## 五、Timeline 与字幕

| 函数名 | 地址 | 说明 |
|--------|------|------|
| `PRISM_Interactions_Drama_DramaSubtitlePlayableAsset__ctor` | 0x18418d8e0 | 字幕 PlayableAsset 构造（无独立 CreatePlayable，继承 PlayableAssetBase） |
| `PRISM_Interactions_Drama_DramaSubtitleMixerBehaviour_Apply` | **0x18418d310** | 字幕 Mixer 的 Apply：根据当前时间将 behaviour 的 text 应用到 DramaSceneText |
| `PRISM_Interactions_Drama_DramaSubtitleMixerBehaviour_NoHitClip` | 0x18418d7d0 | 无命中 Clip 时 |
| `PRISM_Interactions_Drama_DramaSubtitlePlayableBehaviour_DisplayTalkerName` | 0x18418d920 | 显示说话者名 |
| `PRISM_Interactions_Drama_DramaSubtitlePlayableBehaviour_HasVoice` | 0x18418d950 | 是否有语音 |
| `PRISM_Interactions_Drama_DramaSubtitleTrack_FindClip` | 0x18418d990 | 查找字幕 Clip |
| `PRISM_Interactions_Drama_DramaSubtitleTrack__ctor` | 0x18418db60 | 字幕轨道构造 |
| `PRISM_Interactions_Drama_Timeline_DramaLabelPlayableAsset_CreatePlayable` | 0x1841813e0 | 标签 Playable 创建 |
| `PRISM_TimelineController_GetTrackAsset_1` | (SetupAsync 内调用) | 获取 Timeline 轨道 |

---

## 六、数据加载与解析

| 函数名 | 地址 | 说明 |
|--------|------|------|
| `PRISM_DataFile_GetBytes` | (多处调用) | 按 key 读取字节 |
| `PRISM_DataFile_IsKeyExist` | (多处调用) | 检查 key 是否存在 |
| `PRISM_Scenario_ScenarioScriptParser_Parse` | (_initializeAsync_d_156 内) | 解析剧情脚本 |
| `PRISM_SceneLoader_LoadAsync` | (SetupAsync_d_3 内) | 异步加载场景 |

---

## 七、关键调用流程（IDA 视角）

```
DramaLauncherPresenter_InitializeAsync (0x183ee12e0)
    └─ InitializeAsync_d_6_MoveNext_13

Locator_CreateDramaSceneLoader (0x1840cbe60)
    └─ 返回 DramaSceneLoader

DramaSceneLoader_SetupAsync (0x184186350)
    └─ SetupAsync_d_3_MoveNext_1 (0x1841929d0)
        ├─ PRISM_SceneLoader_LoadAsync          // 加载场景
        ├─ PRISM_SoundManager_StopBGM          // 停止 BGM
        ├─ PRISM_TimelineController_GetTrackAsset_1
        └─ InitializeAsync_d_54_MoveNext_1      // 初始化（含 ScenarioManager）

_initializeAsync_d_156_MoveNext (0x1847c6e90)
    ├─ a1[2] = scrName (场景 ID 字符串)
    ├─ DataFile_GetBytes(scrName + ".json")     // 脚本
    ├─ ScenarioScriptParser_Parse
    ├─ ScenarioManager_ReplaceCutTable
    ├─ ScenarioManager_LoadTableData
    ├─ DataFile_GetBytes(scrName + "_voice.acb") // 语音
    ├─ DataFile_GetBytes(scrName + "_table.json") // 表数据
    └─ ScenarioSound_LoadCueSheetAsync         // 加载 CueSheet

DramaSceneLoader_PlayAsync (0x184186030)
    └─ UniTask_WaitUntil(IsBurstEnabled_00000145_PostfixBurstDelegate)
       // 等待 PlayableDirector 播放完成

DramaSubtitleMixerBehaviour_Apply (0x18418d310)
    ├─ 从 PlayableBehaviour 取 text / displayTalkerName
    ├─ DramaSceneText__setMessageText         // 设置到 UI
    └─ GameObject_SetActive                    // 显示/隐藏字幕
```

---

## 八、DramaSubtitle 数据流（无汉化）

1. **Timeline 构建**：`PlayableDirector` 遍历 `DramaSubtitleTrack` 的 Clip
2. **CreatePlayable**：`DramaSubtitlePlayableAsset` 继承 `PlayableAssetBase`，使用基类 `CreatePlayable`（无独立实现）
3. **运行时**：`DramaSubtitleMixerBehaviour_Apply` 在每帧根据当前时间，从 `DramaSubtitlePlayableBehaviour` 取 `text`、`displayTalkerName` 等，调用 `DramaSceneText__setMessageText` 更新 UI

---

## 九、相关 UI / 视图

| 函数名 | 地址 | 说明 |
|--------|------|------|
| `PRISM_Interactions_Drama_DramaSceneText__setMessageText` | (Apply 内调用) | 设置字幕文本到 UI |
| `PRISM_Interactions_Drama_UI_DramaMenuPresenter_Show` | (Drama 命名空间) | 显示菜单 |
| `PRISM_Interactions_Drama_UI_DramaScreenOperationPresenter_SetActive` | (Drama 命名空间) | 屏幕操作激活 |
| `PRISM_Interactions_Drama_DramaVoiceUtility_*` | (Drama 命名空间) | 语音工具 |

---

## 十、IDA 中快速定位

在 IDA 中可按以下顺序查看：

1. **0x1840cbe60** - `Locator_CreateDramaSceneLoader`：入口
2. **0x184186350** - `DramaSceneLoader_SetupAsync`：场景加载
3. **0x1841929d0** - `SetupAsync_d_3_MoveNext_1`：加载与初始化流程
4. **0x1847c6e90** - `_initializeAsync_d_156_MoveNext`：ScenarioManager 初始化主逻辑
5. **0x1847a0a30** - `ScenarioManager_LoadTableData`：表数据加载
6. **0x184186030** - `DramaSceneLoader_PlayAsync`：播放入口
7. **0x18418d310** - `DramaSubtitleMixerBehaviour_Apply`：字幕应用到 UI
