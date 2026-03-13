# SCSP Drama 与 Timeline 初始化流程分析

本文档详细梳理 **Drama（剧情）** 与 **Timeline（时间轴）** 相关部分的初始化过程，以及本地化 Hook 的介入时机。

---

## 一、核心类与职责概览

| 类/接口 | 所在 DLL | 职责 |
|--------|----------|------|
| `ScenarioManager` | PRISM.Legacy | 剧情场景管理器，持有 PlayableDirector、加载表数据、驱动 Timeline |
| `IDramaSceneLoader` | PRISM.Legacy | 剧情场景加载器接口，SetupAsync + PlayAsync |
| `Model3dLoaderCore` | PRISM.Legacy | 3D 场景加载核心，持有 ScenarioManager、SceneLoader |
| `TimelineController` | PRISM.Legacy | Timeline 控制器，持有 PlayableDirector，提供 GetTimelineAsset、RebuildPlayableGraph |
| `PlayableDirector` | UnityEngine | Unity 原生，持有 TimelineAsset，构建 PlayableGraph |
| `DramaSubtitlePlayableAsset` | PRISM.Interactions.Drama / PRISM.Legacy | 字幕 Clip 的 PlayableAsset，继承 PlayableAssetBase |
| `DramaSubtitlePlayableBehaviour` | PRISM.Interactions.Drama / PRISM.Legacy | 字幕 Clip 的运行时行为，含 text、uniqueId 等 |

---

## 二、初始化流程（调用顺序）

```
用户选择剧情
    │
    ▼
DramaLauncherPresenter / 剧情入口
    │
    ▼
ILegacyLocator.CreateDramaSceneLoader()
    │
    ▼
IDramaSceneLoader.SetupAsync(4 参数)
    │   ├─ 加载 Drama 场景 prefab
    │   ├─ 实例化 Model3dLoaderCore 或类似加载器
    │   └─ 在场景中查找/实例化 ScenarioManager
    │
    ▼
ScenarioManager._initializeAsync(scrName)  ◄── 【Hook 1】设置 scenarioId
    │   scrName = "s44_01020199_00" 等场景 ID
    │   ├─ LoadTableData(scrName) 加载剧情表数据
    │   └─ 初始化内部状态
    │
    ▼
ScenarioManager.SetupAsync(5 参数)
    │   完整设置：角色、摄像机、Timeline 等
    │
    ▼
IDramaSceneLoader.PlayAsync(2 参数)
    │
    ▼
PlayableDirector.Play() / RebuildPlayableGraph()
    │   Unity 遍历 TimelineAsset 的轨道与 Clip
    │
    ▼
对每个 Clip：PlayableAsset.CreatePlayable(graph, go)
    │   字幕轨道上的 Clip 为 DramaSubtitlePlayableAsset
    │
    ▼
DramaSubtitlePlayableAsset.CreatePlayable()  ◄── 【Hook 2】ApplyTranslation 修改 text
    │   从 behaviour/m_Template 克隆创建 PlayableBehaviour
    │   本地化在此处修改 behaviour.text 等字段
    │
    ▼
PlayableGraph 构建完成，Timeline 开始播放
```

---

## 三、关键阶段详解

### 3.1 ScenarioManager._initializeAsync（第一阶段）

- **方法签名**：`_initializeAsync(Il2CppString* scrName)`，1 个参数
- **调用时机**：场景加载后、SetupAsync 之前
- **作用**：
  - 接收场景 ID（如 `s44_01020199_00`）
  - 调用 `LoadTableData` 加载剧情表数据
  - 初始化 ScenarioManager 内部状态

**本地化 Hook**（`ScenarioManager_Init_hook`）：

```cpp
// hook.cpp 约 1748 行
void* ScenarioManager_Init_hook(void* retstr, void* _this, Il2CppString* scrName) {
    if (scrName) {
        std::string scenarioId = scrName->ToUtf8String();
        SCLocal::SetDumpingScenarioId(scenarioId);  // 供后续 dump/翻译使用
    }
    return HOOK_CAST_CALL(void*, ScenarioManager_Init)(retstr, _this, scrName);
}
```

- `SetDumpingScenarioId` 将当前 scenarioId 存入全局变量，供：
  - `dumpSubtitle`：仅当 scenarioId 匹配时写入 dump
  - 翻译查找：部分逻辑可能依赖 scenarioId 定位翻译文件

---

### 3.2 Model3dLoaderCore 与场景结构

- **Model3dLoaderCore** 持有：
  - `ScenarioManager`（offset 16）
  - `SceneLoader`（offset 24）
- **方法**：
  - `GetScenarioManager()`：获取当前 ScenarioManager
  - `FindComponentInHandledScene()`：在已加载场景中查找组件
  - `_instantiateScenarioManagerIn()`：在场景中实例化 ScenarioManager

Drama 场景根节点（以 `s44_01020199_00` 为例）结构大致为：

```
s44_01020199_00 (根)
├── Handler (OperationHandler, TimeHandler, ChoiceHandler, ...)
├── Cameras (MainCamera, UICamera, ...)
├── CanvasRoot
│   └── UICanvas/SafeArea/DramaSubtitleView  ← 字幕 UI
├── Render3DManager
└── ScenarioManager / PlayableDirector (挂载在场景某处)
```

---

### 3.3 PlayableDirector 与 Timeline 构建

