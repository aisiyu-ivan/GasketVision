# GasketVision 项目架构

## 1. 概述

工业垫片视觉质检系统，**三进程**同机部署：

| 进程 | 可执行文件 | 职责 |
|------|------------|------|
| **HMI** | `HMI.exe` | 正式人机界面（相机模式） |
| **HMI_Test** | `HMI_Test.exe` | 测试人机界面（合成样品），与 `HMI.exe` 共用源码、编译期锁定配置 |
| **CameraService** | `CameraService.exe` | 采图（合成样本 / GigE 骨架） |
| **VisionEngine** | `VisionEngine.exe` | 读图、OpenCV 检测、结果上报 |

正式环境双击 **`run.bat`**（编译并启动 HMI，Engine/Camera 由 HMI 分离拉起）；合成测试双击 **`test/run_test.bat`**。**两程序不应同时运行**（IPC 名称相同）。

---

## 2. 目录结构

```
GasketVision/
├── CMakeLists.txt
├── run.bat                     # 正式：编译 + 启动 HMI（Engine/Camera 由 HMI 分离拉起）
├── config/                     # 正式环境配置源（编译时复制到 build/）
│   ├── vision_engine.json      # 真机采图与检测配置（gige）
│   ├── pie_chart_okng.json     # HMI OK/NG 初始计数
│   └── templates/              # 定位模板
├── test/                       # 合成测试环境（独立配置，不来自 config/）
│   ├── run_test.bat            # 测试：生成样品 + 启动 HMI_Test（Engine/Camera 由 HMI 分离拉起）
│   ├── vision_engine.json
│   ├── pie_chart_okng.json
│   ├── station1/               # run_test.bat 生成的样品图
│   ├── templates/
│   └── scripts/                # 样本生成与诊断脚本
├── build/                      # CMake 输出目录（exe、DLL、config 副本）
├── docs/
│   ├── architecture.md         # 本文档
│   └── attention.md            # 面试问答
├── HMI/
│   ├── main.cpp
│   ├── ui/
│   ├── stats/
│   ├── socket/engine_hmi/      # EngineHmiSocketServer + 协议
│   └── ipc/                    # 收引擎检测 SHM、InspectionDataService
├── CameraService/
│   ├── camera/
│   ├── socket/camera_engine/   # CameraEngineSocketClient
│   └── ipc/                    # 写相机 SHM
├── VisionEngine/
│   ├── worker/
│   ├── algo/
│   ├── socket/                 # Camera Server + EngineHmi Client + 相机协议
│   ├── ipc_camera/             # 读相机 SHM
│   └── ipc_hmi/                # 写 HMI 检测 SHM
└── Pie_Chart_Control/          # 旧版饼图程序（独立保留）
```

### 2.1 正式 vs 测试：两套入口，一套代码

检测链路相同，差异只在**读哪份配置、从哪个目录启动**：

| 项目 | 正式系统 | 测试系统 |
|------|----------|----------|
| HMI 程序 | `build/HMI.exe` | `build/HMI_Test.exe` |
| 编译宏 | 无 | `GASKETVISION_HMI_TEST=1` |
| 界面标题 | 工业垫片视觉检测系统 | 工业垫片视觉检测系统 — 测试 |
| 顶部标签 | 相机模式（固定） | 测试模式（固定） |
| 配置来源 | `config/` → 编译复制到 `build/` | `test/vision_engine.json`（独立维护） |
| 工作目录 | `build/` | `test/` |
| `imageSource` | `gige` | `synthetic` |
| 启动脚本 | `run.bat` | `test/run_test.bat` |
| 子进程 | `build/` 下 CameraService / VisionEngine | 同上 exe，argv 传 `test/vision_engine.json` |

**无运行时模式切换**：`HMI.exe` 与 `HMI_Test.exe` 共用 `HMI/` 源码，编译期锁定 profile。**不要同时运行两个 HMI**（IPC 名称均为 `GasketVision.*`）。

### 2.2 config/ 与 .bat 的关系

`.bat` 不直接读写 `config/`，流程是：

