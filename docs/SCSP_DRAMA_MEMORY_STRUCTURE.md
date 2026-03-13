# SCSP 剧情场景内存结构

本文档描述剧情（Drama）场景在 Unity 引擎中被实例化后的**完整对象结构**（GameObject 与 Component 层级），用于后续分析 AB 包加载与引用机制。

---

## 一、场景根节点与核心组件

```
[Scene Root] s44_01020199_00 (剧情实例根节点)
 │
 ├─ Component: ScenarioManager (剧情总控核心)
 │   └─ [持有数据]: ScriptData (剧情脚本 JSON 数据)
 │
 ├─ Component: PlayableDirector (Timeline 播放器)
 │   └─ [持有资源]: TimelineAsset (依赖 Timeline AB 包)
 │
 └─ Component: Model3dLoaderCore (3D 资源加载器)
```

---

## 二、摄像机组 (Cameras)

```
Cameras
 ├─ MainCamera
 │   ├─ Component: Camera (渲染 3D 场景)
 │   ├─ Component: CinemachineBrain (受 Timeline 镜头轨道控制)
 │   └─ Component: PostProcessLayer (后期处理层)
 │
 ├─ UICamera
 │   └─ Component: Camera (专门渲染 UI Canvas)
 │
 ├─ CaptureOverlayCamera (用于截屏/转场)
 │
 └─ OverlayEffectCamera (全屏特效)
```

---

## 三、UI 根节点 (CanvasRoot)

```
CanvasRoot
 ├─ 3dOverlayCanvas (3D 空间 UI)
 │   └─ DramaFadePanel (3D 淡入淡出遮罩)
 │
 ├─ CaptureOverlayCanvas
 │   └─ RawImage (截屏显示容器)
 │
 └─ UICanvas (核心 2D UI 画布)
     ├─ DramaFadePanel (UI 层淡入淡出)
     │
     └─ DramaUIViewRoot
         └─ DramaUIView(Clone) (实例化的 UI 预制体)
             ├─ Cmn_SafeArea (通用安全区)
             │   ├─ AdvTitleView (左上角标题卡)
             │   │   ├─ Component: AdvTitleView (C# 逻辑)
             │   │   ├─ 1LineTitle / 2LineTitle (TextMeshProUGUI)
             │   │   └─ icon/ProduceIdolIcon_Square 等 (Image 组件)
             │   │       └─ [持有资源]: 角色头像 Sprite (依赖 UI 图集 AB 包)
             │   └─ Margin (1) -> MaxPlaySpeedView (倍速提示)
             │
             ├─ DramaMenuView (暂停/设置菜单)
             │   └─ SubMenuArea (LogButton, SettingsButton, SkipButton)
             │
             ├─ DramaScreenOperationView (全屏操作区)
             │   ├─ Component: DramaScreenOperationPresenter
             │   └─ TurnPageArea (全屏透明按钮，监听点击翻页)
             │
             ├─ SafeArea (剧情专属安全区)
             │   └─ DramaSubtitleView (底部字幕视图)
             │       ├─ Component: DramaSubtitleView
             │       └─ SubtitleText -> TextArea
             │           ├─ Component: DramaSceneText (字幕逻辑组件)
             │           ├─ BodyText (TextMeshProUGUI, 显示正文)
             │           └─ NameText (TextMeshProUGUI, 显示角色名)
             │
             └─ DramaChoiceButtonGroupView (选择支按钮组，由 DramaScene.choiceGroup 引用)
                 ├─ Component: DramaChoiceButtonGroupView (MonoBehaviour)
                 │   └─ 方法: Show(labels, onChoose), Hide, OnDestroy
                 └─ ChoiceButton[] (选项按钮子节点)
                     └─ Component: DramaChoiceButtonView
                         └─ 绑定: DramaChoiceButtonViewModel (Text, ToLabel, IsAlreadySelected)
```

---

## 四、逻辑处理器 (Handler)

```
Handler (逻辑控制中心)
 ├─ ChoiceHandler (选择支控制器)
 │   └─ Component: DramaSceneChoiceHandler (IDramaSceneChoiceHandler)
 │        ├─ choiceGroup: DramaChoiceButtonGroupView (选项按钮组 UI 引用)
 │        ├─ operationHandler: DramaSceneOperationHandler
 │        ├─ jumpHandler: DramaSceneJumpHandler
 │        ├─ chosenLabel: ReactiveProperty<string> (玩家选中的分支标签)
 │        ├─ selectedChoiceIds: IReadOnlyCollection (已选选项 ID 集合)
 │        └─ uiView: IDramaUIView (UI 视图引用)
 │
 ├─ JumpHandler (跳转处理器)
 │
 ├─ OperationHandler (操作处理器)
 │
 ├─ PhysicsHandler (物理处理器)
 │   └─ Component: MagicaClothController (控制头发/裙摆物理)
 │
 ├─ SoundHandler (声音处理器)
 │   ├─ Component: DramaSceneSoundHandler
 │   ├─ VoicePlayer (CriAtomSource 组件) -> [持有资源]: 语音 .acb/.awb (依赖音频流)
 │   ├─ BGMPlayer (CriAtomSource 组件)
 │   └─ SEPlayer (CriAtomSource 组件)
 │
 └─ TimeHandler (时间轴推进器)
```

---

## 4.1 选择支控制器 (ChoiceHandler) 完整结构

