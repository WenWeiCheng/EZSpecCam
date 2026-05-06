---
name: windows-exe-missing-dependency
description: Windows 可执行文件运行时无输出、弹窗提示缺少依赖的诊断技能。帮助识别 DLL 缺失问题，避免误以为无输出就是程序成功执行。
---

## 问题现象

运行 Windows 可执行文件（.exe）时：
- 命令行没有任何输出（甚至没有错误信息）
- 可能弹窗提示"缺少 xxx.dll"
- 程序无法启动

**重要**：**无输出 ≠ 程序成功运行**，特别是在 Windows GUI 应用程序中。

## 诊断步骤

### 步骤 1：获取退出码

使用 PowerShell 获取进程退出码：

```powershell
$p = Start-Process -FilePath 'your_app.exe' -PassThru -Wait
Write-Host 'Exit code:' $p.ExitCode
```

或使用 cmd：

```cmd
your_app.exe
echo Exit code: %errorlevel%
```

### 步骤 2：解析退出码

常见退出码含义：

| 退出码 | 十六进制 | 含义 |
|--------|----------|------|
| -1073741515 | 0xC0000135 | **STATUS_DLL_NOT_FOUND** - 缺少 DLL |
| -1073741701 | 0xC0000139 | **ENTRY_POINT_NOT_FOUND** - DLL 函数入口找不到 |
| -1073741510 | 0xC000013A | STATUS_ENTRYPOINT_NOT_FOUND |
| 0 | - | 程序正常退出 |

**如果退出码是负的大数（特别是 -1073741515），几乎可以确定是 DLL 缺失。**

### 步骤 3：使用 dumpbin 查看依赖

使用 Visual Studio 的 dumpbin 工具查看 exe 依赖了哪些 DLL：

```bash
# WSL 环境下
"/mnt/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/dumpbin.exe" /dependents "C:\\path\\to\\your_app.exe"

# Windows PowerShell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\dumpbin.exe' /dependents "C:\path\to\your_app.exe"
```

输出示例：
```
Image has the following dependencies:
    exiv2.dll
    Qt6Widgetsd.dll
    Qt6Guid.dll
    Qt6Cored.dll
    MSVCP140D.dll
    KERNEL32.dll
    VCRUNTIME140D.dll
    ucrtbased.dll
```

### 步骤 4：查找缺失的 DLL

常见依赖路径：

| 依赖类型 | 典型路径 |
|----------|----------|
| Qt 库 | `C:\Qt\6.8.2\msvc2022_64\bin` |
| vcpkg 库 | `C:\Users\weichen\vcpkg\installed\x64-windows\debug\bin` |
| vcpkg 库 (release) | `C:\Users\weichen\vcpkg\installed\x64-windows\lib` |
| MSVC 运行时 | `C:\Windows\System32` |

### 步骤 5：验证修复

添加依赖路径到 PATH 后重新运行：

```powershell
$env:Path = "C:\Qt\6.8.2\msvc2022_64\bin;C:\Users\weichen\vcpkg\installed\x64-windows\debug\bin;" + $env:Path
.\your_app.exe
```

如果程序能正常启动并输出到命令行，说明问题已解决。

## 常见场景

### Qt 应用程序

Qt GUI 应用程序默认没有 attached console，DLL 加载失败时：
- 不会在命令行显示任何错误
- 可能弹窗提示缺少 DLL
- 使用 `CONFIG += console` 可以让程序输出到控制台

### 测试可执行文件

Qt 单元测试（QTest）可能依赖：
- Qt 库（Qt6Cored.dll, Qt6Testd.dll 等）
- vcpkg 库（exiv2.dll 等）
- MSVC 运行时库

### 插件/Driver DLL

如果程序使用插件系统（如 Qt Plugin），还需要确保插件依赖的 DLL 也在可加载路径中。

## 快速检查清单

- [ ] 获取退出码（排除 0）
- [ ] 检查退出码是否为 -1073741515 (DLL_NOT_FOUND)
- [ ] 使用 dumpbin 查看依赖
- [ ] 确认依赖的 DLL 路径在 PATH 中
- [ ] 添加缺失路径后重新运行验证

## 相关工具

- **dumpbin**: Visual Studio 自带的 PE 依赖查看工具
- **Dependencies** (GitHub: lucasg/Dependencies): 图形化的 DLL 依赖查看器
- **Process Monitor**: Sysinternals 工具，可监控程序加载了哪些 DLL
