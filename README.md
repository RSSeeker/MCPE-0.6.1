# MCPE 0.6.1（Windows 桌面版）

基于 [4chan 泄漏的 MCPE 0.6.1 开发者构建源码](https://boards.4chan.org/g/thread/108264778/minecraft-ps3-edition-source-code) 改造的 Windows 桌面版本。编译产物为**单个可执行文件**，双击即可游玩，无需安装、无外部依赖。

## 与原版相比的改动

### 构建与发布

- **单文件便携版**：游戏数据（贴图、语言包等）和运行库（libEGL / libgles_cm / libpng12 / zlib1）全部内嵌进 exe。首次启动自动解压到 `%TEMP%\MCPE061-<hash>`（内容变更自动换目录），不污染 exe 所在目录。
- **静态链接** VC 运行库（/MT），目标机器无需安装 Visual C++ 运行库；加载依赖只剩 Windows 系统 DLL。
- Release 为窗口程序（不弹控制台），自带 MCPE 图标，窗口标题 "Minecraft PE 0.6.1"。
- **存档跟随 exe**：`games\` 与 `options.txt` 生成在 exe 旁边，拷走整个 exe 即可带走进度。
- `build.bat` 一键构建：`build.bat single`（单文件版，默认）/ `debug` / `gl`。
- GitHub Actions 自动构建：输出单文件 exe 与完整压缩包，自动创建预发布（prerelease）。

### 操作方式（完全键盘 + 鼠标）

- 移除触摸 UI（左下角方向键、触摸菜单等），改为桌面式菜单与鼠标操作。
- **鼠标捕获**：进入世界后隐藏并锁定光标（FPS 式），使用**原始输入（RAW INPUT）**转视角，不受 Windows 指针加速影响；菜单、暂停、Alt+Tab 自动释放。
- **准星**：屏幕中心显示，仅游戏内可见。
- 按钮悬停高亮跟随鼠标。
- 鼠标滚轮切换快捷栏。

| 按键 | 功能 |
|---|---|
| WASD | 移动 |
| 空格 | 跳跃；创造模式**双击起飞 / 落地**；飞行中上升 |
| Shift | 潜行；飞行中下降 |
| 鼠标左键 / 右键 | 破坏 / 放置 |
| E | 打开 / 关闭物品栏（创造 = 物品选择器，生存 = 2×2 合成） |
| 滚轮 / 数字键 1-9 | 切换快捷栏 |
| ESC | 暂停菜单 / 关闭界面 |

### 创造模式飞行

- 双击空格起飞，再次双击落地；飞行中按住空格上升、按住 Shift 下降。
- 双击空格飞行与 F 键无碰撞飞行两种模式都支持空格/Shift 升降。

### 设置界面

- 修复原版 Options 界面几乎空白的问题：Account（灵敏度）、Game（第三人称 / 隐藏 GUI / 服务器可见）、Controls（反转鼠标 / 左手模式）、Graphics（渲染距离 / 视角摆动 / 平滑光照 / 性能）全部可用。
- 补全缺失的语言文本（组标题、选项名等）。
- **灵敏度滑块范围 0~4，默认 1**：线性映射，1 = 旧版最大手感，4 = 旧版最大值的 4 倍。

### 问题修复

- 滚轮不再导致视角乱飞 / 看天。
- 右键放置不再连带破坏方块。
- Shift 键码映射修复（兼容 `VK_SHIFT` / `VK_LSHIFT`），潜行与飞行下降生效。
- 视角移动改为线性、与帧率无关（原来是按 tick 累积 + 非线性曲线，导致时快时慢）。
- 物品栏打开后按 E 可关闭（与 ESC 同路径）。
- 窗口失去焦点自动释放鼠标捕获。

## 构建

需要 Visual Studio 2022+（或 Build Tools，含 C++ 桌面开发）。

```bat
build.bat            :: 单文件版（打包资源 → 编译 Release → 输出 dist\MinecraftPE-0.6.1.exe）
build.bat debug      :: 传统 Debug 版
build.bat gl         :: GL Debug 版
```

发布版 exe 位于 `dist\MinecraftPE-0.6.1.exe`。
