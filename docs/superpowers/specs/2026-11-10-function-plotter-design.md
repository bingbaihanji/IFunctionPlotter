# 函数绘制器（IFunctionPlotter）设计

日期：2026-11-10
状态：已确认，按此实现

## 1. 目标

ImGui + ImPlot 桌面函数绘图器：用户输入数学表达式，实时绘制曲线；支持多函数叠加、缩放平移、自由变量自动参数滑条。界面采用液体玻璃（liquid glass）风格：实时背景采样、高斯模糊、边缘折射、色散与高光。

## 2. 技术选型

| 项 | 选择 | 理由 |
| --- | --- | --- |
| 窗口/渲染 | Win32 + DirectX 11 | ImGui 自带后端，零第三方依赖 |
| 构建 | CMake + MSVC 2022 (x64), C++17 | 本机已具备 |
| 表达式 | 自研递归下降解析器 | 零依赖、数据流完全可控 |
| 玻璃效果 | 自写 DX11 管线（移植 Demo 的 GLSL） | Demo 为 Android GLES/Vulkan，需迁移 |
| 字体 | 微软雅黑 + Consolas | 中文界面 + 科技感等宽数字 |

第三方源码只复制：ImGui 1.93 核心 4 个 cpp + Win32/DX11 后端；ImPlot 1.1 两个 cpp。均保留 LICENSE。

## 3. 架构

依赖严格单向：`main → ui → app → core`；`core` 不依赖 ImGui/ImPlot/Windows。

```
main.cpp                      Win32 窗口、D3D11、ImGui 生命周期、主循环
platform/App.*                上述平台细节的封装
render/GlassRenderer.*        液体玻璃渲染管线（背景→模糊→面板合成）
render/Shaders.h              HLSL 源码（内嵌）
ui/MainWindow.*               整体布局、玻璃面板登记、标题栏
ui/FunctionPanel.*            函数列表编辑
ui/PlotPanel.*                ImPlot 绘制、采样范围、视图按钮
app/PlotterApp.*              函数集合、参数表、解析/采样编排
app/FunctionEntry.h           单函数状态与采样缓冲
core/Parser.*                 词法 + 递归下降语法分析
core/Expression.*             AST 节点与求值
core/Sampler.*                区间等距采样，非有限值转 NaN 断点
```

数据流（每帧）：

```
文本/滑条变化 → Parser → AST + 自由变量 → ParameterStore
PlotPanel 读取 ImPlot 当前 x 范围与像素宽 → PlotterApp::Sample
→ xs/ys（NaN 断开）→ ImPlot::PlotLine → UI DrawData
```

## 4. 表达式引擎

语法：

```
expr    := term (('+' | '-') term)*
term    := unary (('*' | '/' | '%') unary | 隐式乘法 unary)*
unary   := ('+' | '-') unary | power
power   := primary ('^' unary)?        // 右结合
primary := number | identifier | '(' expr ')' | func '(' args ')'
```

- 数字：`3`、`3.14`、`1e-3`
- 常量：`pi`、`e`；自变量：`x`
- 函数：sin cos tan asin acos atan sinh cosh tanh exp ln log log10 log2 sqrt cbrt abs floor ceil round sign；双参：min max pow atan2 mod
- 隐式乘法：`2x`、`2(x+1)`、`(x+1)(x-1)`、`a(x+1)`
- 可选忽略 `y=` 前缀；标识符统一小写
- 优先级：`-x^2 = -(x^2)`，`2^3^2 = 2^(3^2)`
- 递归深度上限 128

解析产物：`ParseResult { unique_ptr<Expr> expr; vector<string> freeVars; string error; }`。
求值错误（除零、定义域外）返回 NaN/Inf，由采样层转断点，不抛异常。解析错误携带列号。

## 5. 应用与 UI