- **ScenarioManager** 持有 `PlayableDirector`（il2cpp dump 中 offset 296）
- **TimelineController** 持有 `PlayableDirector`（`myDirector`），提供：
  - `GetTimelineAsset()`：获取当前 Timeline
  - `GetTrackAsset()`：按名称获取轨道
  - `RebuildPlayableGraph()`：重建 Playable 图

当 `PlayableDirector.Play()` 或 `RebuildPlayableGraph()` 被调用时：

1. Unity 遍历 `TimelineAsset` 的轨道和 Clip
2. 对每个 Clip，调用其 `PlayableAsset.CreatePlayable(PlayableGraph graph, GameObject go)`
3. 字幕轨道上的 Clip 类型为 `DramaSubtitlePlayableAsset`

---

### 3.4 DramaSubtitlePlayableAsset.CreatePlayable（第二阶段）

- **继承关系**：`DramaSubtitlePlayableAsset` → `PlayableAssetBase<DramaSubtitlePlayableBehaviour>` → `PlayableAsset`
- **CreatePlayable**：从 `behaviour` / `m_Template` 克隆创建 `DramaSubtitlePlayableBehaviour` 实例，并生成对应 Playable

**DramaSubtitlePlayableBehaviour 字段**（用于 dump 与翻译）：

| 字段 | 用途 |
|------|------|
| `uniqueId` / `uuid` | 唯一标识，翻译查找 key |
| `text` / `_text` | 对话正文 |
| `displayTalkerName` / `_displayTalkerName` | 显示用角色名 |
| `talkerName` / `_talkerName` | 内部角色名 |
| `mstCharacterInfoId` | 角色 ID |
| `cueName` | 语音 Cue 名 |

**本地化 Hook**（`Shared_CreatePlayable_hook` → `ApplyTranslation`）：

```cpp
// hook.cpp 约 1967 行
void* Shared_CreatePlayable_hook(void* retstr, void* _this, void* graph, void* go, void* mtd) {
    ApplyTranslation(_this);  // 在创建 Playable 前修改 behaviour 的 text 等
    // ... 调用原始 CreatePlayable
}
```

`ApplyTranslation` 流程：

1. 判断 `_this` 是否为 `DramaSubtitlePlayableAsset`
2. 读取 `behaviour` / `m_Template` 中的 `uniqueId`、`text` 等
3. 调用 `SCLocal::getSubtitle(uidStr, subData)` 按 uuid 查找翻译
4. 若有翻译，用 `il2cpp_symbols::write_field(behaviour, text_field, str)` 写入
5. 调用 `SCLocal::dumpSubtitle(...)` 进行 dump（若开启）

---

## 四、时序关系总结

| 阶段 | 方法 | 本地化介入 |
|------|------|------------|
| 1 | `ScenarioManager._initializeAsync(scrName)` | 设置 `g_dumpingScenarioId` |
| 2 | `ScenarioManager.SetupAsync(...)` | 无 |
| 3 | `IDramaSceneLoader.PlayAsync(...)` | 无 |
| 4 | `PlayableDirector` 构建 PlayableGraph | 无 |
| 5 | `DramaSubtitlePlayableAsset.CreatePlayable(...)` | `ApplyTranslation` 修改 text |

**重要**：`_initializeAsync` 必须先于 `CreatePlayable` 执行，否则 `GetDumpingScenarioId()` 可能为空，dump 逻辑会跳过。

---

## 五、Hook 安装位置（hook.cpp）

| Hook 名称 | 目标方法 | 行号 |
|-----------|----------|------|
| `ScenarioManager_Init` | `ScenarioManager._initializeAsync` | 3302–3305, 3680 |
| `Shared_CreatePlayable` | `DramaSubtitlePlayableAsset.CreatePlayable` | 3389–3398, 3626–3662 |

若 `DepthOfFieldClip.CreatePlayable` 与 `DramaSubtitlePlayableAsset.CreatePlayable` 地址相同（共享 `PlayableAssetBase<T>.CreatePlayable` 实现），则只安装一个 Hook，通过 `_this` 类型区分处理。

---

## 六、数据流简图

```
scrName (s44_01020199_00)
    │
    ├─► SCLocal::SetDumpingScenarioId(scenarioId)
    │       │
    │       └─► g_dumpingScenarioId (全局，供 dump 使用)
    │
    └─► ScenarioManager 内部 LoadTableData 等

Timeline Clip (DramaSubtitlePlayableAsset)
    │
    ├─► behaviour.uniqueId  ──► SCLocal::getSubtitle(uuid)  ──► 翻译文本
    │                                                              │
    │                                                              └─► il2cpp write_field(behaviour, text, ...)
    │
    └─► SCLocal::dumpSubtitle(GetDumpingScenarioId(), uuid, ...)  ──► 写入 dump 文件
```

---

## 七、参考文件

- `scsp-localify/src/hook.cpp`：ScenarioManager、DramaSubtitle 相关 Hook
- `scsp-localify/src/local/local.cpp`：`SetDumpingScenarioId`、`getSubtitle`、`dumpSubtitle`
- `scsp-localify/docs/analysis_dump/il2cpp_dump_1771308124.json`：il2cpp 元数据
- `scsp-localify/docs/analysis_dump/SCSP_DRAMA_INSTANCE_DETAIL.json`：Drama 场景层级结构
