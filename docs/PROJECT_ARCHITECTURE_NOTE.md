# scsp-localify 项目架构与模块化设计笔记

## 1. 项目核心模块划分
本项目是一个非常成熟且结构清晰的 C++ 逆向/注入工程，主要在 Native C++ 层对 Unity (Il2Cpp) 游戏进行 Hook 和本地化修改。核心模块主要分布在 `src/` 目录下：

### 底层通信与反射模块
*   **`src/il2cpp/il2cpp_symbols.cpp / .hpp`**
    负责导出和解析游戏底层的 Il2Cpp C API（例如 `il2cpp_resolve_icall`、`il2cpp_class_from_name`、`il2cpp_string_new` 等）。这是所有底层操作、反射和 Hook 的基石。
*   **`src/reflection.cpp / .hpp`**
    对原生的 Il2Cpp API 做了高级封装，便于安全地读取类（Class）、方法（Method）和字段/属性（Field/Property），极大简化了对游戏业务逻辑的探测。

### 系统交互与生命周期
*   **`src/main.cpp` & `src/console.cpp`**
    控制整个外挂 DLL 的生命周期。在初始化阶段分配并挂载了黑框控制台（AllocConsole），且显式将输出编码设置为 UTF-8（`SetConsoleOutputCP(65001)`），这为后续输出中日文字符不乱码提供了完美保障。
*   **`src/mhotkey.cpp / .hpp`**
    系统级热键与输入拦截模块。没有依赖 Unity 的输入系统，而是直接通过 `SetWindowLongPtr` 和 `WndProcCallback` 对 Windows 原生窗口消息（如 `WM_KEYDOWN`）进行了 Hook，确保了热键响应的绝对稳定。

### 核心业务逻辑
*   **`src/hook.cpp`**
    项目最核心的业务逻辑文件（高达 3500+ 行）。集中处理了几乎所有的函数 Hook 逻辑（包括文本替换、游戏系统参数解锁等）。这里也是注入新业务逻辑（如在主线程安全地获取游戏状态）的最佳着陆点。

### UI 渲染模块 (局内菜单)
*   **`src/imgui/` & `src/scgui/`**
    基于 Dear ImGui 库，负责在 Direct3D 渲染层之上绘制开发者调试菜单。

---

## 2. F10 场景树导出功能 (深度=3) 实现方案蓝图

为避免多线程冲突与数据爆炸，将采用“热键标记信号 -> 主线程消费信号 -> Il2Cpp 反射获取 -> 递归缩进打印”的架构。

### 涉及的模块与修改点
1.  **`src/mhotkey.cpp`**: 拦截 F10 按键，发出全局导出信号。
2.  **`src/il2cpp/il2cpp_symbols.hpp / .cpp`**: 补充并声明需要的 Unity 引擎底层获取节点与层级的 API。
3.  **`src/hook.cpp`**: 寻找合适的 Update 钩子消费信号，并在主线程安全的时机执行递归打印逻辑。
