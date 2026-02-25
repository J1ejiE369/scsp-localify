# 剧情文本与UI节点定位笔记

## 1. 剧情对话框 (Subtitle)
位于 `s44_...` 实例下的 `UICanvas` 中。

*   **根路径**: `CanvasRoot/UICanvas/SafeArea/DramaSubtitleView`
*   **关键文本节点**:
    *   **角色名**: `SubtitleText/TextArea/NameText`
    *   **对话内容**: `SubtitleText/TextArea/BodyText`
    *   **装饰线**: `SubtitleText/TextArea/Line`
    *   **下一页箭头**: `SubtitleText/Indicator`

## 2. 剧情选项分支 (Choices)
当出现多选一的分支时，UI 会激活 `DramaChoiceGroup`。

*   **根路径**: `CanvasRoot/UICanvas/SafeArea (1)/DramaChoiceGroup/ButtonRoot`
*   **关键文本节点**:
    *   **选项按钮**: `DramaChoiceButton`, `DramaChoiceButton (1)`, `DramaChoiceButton (2)` ...
    *   **选项文字**: `DramaChoiceButton/LabelText`
    *   **选中高亮**: `DramaChoiceButton/SelectedImage`

## 3. 剧情标题卡 (Title Card)
位于左上角的章节信息板。

*   **根路径**: `CanvasRoot/UICanvas/DramaUIViewRoot/DramaUIView(Clone)/Cmn_SafeArea/AdvTitleView`
*   **关键文本节点**:
    *   **主标题**: `AnimatedObjects/content/1LineTitle/Title` 或 `2LineTitle/ContentSizeFitter/Title1`
    *   **角色图标**: `AnimatedObjects/content/icon`

## 4. 剧情菜单 (Menu)
点击屏幕呼出的暂停/设置菜单。

*   **根路径**: `CanvasRoot/UICanvas/DramaUIViewRoot/DramaUIView(Clone)/DramaMenuView`
*   **功能按钮**: `SkipButton` (跳过), `LogButton` (回放), `HideDramaMenuButton` (隐藏UI)
