---
name: execute-windows-cmd-in-wsl
description: 当系统环境是 WSL，但是需要 Windows 程序和库的支持，比如搜索 Windows 下的路径，执行 Windows 命令或执行在 Windows 上才能运行的编译脚本，使用此技能。
---

### 调用 Windows 可执行文件
这是最直接的方法，适用于调用 Windows 系统工具（如 `cmd.exe`、`msbuild.exe`）或已安装的编译工具（如 Visual Studio 的编译器）。

*   **语法**：直接在 WSL 终端中输入 Windows 可执行文件的路径和名称。
*   **示例**：
    ```bash
    # 调用 Windows 记事本
    notepad.exe
    # 调用 Windows 的 ipconfig 命令
    ipconfig.exe
    # 调用 Visual Studio 的 MSBuild 编译项目
    /mnt/c/Program\ Files/Microsoft\ Visual\ Studio/2022/Professional/MSBuild/Current/Bin/MSBuild.exe
    ```

可以看到需要有后缀 `.exe`，如果输入 `notepad` 会找不到。

### 通过 cmd.exe 执行批处理脚本
如果你的编译脚本是 `.bat` 或 `.cmd` 文件，或者需要依赖 Windows 命令提示符的特定环境（如 VS 开发人员命令提示符），可以使用 `cmd.exe` 来执行。

*   **语法**：`cmd.exe /c "脚本路径"`
*   **示例**：
    ```bash
    # 执行位于 Windows C 盘的编译脚本
    cmd.exe /c "C:\Scripts\build.bat"
    # 执行位于 WSL 文件系统中的脚本（需转换路径）
    cmd.exe /c "$(wslpath -w ./my_script.bat)"
    ```

### 使用 wslpath 进行路径转换
WSL 提供了 `wslpath` 工具，用于在 Windows 路径（如 `C:\Users`）和 WSL 路径（如 `/mnt/c/Users`）之间进行转换。这在处理文件路径参数时非常有用。

*   **语法**：
    *   `wslpath -w <Linux路径>`：将 Linux 路径转换为 Windows 路径。
    *   `wslpath -u <Windows路径>`：将 Windows 路径转换为 Linux 路径。
*   **示例**：
    ```bash
    # 假设脚本需要 Windows 格式的路径作为参数
    my_script.exe "$(wslpath -w /home/user/project)"
    ```

### 混合命令与管道操作
WSL 支持将 Windows 命令的输出通过管道传递给 Linux 命令，反之亦然。

*   **示例**：
    ```bash
    # 使用 Windows 的 ipconfig 输出，通过 Linux 的 grep 过滤
    ipconfig.exe | grep IPv4
    # 使用 Windows 的 dir 命令，通过 Linux 的 wc 统计行数
    cmd.exe /c dir | wc -l
    ```
