# Scripa UI Demo (候选窗 Demo)

这个目录包含一个最小的 Win32 候选窗 Demo（纯本地、独立运行），用于 UI 设计与交互验证。

文件：
- `candidate_demo.cpp`：Win32 程序，显示组合栏与候选项，支持箭头、数字键和鼠标选择。

构建示例（使用 Visual Studio 开发者命令提示符）：

```bat
cl /EHsc /W4 /Zi /Fe:build\scripa_ui_demo.exe src\ui\candidate_demo.cpp user32.lib gdi32.lib
```

运行：

双击 `build\scripa_ui_demo.exe` 或在命令行运行它。

说明：
- 该 demo 使用硬编码的示例候选列表与组合缓冲（`th`），用于在不依赖 TSF 的情况下快速迭代 UI。将来可以把 `ScripaTSF` 后端接入，动态提供候选与组合字串。
