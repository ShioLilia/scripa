# Scripa TSF / COM Glue (开发说明)

说明：该目录包含一个最小的 COM 包装器骨架（非完整 TSF 文本服务实现）。目的是：

- 提供一个可注册的 COM 对象（IScripaTSF），它封装了 `ScripaTSF`（位于 `src/tsf/ScripaTSF.*`）。
- 该对象实现基本方法：`Initialize`, `Uninitialize`, `OnKeyDown`, `GetComposition`, `GetCandidates`，方便后续把它嵌入到真正的 TSF TextService 类中。

如何继续：

1. 实现完整的 TSF Text Service 类（继承并实现 TSF 接口，如 `ITfTextInputProcessor` / `ITfTextService`，或使用 ATL 的 `CComObjectRootEx` / `CComCoClass` 进行包装）。在接收到按键事件时，调用本目录中的 `ScripaTSFCOM` 或直接调用 `ScripaTSF`。

2. DllRegisterServer / DllUnregisterServer 需要写入 TSF 特有的注册表项以便 Windows 把该 COM 对象识别为 Text Service。参考 MSDN 文档：注册项位置示例：

   HKLM\SOFTWARE\Microsoft\CTF\TIP\{Your-TIP-CLSID}\
   HKLM\SOFTWARE\Microsoft\CTF\TIP\LanguageProfile\{GUID}\{LangID}\{ProfileGUID}

3. 开发流程建议：先在记事本中通过测试 `ScripaTextServiceCOM` 的 `OnKeyDown` -> `GetComposition` 流程，确认引擎行为；再把逻辑迁入真正的 TSF TextService 类并实现候选窗体交互。

注册（开发）示例：

1. 编译 DLL 到 `build\ScripaTSF.dll`。
2. 以管理员权限运行项目根目录下的 `bat\register.bat`（目前会调用 `regsvr32` 注册 DLL）。

注意：当前 DllRegisterServer 仍为 stub（返回 `E_NOTIMPL`）。正式发布前必须实现完整注册逻辑。