```
config/  ──CMake POST_BUILD 复制──►  build/vision_engine.json
                                      build/templates/
                                      build/pie_chart_okng.json
         run.bat / test/run_test.bat ──►  首次无 build 时自动 CMake；再编译并启动三进程

test/vision_engine.json  ──►  test/run_test.bat（生成 station1 + 启动 HMI_Test 等，cwd=test/）
```

- 改相机 IP、公差、正式模板 → 编辑 **`config/`**，再执行 **`run.bat`** 重新编译。
- 改测试样品、合成间隔 → 编辑 **`test/`** 或重新运行 **`test/run_test.bat`**，无需动 `config/`。

### 2.3 根目录脚本说明

| 脚本 | 作用 |
|------|------|
| `run.bat` | 正式：编译 + 启动 HMI（cwd=`build/`，Engine/Camera 自动分离拉起） |
| `test/run_test.bat` | 测试：生成样品 + 启动 HMI_Test（cwd=`test/`） |

构建后 `config/` 内容在编译时复制到 `build/`；**首次运行**若尚无 `build/*.exe` 会自动 CMake 配置。

---

## 3. 进程职责

### 3.1 HMI / HMI_Test

| 模块 | 主要类 | 作用 |
|------|--------|------|
| `ui/` | `InspectionWindow` | 图像区、工具栏、状态栏；提示独立启动 VisionEngine / CameraService |
| `stats/` | `OkNgStatsPanel`、`InspectionAggregator` | OK/NG 计数与展示 |
| `ipc/` | `HmiIpcSubscriber`、`InspectionDataService` | 收检测 SHM、聚合 OK/NG、刷 UI |
| `HMI/socket/engine_hmi/` | `EngineHmiSocketServer` | listen `GasketVision.EngineHmi.Control`；收 `ready`/`result`、发 `ack`；心跳超时断连 |

**线程**：主线程（事件循环 + UI）+ 1 个通信子线程（读 HMI SHM + `EngineHmiSocketServer`，同线程处理 `ready` 与 `ack`）。

`InspectionWindow` 通过编译宏 `GASKETVISION_HMI_TEST` 区分正式/测试：正式版固定 `resolveCameraWorkDir()`，测试版固定 `resolveTestWorkDir()`，无设置菜单切换。

### 3.2 CameraService

| 模块 | 主要类 | 作用 |
|------|--------|------|
| `camera/` | `IVisionImageSource`、`SyntheticImageSource`、`GigEVisionCamera`、`ImageSourceFactory` | 按配置采图 |
| `ipc/` | `CameraIpcPublisher`、`CameraIpcSync`、`CameraIpcLayout` | 写相机 SHM |
| `CameraService/socket/camera_engine/` | `CameraEngineSocketClient` | connect `GasketVision.Camera.Control`；严格模式 **写 SHM 前** `sendReadyAndWaitAck` |

**线程**：**主线程** + **采图线程**（按间隔 `grabFrame`）+ **发布线程**（**写 SHM 前** ready/ack、publish）。严格模式下采图与发布 **解耦**：采图持续进行，发布队列最多 **4 帧**，满则丢新帧；环缓反压只阻塞 **发布写 SHM**。

**正式**（`build/` + `HMI.exe`）：`config/vision_engine.json` → `gige`，连真机；`grabFrame` 未接 SDK 前无图、饼图无数据属正常。  
**测试**（`test/` + `HMI_Test.exe` + **`test/run_test.bat`**）：`test/vision_engine.json` → `synthetic`，循环读 `test/station1/` 合成 png。

### 3.3 VisionEngine

| 模块 | 主要类 | 作用 |
|------|--------|------|
| `ipc_camera/` | `CameraIpcReader` | 读 CameraService SHM |
| `algo/` | `GasketInspector` | 模板匹配 + 径向测径 + 公差判定 |
| `ipc_hmi/` | `HmiIpcPublisher`、`HmiIpcSync`、`HmiIpcLayout` | 写 HMI 检测 SHM |
| `VisionEngine/socket/camera_engine/` | `CameraEngineSocketServer` | listen 相机套接字；解析 `ready`，环缓有空位时 `ack` |
| `VisionEngine/socket/engine_hmi/` | `EngineHmiSocketClient` | connect HMI；`hello`/`ping`；严格模式 `sendReadyAndWaitAck` 后再写 HMI SHM |
| `worker/` | `StationAlgoWorker` | 算法线程：OpenCV 模板匹配 + 径向测径 + 公差判定 |
| `worker/` | `StationCommWorker` | 通信线程：读相机 SHM、**异步**调度检测、写 HMI SHM、**onReadyTick** grant cam_ack、ping |

