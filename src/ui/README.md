# Scripa UI Demo (候选窗 Demo)

这个目录包含一个完整的 Win32 候选窗 Demo，集成了图标工具栏、分页候选列表、ENG/IPA 模式切换及多个交互按钮。

文件：
- `candidate_demo.cpp`：Win32 程序（已更新），支持：
  - 从 `uicon/` 加载 PNG 图标（使用 GDI+）
  - 工具栏按钮：默认、用户、模式切换、翻页、设置
  - 分页显示候选（每页 8 个）
  - 键盘与鼠标交互

构建（使用 Visual Studio 开发者命令提示符）：

```bat
mkdir build
cl /EHsc /W4 /Zi /Fe:build\scripa_ui_demo.exe src\ui\candidate_demo.cpp /link gdiplus.lib user32.lib gdi32.lib shell32.lib
build\scripa_ui_demo.exe
```

图标资源：
- 期望在项目根目录下有 `uicon/` 文件夹，包含以下 PNG 文件（30×30，除 bcg 外）：
  - `bcg.png` — 背景（可选，当前未使用）
  - `default.png` — 打开默认 scheme 文件
  - `user.png` — 用户设置
  - `ENG.png` — 英文模式
  - `IPA.png` — IPA 模式
  - `left.png` — 上一页
  - `right.png` — 下一页
  - `settings.png` — 全局设置
  
如果图标缺失，demo 会使用灰色占位矩形。

交互：
- 数字键 1–8：选择当前页候选
- `[` / `]` 或点击 left/right 按钮：翻页
- 点击工具栏按钮：触发相应功能
- 鼠标点击候选：选中并弹窗显示
- Esc：退出程序
- Enter：选中当前高亮项

说明：
- 该 demo 已连接 `ScripaTSF` 后端，实时获取 composition 与候选
- 后端在 IPA 模式下工作；点击模式按钮可切换（目前仅更改 UI 状态，未传递给引擎）
