# Scripa TSF 输入法 UI 功能指南

## 1. 自定义候选窗口

### 显示内容
- **顶部**: 当前输入缓冲区内容 (例如: `Buffer: ama`)
- **候选列表**: 每页显示8个候选项
  - 每个候选项显示为矩形框
  - 左侧显示序号 (1-9)
  - 选中的候选项用蓝色高亮 (RGB 0, 120, 215)
- **底部**: 页码指示器 (例如: `Page 1/3`)

### 选择候选
1. **鼠标点击**: 直接点击候选项
2. **大键盘数字键**: 按1-9选择对应候选
3. **空格键**: 选择第一个候选 (序号1)

### 翻页操作
- **PageDown / ]**: 下一页
- **PageUp / [**: 上一页
- 页码显示在候选窗口底部 (例如: `Page 2/5`)
- 每页显示最多8个候选项

### 窗口特性
- 自动跟随光标位置显示
- 半透明背景
- 使用GDI+渲染，支持Unicode字符

## 2. 工具栏

### 位置
- 固定在屏幕右上角 (距离右边缘10像素)
- 尺寸: 150x40 像素

### 三个按钮
1. **模式指示器** (左侧)
   - IPA模式: 显示 `IPA.png` 图标
   - ENG模式: 显示 `ENG.png` 图标
   - 点击切换模式

2. **默认按钮** (中间)
   - 显示 `default.png` 图标
   - 点击打开方案参考窗口 (TODO)

3. **设置按钮** (右侧)
   - 显示 `settings.png` 图标
   - 点击打开设置菜单 (TODO)

### 图标资源
图标文件位于: `uicon/`
- `ENG.png` - ENG模式指示
- `IPA.png` - IPA模式指示
- `settings.png` - 设置按钮
- `default.png` - 方案参考按钮

## 3. 模式切换

### Ctrl+Shift 快捷键
- **功能**: 在IPA模式和ENG模式之间切换
- **实现**: TSF保留键 (Preserved Key)
- **效果**: 
  - 工具栏图标自动更新
  - 切换后立即生效

### 两种模式
- **IPA模式**: 输入拉丁字母转换为IPA符号
- **ENG模式**: 直接输入拉丁字母，不转换

## 4. 数字键处理

### 大键盘数字键 (1-9)
- **功能**: 选择当前页的候选项
- **检测方法**: 检查lParam的扩展键标志位 (bit 24)
- **示例**: 
  - 按 `1` → 选择当前页第1个候选
  - 按 `9` → 选择当前页第9个候选
  - 跨页选择: 先翻页，再按数字键

### 小键盘数字键 (Numpad 0-9)
- **功能**: 作为普通输入处理
- **检测方法**: 虚拟键码 VK_NUMPAD0 到 VK_NUMPAD9
- **示例**: 
  - 按 `Numpad 5` → 输入字符 '5'
  - 在IPA模式下可能触发转换

### 方括号和翻页键
- **功能**: 候选窗口翻页
- **按键映射**:
  - `[` 或 `PageUp` → 上一页
  - `]` 或 `PageDown` → 下一页
- **行为**: 仅在有候选列表时生效

## 5. 架构说明

### 文件结构
```
src/tsf/
├── TextService.h          # TSF主服务类
├── TextService.cpp        # TSF服务实现
├── CandidateWindow.h      # 候选窗口类
├── CandidateWindow.cpp    # 候选窗口实现
├── ScripaTSF.h           # 后端引擎接口
├── ScripaTSF.cpp         # 后端引擎实现
├── dllmain.cpp           # DLL入口和COM注册
└── ScripaTSF.def         # DLL导出定义
```

### 关键类

#### CTextService
- 实现 `ITfTextInputProcessor` 接口
- 管理组合 (composition) 和上下文 (context)
- 处理键盘事件
- 管理UI窗口生命周期

#### CCandidateWindow
- 继承自Win32窗口
- 使用GDI+渲染
- 方法:
  - `Create()` - 创建窗口
  - `Update(candidates, selected)` - 更新候选列表
  - `Move(x, y)` - 移动到指定位置
  - `Show(visible)` - 显示/隐藏

#### CToolbar
- 独立的工具栏窗口
- 加载PNG图标
- 方法:
  - `UpdateMode(isIPA)` - 更新模式图标
  - `_ShowSettingsMenu()` - 打开设置菜单 (TODO)
  - `_ShowSchemeReference()` - 打开方案参考 (TODO)

### 事件流程

#### 键盘输入
1. `OnTestKeyDown` - 预测试是否处理此键
2. `OnKeyDown` - 实际处理按键
   - 区分大键盘/小键盘数字
   - 调用 `_backend.OnKeyDown(ch)`
   - 启动/更新组合
3. `_UpdateCompositionString` - 更新TSF组合
4. `_UpdateCandidateWindow` - 刷新候选窗口
5. `_PositionWindows` - 调整窗口位置

#### 候选选择
1. 用户点击候选或按数字键
2. `_OnCandidateSelected(index)` 被调用
3. 创建 `CInsertTextEditSession`
4. 通过 `ITfRange::SetText` 插入文本
5. `EndComposition` 结束组合
6. 清空缓冲区并隐藏候选窗口

## 6. 编译和安装

### 构建命令
```bat
cd bat
build_tsf.bat
```

### 注册输入法
```bat
cd bat
register.bat  (需要管理员权限)
```

### 编译依赖
- Visual Studio 2022
- Windows SDK (TSF API)
- GDI+ (包含在 gdiplus.lib)

### 链接的库
- `ole32.lib` - COM
- `oleaut32.lib` - COM自动化
- `uuid.lib` - GUID
- `user32.lib` - Windows API
- `advapi32.lib` - 注册表
- `gdiplus.lib` - GDI+渲染

## 7. 调试技巧

### 查看输出
使用 DebugView++ 查看 `OutputDebugString` 消息

### 常见问题
1. **候选窗口不显示**: 检查GDI+初始化是否成功
2. **图标不加载**: 确认 `uicon/*.png` 文件存在
3. **数字键不响应**: 检查lParam标志位判断逻辑
4. **Ctrl+Shift无效**: 确认 `_InitPreservedKey()` 成功调用

### 重新注册
如果修改了代码：
```bat
unregister.bat
build_tsf.bat
register.bat
```
然后重启explorer或重启系统。

## 8. 待实现功能 (TODO)

- [ ] 设置菜单窗口 (SettingsMenuProc)
- [ ] 方案参考窗口 (SchemeReferenceWndProc)
- [ ] 自定义字典编辑
- [ ] 方案启用/禁用界面
- [ ] 候选窗口翻页 (PageUp/PageDown)
- [ ] 更多快捷键支持

## 9. 与demo exe的差异

### 相同之处
- GDI+渲染逻辑
- 候选项显示样式
- 图标资源

### 不同之处
- **窗口类型**: TSF使用 WS_EX_NOACTIVATE 防止抢夺焦点
- **位置管理**: TSF通过 ITfContextView::GetTextExt 获取光标位置
- **文本插入**: TSF使用 ITfRange::SetText 而不是SendInput
- **事件模型**: TSF基于编辑会话 (Edit Session)