**线程**：**主线程** + **通信线程**（poll 读 SHM、**Queued** 派发 `inspectFrameAsync`、写 HMI 前 ready/ack）+ **算法线程**（`inspectFrameAsync` → `inspectCompleted`）。

---

## 4. 进程间通信

通信分两层，**不要混在同一目录**：

| 层次 | 目录 | 载体 | 内容 |
|------|------|------|------|
| **数据面** | 各进程 `ipc/` | 共享内存 + **命名 Mutex（读写互斥）** + **命名信号量（新帧通知）** | 原图、标注图、OK/NG、测量值 |
| **套接字层** | 各进程 `socket/` | Qt 本地套接字（UTF-8 行协议） | 连接/心跳、**ready/ack 环缓反压**（不传像素） |

`vision_engine.json` 中 **`strictSampleAccounting`**（默认 `true`）：为 `true` 时通过 **ready/ack** 在写 SHM **之前** 申请环缓空位，保证逐件写入、不覆盖环缓；为 `false` 时不握手，吞吐更高但可能漏计或覆盖中间帧。

### 4.1 CameraService → VisionEngine（数据 + 控制）

**数据 — SHM**

| 项目 | 说明 |
|------|------|
| 介质 | 共享内存 `GasketVision.Camera.Ipc.Region` |
| 缓冲 | 四槽环形缓冲 + 控制块（`frameId`、路径、相机状态） |
| 同步 | 互斥锁 `GasketVision.Camera.Ipc.Mutex` + 新帧信号量 `…FrameReady` |
| 内容 | 高清原图 |

代码：`CameraService/ipc/` 发布，`VisionEngine/ipc_camera/CameraIpcReader` 订阅。

**套接字**（`VisionEngine/socket/camera_engine/`、`CameraService/socket/camera_engine/`）

| 项目 | 说明 |
|------|------|
| 端点 | `GasketVision.Camera.Control` |
| 服务端 | **VisionEngine**（`CameraEngineSocketServer`） |
| 客户端 | **CameraService**（`CameraEngineSocketClient`） |
| Camera → Engine | `ready <frameId>`（申请写入相机环缓的空位） |
| Engine → Camera | `ack <frameId>`（环缓未满，允许采图并写 SHM） |

未收到 `ack` 时客户端 **`sendReadyAndWaitAck`**：每 **3s** 重发 `ready`、等待 `ack`，总超时 **60s**；失败则间隔 `intervalMs` 后重试。

### 4.2 VisionEngine → HMI（数据 + 控制）

**数据 — SHM**

| 项目 | 说明 |
|------|------|
| 介质 | 共享内存 `GasketVision.Ipc.Region` |
| 缓冲 | 四槽环形缓冲；每槽原图 + 标注图双平面 + 控制块 |
| 同步 | 互斥锁 `GasketVision.Ipc.Mutex` + 新帧信号量 `…FrameReady` |
| 内容 | 原图/标注图、OK/NG、缺陷、外径/内径/偏心等 |

代码：`VisionEngine/ipc_hmi/HmiIpcLayout.h`，`HmiIpcPublisher` 发布，`HMI/ipc/HmiIpcSubscriber` 订阅。

**套接字**（`HMI/socket/engine_hmi/`、`VisionEngine/socket/engine_hmi/`）

| 项目 | 说明 |
|------|------|
| 端点 | `GasketVision.EngineHmi.Control` |
| 服务端 | **HMI**（`EngineHmiSocketServer`） |
| 客户端 | **VisionEngine**（`EngineHmiSocketClient`） |
| Engine → HMI | `hello`（连接后握手）；`ping`（约 **1s** 心跳）；严格模式 `ready <frameId> <stationId> <ok>`（申请写 HMI 环缓）；非严格可选 `result …` |
| HMI → Engine | `ack <frameId>`（环缓未满，允许写 HMI SHM） |

