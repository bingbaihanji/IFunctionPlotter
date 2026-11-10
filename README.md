# IFunctionPlotter

基于 **Dear ImGui 1.93 + ImPlot 1.1** 的桌面函数绘图器，使用 **Win32 + Direct3D 11** 渲染。界面采用液体玻璃风格：实时背景采样、高斯模糊、边缘折射、色散、边缘高光与外阴影。

## 功能

- 输入数学表达式，逐帧采样并实时绘制曲线
- 多函数叠加：颜色、显示/隐藏、删除
- 自动识别自由变量并生成参数滑动条（例如 `a*sin(b*x)` 自动生成 `a`、`b`）
- ImPlot 原生滚轮缩放、拖拽平移
- “自动适应”与“重置视图”
- 表达式错误中文提示（含列号），除零/定义域外自动断线
- 中文界面，微软雅黑 + Consolas 等宽字体
- 青绿渐变背景 `#005a4b → #01738a`
- 标题栏“界面设置”按钮：渐变开关、桌面背景采样、玻璃不透明度
- 桌面背景采样：无色液体玻璃采样窗口后方桌面（DXGI Desktop Duplication）
- 录屏兼容模式：使用静态桌面快照作为玻璃背景，窗口可正常截图/录屏
- 自实现逐帧窗口拖动/缩放：拖动、缩放期间渲染循环不中断，玻璃与桌面背景持续实时更新

## 表达式语法

| 类别 | 内容 |
| --- | --- |
| 自变量 | `x` |
| 常量 | `pi`、`e` |
| 一元函数 | `sin cos tan asin acos atan sinh cosh tanh exp ln log log10 log2 sqrt cbrt abs floor ceil round sign` |
| 双参函数 | `min max pow atan2 mod` |
| 隐式乘法 | `2x`、`2(x+1)`、`(x+1)(x-1)`、`a sin(x)` |
| 等式 | 可省略 `y=` 前缀 |

优先级：`-x^2 = -(x^2)`，`2^3^2 = 2^(3^2)`。

## 构建

环境要求：

- Windows 10/11
- Visual Studio 2022（MSVC v143，含 Windows SDK）
- CMake 3.20+

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
```

产物：`build/Release/IFunctionPlotter.exe`（无控制台窗口）。

Debug 构建：

```powershell
cmake --build build --config Debug --parallel
```

## 操作

- 标题栏可拖动；双击标题栏最大化；右上角最小化/最大化/关闭
- 标题栏滑块图标：打开“界面设置”
- 窗口边缘可缩放
- 曲线区域：滚轮缩放、左键拖拽平移、右键菜单
- 参数滑条：拖动调节；`Ctrl+点击` 可输入精确值

> 桌面背景采样有两种模式：
>
> - **实时模式**：窗口设置为 `WDA_EXCLUDEFROMCAPTURE`，玻璃逐帧采样真实桌面，但本窗口不会被截图/录屏捕获。
> - **录屏兼容模式**：使用静态桌面快照作为玻璃背景，窗口可正常截图/录屏；开启或点击“刷新背景”时会短暂排除窗口以抓取快照，背景不逐帧更新。
>
> 如需录制带桌面背景的演示，请开启“桌面背景采样”后再开启“录屏兼容模式”。

## 架构

依赖方向严格单向：`main → ui → app → core`，`core` 不依赖任何 GUI/Windows 头文件。

```
main.cpp / platform/App.*      Win32 窗口、D3D11、ImGui 生命周期、主循环
platform/DesktopCapture.*      DXGI Desktop Duplication 桌面背景捕获
render/GlassRenderer.*        液体玻璃管线（场景 → 下采样 → 高斯 → 面板合成）
render/Shaders.h              HLSL：青绿渐变/桌面场景、模糊、折射/色散/高光
ui/                            标题栏、设置弹窗、函数列表、参数栏、ImPlot 绘图区
ui/UiWidgets.*                 开关、设置图标等共享控件
app/PlotterApp.*               函数集合、参数表、解析与采样编排
core/Parser.*                  Tokenizer + 递归下降解析
core/Expression.*              AST 求值（解释器模式）
core/Sampler.*                 区间等距采样，非有限值转 NaN
```

每帧数据流：

```
表达式文本 → Parser → AST + 自由变量 → 参数表
→ ImPlot 当前可见 x 范围 → Sampler → xs/ys
→ ImPlot::PlotLine → ImGui DrawData
```

液体玻璃每帧执行：

1. 程序化科技背景（渐变、极光光斑、网格、扫描光）渲染到离屏纹理
2. 半分辨率 4-tap 下采样 + 可分离高斯 H/V 模糊
3. 玻璃面板 Shader 采样模糊背景，计算圆角 SDF、边缘折射、RGB 色散、染色、内外阴影与边缘高光
4. 面板合成后再绘制 ImGui 控件

## 第三方

- [Dear ImGui 1.93](https://github.com/ocornut/imgui) — MIT
- [ImPlot 1.1](https://github.com/epezent/implot) — MIT
- 液体玻璃 Shader 迁移自 `imgui-liquid-glass-main`（MIT，源自 AndroidLiquidGlassView，MIT）

详见 `third_party/THIRD_PARTY_NOTICES.md`。

## 测试说明

项目不包含自动化测试。构建后由人工按上述操作路径验证功能。
