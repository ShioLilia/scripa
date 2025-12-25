# Copilot / AI 说明（简明）

目的：帮助 AI 编码助手快速理解本仓库的架构、工作流、约定与常见修改点。

- **大体架构**：本项目实现一个 Windows 下的 IPA 输入法（TSF 框架）。主要职责分层：
  - `src/core/Dic.hpp`：字典加载与查询（scheme 文本 -> `std::u32string` 值）。
  - `src/core/Engine.hpp`：输入引擎（模式 `ENG` / `IPA`、缓冲区、候选生成与选择）。
  - `src/main.cpp`：最小化的示例运行器，展示如何用 `SchemeLoader` 加载 `schemes/` 并驱动 `Engine`。
  - UI/TSF 层位于 `tsf/` 与 `ui/` 目录（注册/集成到 Windows 的逻辑），辅助脚本在 `bat/` 下（`register.bat` / `unregister.bat`）。

- **数据流与关键约定（可直接查验源码）**：
  - 输入字符 -> `Engine::inputChar(char)` 推入 `buffer_`。
  - 在 IPA 模式下，`Engine::getCandidates()` 调用 `Dictionary::lookup(buffer)` 返回 `std::vector<std::u32string>`。
  - 选中候选后调用 `Engine::chooseCandidate(size_t)`，将结果以 UTF-32 表示取出并清空缓冲。
  - 字典文件（`schemes/` 中的文本）由 `Dictionary::load` 逐行解析：格式为 `key value_utf8`，注释以 `#` 开头。示例：
    - `th θ`

- **文件/符号示例**（助于定位实现细节）
  - 字典编码/转码在 `src/core/Dic.hpp`（函数 `utf8_to_utf32`）。
  - 引擎行为在 `src/core/Engine.hpp`（方法：`inputChar`, `toggleMode`, `getCandidates`, `chooseCandidate`）。
  - CLI 运行示例在 `src/main.cpp`（使用 `SchemeLoader loader; loader.loadSchemes("schemes/", dict);`）。

- **开发者工作流 / 运行方式（可直接尝试）**
  - 在 Windows 上使用 MSVC/Visual Studio 构建（项目目前无 CMake 文件）；也可以用 `g++` 在 MinGW 下单文件编译用于快速调试。核心点：编译输出的可执行需能访问 `schemes/` 目录。
  - 注册为系统输入法或测试 TSF 集成：先查看并运行 `bat/register.bat` / `bat/unregister.bat`（需管理员权限）。

- **约定与注意事项（针对 AI 的编辑建议）**
  - 不要改变 `Dictionary` 的外部 API（`load`, `lookup`），因为 `Engine` 与其他模块直接依赖这些接口。
  - 不要修改 `schemes/` 中txt文档的文本格式，保持每行 `key value` 的简洁解析格式（参考 `src/core/Dic.hpp` 的解析逻辑），否则需要同时更新解析逻辑。每行右边的IPA音标对应左边不止一种输入的情况。
  - 关于编码：内部候选使用 `std::u32string`（UTF-32），输入与 scheme 文件为 UTF-8。任何对字符编码的改动必须同时更新 `utf8_to_utf32` 使用处。

- **常见修改场景与定位策略**
  - 想要改变候选排序或合并多来源词表：修改 `Dictionary::lookup` 的返回合并逻辑（`src/core/Dic.hpp`）。
  - 想要改变输入到候选的触发规则（例如空格或回车）：修改 `Engine::inputChar` 与 `Engine::getCandidates`（`src/core/Engine.hpp`）。
  - 想要添加新的 scheme 文件解析特性（多字段、优先级）：扩展 `Dictionary::load`，并添加单元样例到 `schemes/`。

- **未在仓库中发现的或需确认的点**
  - 当前没有统一的构建脚本（如 CMake / Makefile）。请告知首选构建工具（MSVC 工程文件 / CMake / other），以便我生成相应构建说明。
  - UI / TSF 的具体实现未在本文档中详述，请指示是否需要我进一步打开 `tsf/`、`ui/` 目录以提取注册/消息流细节。

请检查以上要点是否覆盖你的预期；我可以把其中任何一项扩展为更详细的编辑/PR 指南或补充构建脚本。

cl /std:c++17 /utf-8 /EHsc /W4 /Zi /Fe:..\build\206.exe ..\src\ui\candidate_demo.cpp /link gdiplus.lib user32.lib gdi32.lib shell32.lib

cl /std:c++17 /utf-8 /EHsc /W4 /Zi /Fe:..\build\326.exe ..\src\ui\candidate_demo.cpp ..\src\tsf\ScripaTSF.cpp ..\src\ui\candidate_demo.res /link gdiplus.lib user32.lib gdi32.lib shell32.lib