```
[逻辑层] Handler/ChoiceHandler
 └─ DramaSceneChoiceHandler
      ├─ choiceGroup ──────────────────┐
      ├─ chosenLabel (ReactiveProperty) │
      ├─ selectedChoiceIds              │
      └─ uiView                         │
                                       │
[UI 层] DramaUIView(Clone)             │
 └─ DramaChoiceButtonGroupView ◄──────┘
      ├─ Show(labels, onChoose)  // 显示选项，labels 来自 Timeline
      ├─ Hide()                  // 隐藏选项
      └─ ChoiceButton[]          // 每个选项一个按钮
           └─ DramaChoiceButtonViewModel (Text, ToLabel, IsAlreadySelected)

[Timeline 层] DramaChoiceTrack
 └─ DramaChoicePlayableAsset
      └─ behaviour.labels: DramaChoiceData[]
           ├─ label (脚本跳转目标，如 "choice_a")
           └─ text (选项显示文本，可本地化)

[工具] DramaSceneChoiceHandlerUtility
 └─ ConvertLabelToId(label)  // 标签与选项 ID 转换
```

**数据流**：Timeline 播放到选择支 Clip → `DramaChoiceMixerBehaviour.Apply` 驱动 → `DramaSceneChoiceHandler.ManualUpdate` 检测 → 调用 `DramaChoiceButtonGroupView.Show(labels)` → 玩家点击 → `chosenLabel` 更新 → `JumpToChoice` 跳转脚本。

---

## 五、其他静态节点

```
├─ Render3DManager (3D 渲染管理器)
│   └─ Component: Render3DManager (管理场景光照、阴影设置)
│
├─ Volume (后期处理体积)
│   └─ Component: PostProcessVolume (景深、泛光等滤镜参数)
│
└─ Directional Light (场景主光源)
    └─ Component: Light
```

---

## 六、动态挂载对象（运行时注入）

在 `ScenarioManager.SetupAsync` 和 Timeline 播放期间，会在根节点下动态生成以下对象：

```
[动态挂载于 s44_01020199_00 或其子节点下]

 ├─ Environment / Stage (3D 背景/舞台)
 │   └─ 实例化的 3D 场景 Prefab (依赖 Stage AB 包)
 │
 ├─ Characters (登场角色组)
 │   ├─ Character_101 (例如：樱木真乃)
 │   │   ├─ Component: Animator (动画状态机)
 │   │   ├─ Component: DramaSceneCharacter (剧情角色控制器)
 │   │   ├─ SkinnedMeshRenderer (角色模型网格) -> [持有资源]: 材质与贴图 (依赖 Character AB 包)
 │   │   └─ 骨骼节点 (Head, Spine, etc.)
 │   └─ Character_102...
 │
 └─ PlayableGraph (Timeline 运行时生成的播放图)
     ├─ AnimationPlayableOutput -> 绑定到 Character 的 Animator
     │   └─ AnimationClip (依赖动作 AB 包)
     ├─ AudioPlayableOutput -> 绑定到 SoundHandler 的 CriAtomSource
     └─ ScriptPlayableOutput -> 绑定到 DramaSubtitleView
         └─ DramaSubtitlePlayableBehaviour (包含当前帧的字幕文本)
```

---

## 七、Timeline 资源内部结构

```
 TimelineAsset (PlayableDirector 持有)
 └─ TrackAsset[]
     ├─ AnimationTrack (动画轨道)
     │   └─ TimelineClip[] -> AnimationClip (依赖动作 AB 包)
     │
     ├─ AudioTrack (音频轨道)
     │   └─ TimelineClip[] -> CriAtomClip (依赖音频 AB 包)
     │
     ├─ DramaSubtitleTrack (字幕轨道)
     │   └─ TimelineClip[] -> DramaSubtitlePlayableAsset
     │        └─ behaviour (DramaSubtitlePlayableBehaviour)
     │             ├─ uniqueId / uuid
     │             ├─ text (对话正文)
     │             ├─ displayTalkerName (角色名)
     │             ├─ mstCharacterInfoId (角色 ID)
     │             └─ cueName (语音 Cue 名)
     │
     ├─ DramaChoiceTrack (选择支轨道)
     │   └─ TimelineClip[] -> DramaChoicePlayableAsset
     │        └─ behaviour (DramaChoicePlayableBehaviour)
     │             └─ labels: DramaChoiceData[] (选项数据)
     │                  ├─ label (跳转目标标签)
     │                  └─ text (选项显示文本)
     │   └─ Mixer: DramaChoiceMixerBehaviour (Apply, NoHitClip)
     │
     └─ CinemachineTrack (镜头轨道)
          └─ TimelineClip[] -> CinemachineShot (镜头配置)
```

---

## 八、AB 包加载锚点映射

| 内存节点 | 资源类型 | 触发加载的入口 |
|----------|----------|----------------|
| 场景根 | 3D Scene Prefab | `PRISM_SceneLoader_LoadAsync` |
| 脚本/表 | JSON | `PRISM_DataFile_GetBytes` |
| 语音 | .acb/.awb | `PRISM_Scenario_ScenarioSound_LoadCueSheetAsync` |
| 角色 | Character Prefab | `ScenarioManager.AttachCharacter` |
| 动作 | AnimationClip | `PlayableDirector.RebuildPlayableGraph` |
| UI 头像 | Sprite | `AdvTitleView._initializeIconAsync` |
| Timeline | TimelineAsset | `PlayableDirector` 引用 |

---

## 九、参考文档

- `SCSP_DRAMA_INSTANCE_DETAIL.json`（场景节点树）
- `SCSP_DRAMA_TIMELINE_INIT.md`（初始化流程）
- `IDA_DRAMA_NATIVE_IMPL.md`（IDA 函数地址）