HMI 在 **`ready`** 到达且 `frameId ≤ lastRead + 4` 时 **`ack`**；**15s** 无任何收包则断开 Engine 连接。Engine 严格模式下 **`sendReadyAndWaitAck`**（3s/次、总 60s）成功后才 `publish` HMI SHM。

### 4.3 严格模式下的单帧顺序

```
Camera:  ready → 等待 cam_ack → grabFrame → publish(相机 SHM)
Engine:  wait(相机 SHM) → inspect → ready(HMI) → 等待 hmi_ack → publish(HMI SHM)
HMI:     ready 到达 →（环缓有空位）ack → wait(HMI SHM) → record → UI 刷新
```

读相机 SHM 与算法 **异步并行**；strict 下 Engine 通过 **`onReadyTick`（10ms）** 独立 `processReadyRequests` 及时 `cam_ack`，不依赖算法完成。

### 4.4 非严格模式

仅 SHM + 信号量通知；Camera 不 connect 相机控制套接字，Engine 不 listen `Camera.Control`；Engine→HMI 仍可有 hello/ping，但不走写前 ready/ack。测试配置 `strictSampleAccounting: false`。

### 4.5 启动顺序

1. 启动 `HMI.exe` 或 `HMI_Test.exe` → `EngineHmiSocketServer` listen 成功  
2. 启动 `VisionEngine.exe` →（严格）listen `Camera.Control` → connect `EngineHmi.Control`  
3. 启动 `CameraService.exe` →（严格）connect `Camera.Control`  
4. 进入 §4.3（严格）或 §4.4（非严格）数据流  

可用 **`run.bat`**（正式）或 **`test/run_test.bat`**（测试）按上述顺序自动启动。

---

## 5. 检测流水线

```
CameraService（严格）: ready/wait ack → grabFrame → publish(相机 SHM)
CameraService（非严格）: grabFrame → publish(相机 SHM) [+ 可选队列]
VisionEngine:          wait(相机 SHM) → GasketInspector → [严格: ready/wait ack] → publish(HMI SHM)
HMI:                   [严格: ack on ready] → wait(HMI SHM) → record → UI（防抖刷新）
```

**GasketInspector**（`VisionEngine/algo/`）：OpenCV **模板匹配定位** → ROI **径向扫描**测外径/内径及中心 → 与 **公差** 比较 OK/NG → 输出 **标注图**及外径、内径、XY 偏心、匹配得分等。

---

## 6. 构建目标

| CMake 目标 | 依赖 |
|--------------|------|
| `HMI` | Qt Widgets + Network（正式，相机模式） |
| `HMI_Test` | 同上 + `GASKETVISION_HMI_TEST`（测试，合成模式） |
| `CameraService` | Qt Core + OpenCV |
| `VisionEngine` | Qt Core + Network + OpenCV |
| `Pie_Chart_Control` | Qt Widgets（独立） |

OpenCV 模块：`core`、`imgproc`、`imgcodecs`。MinGW 套件需使用 MinGW 版 OpenCV，与 exe 同目录部署 `libopencv_*.dll`。

---

## 7. 设计原则

- **一个进程一个文件夹**，进程内按职责分子目录
- **多线程职责分离**：各进程主线程挂事件循环；I/O/套接字与采图、OpenCV 检测分线程，避免 UI 与主循环被长耗时操作占用；**非**「全链路无等待」（严格反压会主动限流）
- **数据面与套接字层分离**：大图走 `ipc/` 共享内存；**ready/ack 环缓反压**、hello/ping 走各进程 `socket/`
- **正式/测试双 HMI 入口、一套源码**：编译期 profile，避免运行时切模式重启子进程
- **config/ 管正式、test/ 管联调**：CMake 只把 `config/` 复制到 `build/`；`strictSampleAccounting` 等开关写在 `vision_engine.json`
- **按通信对象拆分 IPC**：`ipc_camera/`、`ipc_hmi/`；控制实现按进程归属 `HMI/socket/`、`VisionEngine/socket/`、`CameraService/socket/`
- **协议由发布方定义**：相机 SHM 协议在 `CameraService/ipc/`，HMI SHM 在 `VisionEngine/ipc_hmi/`；控制协议头在 HMI（EngineHmi）与 VisionEngine（CameraEngine）
- **无 `common/` 目录**；`socket/` 与 `ipc/` 通过 CMake include 被各目标引用