- 函数列表：可见开关 + 颜色 + 表达式输入 + 删除；编辑即解析，错误红字提示；默认调色板循环。
- 参数：解析后自动收集所有函数自由变量并集；滑条范围 `[-10,10]`；新参数默认 `1.0`；编辑后保留已有取值。
- 绘图：ImPlot 原生缩放/平移；"自动适应"与"重置视图"按钮；每帧按可见 x 范围和 `2×像素宽` 采样（上限 8192）。
- 断点：非有限值置 NaN；ImPlot 自动断开，避免除零/渐近线错误连线。
- 曲线：双层绘制（低透明度粗线模拟辉光 + 细亮线），霓虹配色。
- 单线程、无缓存机制，保持最小实现。
- 标题栏“界面设置”弹窗：渐变背景开关、桌面背景采样开关、玻璃不透明度滑条；弹窗自身使用液体玻璃面板。
- 桌面背景采样开启时，面板浸染降为无色、渐变背景被桌面画面替代，玻璃采样窗口后方真实桌面。

## 6. 液体玻璃桌面迁移

参考 `D:\cLib\demo\imgui-liquid-glass-main`（MIT，源自 AndroidLiquidGlassView，MIT）。桌面端实现：

1. 每帧将程序化科技背景渲染到全尺寸离屏纹理（渐变、极光光斑、网格、扫描光、噪点）。
2. 背景 → 半分辨率 4-tap box downsample → 可分离高斯 H/V 两 Pass（12 对 bilinear tap）。
3. 玻璃面板 Shader 移植自 Demo `liquid_glass.frag` + common 模块：圆角 SDF、边缘折射、RGB 色散、染色/对比度、内外阴影、边缘高光、像素覆盖率抗锯齿。
4. 面板绘制顺序：背景副本书写后缓冲 → 玻璃面板 → ImGui DrawData。ImGui 窗口背景透明，控件浮于玻璃之上。
5. 面板参数（默认）：圆角 26/28、折射高度 32、折射量 -90~-130、色散 0.65、外阴影 0.42/22px、模糊半径 28。
6. 窗口：无边框 WS_POPUP，自定义标题栏（拖动/最小化/最大化/关闭），WM_NCHITTEST 支持边缘缩放；Win11 DWM 圆角/暗色。
7. 背景渐变：`#005a4b → #01738a`，叠加低强度青绿/青色光斑、细网格、扫描光与噪点；可在设置中关闭为深色纯色。
8. 桌面背景采样：使用 DXGI Desktop Duplication 获取窗口所在显示器的桌面快照，`WDA_EXCLUDEFROMCAPTURE` 排除本窗口，避免镜像反馈；着色器按窗口位置采样快照，面板浸染降为无色。
9. 实时模式窗口不参与屏幕捕获；关闭采样时恢复 `WDA_NONE`。
10. 录屏兼容模式：窗口保持可被截图/录屏，玻璃使用一张静态桌面快照。开启或“刷新背景”时临时设置 `WDA_EXCLUDEFROMCAPTURE`，通过 `DwmFlush` + Desktop Duplication 抓取一帧后立即恢复 `WDA_NONE`；之后不再逐帧更新，窗口拖动/缩放仍按帧重新采样该快照。
11. 窗口拖动与边缘缩放不走 Windows 原生模态循环：标题栏拖动按帧轮询 `GetCursorPos` 并 `SetWindowPos`，边缘缩放拦截 `WM_NCLBUTTONDOWN` 后逐帧计算新矩形。主循环持续运行，因此拖动/缩放期间玻璃仍每帧采样桌面并重新渲染。

## 7. 构建与验证

- `cmake -S . -B build -G "Visual Studio 17 2022" -A x64`
- `cmake --build build --config Release --parallel`
- 启动即验收：中文显示、玻璃面板、背景动画、输入 `sin(x)` 出曲线、缩放平移、多函数、参数滑条、错误提示。
- 不编写自动化测试；由用户人工按业务流程验证。

## 8. 实施顺序

1. 工程骨架：third_party、CMake、Win32/DX11、ImGui 中文字体、空窗口
2. 表达式引擎 + 单函数输入/绘制
3. 多函数列表
4. 自由变量参数滑条
5. 液体玻璃渲染管线与科技风样式
6. 打磨与人工验收
