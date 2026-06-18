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
│   ├── socket/camera_hmi/      # CameraHmiSocketServer + 协议
│   └── ipc/                    # attach 相机 SHM、InspectionDataService
├── CameraService/
│   ├── camera/
│   ├── socket/camera_hmi/      # CameraHmiSocketClient
│   └── ipc/                    # 写相机 SHM（含原地标注后的 BGR）
├── VisionEngine/
│   ├── worker/
│   ├── algo/
│   └── ipc_camera/             # 读相机 SHM、原地标注（无套接字）
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
| `ipc/` | `CameraIpcSubscriber`、`InspectionDataService` | attach **相机 SHM**、聚合 OK/NG、刷 UI |
| `HMI/socket/camera_hmi/` | `CameraHmiSocketServer` | listen `GasketVision.CameraHmi.Control`；收 Camera `ready`、发 `ack`（含 slotIndex）；心跳超时断连 |

**线程**：主线程（事件循环 + UI）+ 1 个通信子线程（读相机 SHM 标注图 + `CameraHmiSocketServer`，同线程处理 Camera `ready` 与 `ack`）。界面**仅展示标注图**（浅拷 `ShmImageLabel`），**仅标注图落盘**。

`InspectionWindow` 通过编译宏 `GASKETVISION_HMI_TEST` 区分正式/测试：正式版固定 `resolveCameraWorkDir()`，测试版固定 `resolveTestWorkDir()`，无设置菜单切换。

### 3.2 CameraService

| 模块 | 主要类 | 作用 |
|------|--------|------|
| `camera/` | `IVisionImageSource`、`SyntheticImageSource`、`GigEVisionCamera`、`ImageSourceFactory` | 按配置采图 |
| `ipc/` | `CameraIpcPublisher`、`CameraIpcSync`、`CameraIpcLayout` | 写相机 SHM |
| `CameraService/socket/camera_hmi/` | `CameraHmiSocketClient` | connect `GasketVision.CameraHmi.Control`；严格模式 **写 SHM 前** 快/慢路径槽位 gate |

**线程**：**主线程** + **采图线程**（按间隔 `grabFrame`）+ **发布线程**（**写 SHM 前** ready/ack 或本地判定、publish）。严格模式下采图与发布 **解耦**：采图持续进行，发布队列最多 **4 帧**，满则丢新帧；环形存储区反压只阻塞 **发布写 SHM**。

**正式**（`build/` + `HMI.exe`）：`config/vision_engine.json` → `gige`，连真机；`grabFrame` 未接 SDK 前无图、饼图无数据属正常。  
**测试**（`test/` + `HMI_Test.exe` + **`test/run_test.bat`**）：`test/vision_engine.json` → `synthetic`，循环读 `test/station1/` 合成 png。

### 3.3 VisionEngine

| 模块 | 主要类 | 作用 |
|------|--------|------|
| `ipc_camera/` | `CameraIpcReader` | 浅拷读相机 SHM；`prepareAnnotTarget` + `finishInspectPublish` 原地标注 |
| `algo/` | `GasketInspector` | 模板匹配 + 径向测径 + 公差判定；`cvtColor`+画标注 **覆盖同槽** |
| `worker/` | `StationAlgoWorker` | 算法线程：OpenCV 检测 + 原地写相机槽 |
| `worker/` | `StationCommWorker` | 通信线程：poll 读相机 SHM、调度检测、`finishInspectPublish` notify（**无套接字**） |

**线程**：**主线程** + **通信线程**（poll 读 SHM、派发 `inspectDispatchAsync`、`finishInspectPublish` notify）+ **算法线程**。

---

## 4. 进程间通信

通信分两层，**不要混在同一目录**：

| 层次 | 目录 | 载体 | 内容 |
|------|------|------|------|
| **数据面** | 各进程 `ipc/` | **一块**相机共享存储区 + **命名 Mutex** + **新帧信号量** | 灰度原图→原地 BGR 标注、OK/NG、测量值 |
| **套接字层** | HMI / CameraService `socket/` | Qt 本地套接字（UTF-8 行协议） | **Camera↔HMI** 连接/心跳、**ready/ack 槽位反压**（不传像素） |

`vision_engine.json` 中 **`strictSampleAccounting`**（默认 `true`）：为 `true` 时 Camera **写 SHM 前** 通过 **快路径（本地 lastAcked）或慢路径 ready/ack** 申请存储区空槽；为 `false` 时不握手。

### 4.1 相机存储区（CameraService / VisionEngine / HMI）

| 项目 | 说明 |
|------|------|
| 介质 | 共享存储区 `GasketVision.Camera.Ipc.Region`（SHM） |
| 存储区 | 四槽环形存储区 + 控制块（`frameId`、检测元数据、`flags`：原图 Gray / 标注 BGR） |
| 同步 | 互斥锁 `GasketVision.Camera.Ipc.Mutex` + 新帧信号量 `…FrameReady` |
| 内容 | 采图 Gray8；Engine 原地 `cvtColor`+标注为 Bgr888；HMI 浅拷读标注 |

代码：`CameraService/ipc/` 发布，`VisionEngine/ipc_camera/CameraIpcReader` 消费并原地标注，`HMI/ipc/CameraIpcSubscriberWorker` attach 同区域读标注。

### 4.2 CameraService ↔ HMI（套接字，槽位反压）

**套接字**（`HMI/socket/camera_hmi/`、`CameraService/socket/camera_hmi/`）

| 项目 | 说明 |
|------|------|
| 端点 | `GasketVision.CameraHmi.Control` |
| 服务端 | **HMI**（`CameraHmiSocketServer`） |
| 客户端 | **CameraService**（`CameraHmiSocketClient`） |
| Camera → HMI | `hello`（连接后握手）；`ping`（约 **1s** 心跳）；`ready <frameId> <slotIndex>`（申请覆写该槽） |
| HMI → Camera | `ack <slotIndex> <releasedFrameId>`（该槽已展示+落盘，可覆写 Gray） |

**快路径**：Camera 本地 `lastAckedFrameId ≥ frameId - 4` 时直接写 SHM，不发 `ready`。  
**慢路径**：`sendReadyAndWaitAck`，每 **3s** 重发 `ready`，总超时 **60s**；HMI 在 `frameId ≤ lastReleased + 4` 时回 `ack`。

VisionEngine **不参与** 套接字；只 poll 读 SHM、原地标注、notify。

### 4.3 严格模式下的单帧顺序

```
Camera:  [快/慢路径] → grabFrameInto(相机 SHM, Gray) → notify
Engine:  wait(Gray) → inspect 原地 BGR+标注 → control+notify
HMI:     wait(notify) → 读相机 SHM 浅拷标注 → 落盘 annot → paint → ack(slot, frameId)
Camera:  ack 更新 lastAcked → 下一帧可覆写该槽
```

管道内整图 **深拷 1 次**：磁盘/样品 → 相机 SHM。Engine 与 HMI 均为 **SHM 浅拷视图**（无堆上 `clone`、无第二块 SHM）。

### 4.4 非严格模式

仅 SHM + 信号量通知；Camera 不 connect `CameraHmi.Control`。测试配置 `strictSampleAccounting: false`。

### 4.5 启动顺序

1. 启动 `HMI.exe` 或 `HMI_Test.exe` → `CameraHmiSocketServer` listen 成功  
2. 启动 `VisionEngine.exe` → attach 相机 SHM，poll 检测（无套接字）  
3. 启动 `CameraService.exe` →（严格）connect `CameraHmi.Control`  
4. 进入 §4.3（严格）或 §4.4（非严格）数据流  

可用 **`run.bat`**（正式）或 **`test/run_test.bat`**（测试）按上述顺序自动启动。

---

## 5. 检测流水线

```
CameraService（严格）: [快/慢路径] → grabFrame → publish(相机 SHM, Gray)
CameraService（非严格）: grabFrame → publish(相机 SHM) [+ 可选队列]
VisionEngine:          wait(相机 SHM) → 原地检测+标注 → control+notify
HMI:                   wait(相机 SHM 标注) → 仅 annot 落盘 → UI（paint → ack Camera）
```

**GasketInspector**（`VisionEngine/algo/`）：OpenCV **模板匹配定位** → ROI **径向扫描**测外径/内径及中心 → 与 **公差** 比较 OK/NG → 输出 **标注图**及外径、内径、XY 偏心、匹配得分等。

---

## 6. 构建目标

| CMake 目标 | 依赖 |
|--------------|------|
| `HMI` | Qt Widgets + Network（正式，相机模式） |
| `HMI_Test` | 同上 + `GASKETVISION_HMI_TEST`（测试，合成模式） |
| `CameraService` | Qt Core + OpenCV |
| `VisionEngine` | Qt Core + OpenCV |
| `Pie_Chart_Control` | Qt Widgets（独立） |

OpenCV 模块：`core`、`imgproc`、`imgcodecs`。MinGW 套件需使用 MinGW 版 OpenCV，与 exe 同目录部署 `libopencv_*.dll`。

---

## 7. 设计原则

- **一个进程一个文件夹**，进程内按职责分子目录
- **多线程职责分离**：各进程主线程挂事件循环；I/O/套接字与采图、OpenCV 检测分线程，避免 UI 与主循环被长耗时操作占用；**非**「全链路无等待」（严格反压会主动限流）
- **数据面与套接字层分离**：大图走 `ipc/` 共享内存；**Camera↔HMI ready/ack 槽位反压**、hello/ping 走 `socket/camera_hmi/`
- **正式/测试双 HMI 入口、一套源码**：编译期 profile，避免运行时切模式重启子进程
- **config/ 管正式、test/ 管联调**：CMake 只把 `config/` 复制到 `build/`；`strictSampleAccounting` 等开关写在 `vision_engine.json`
- **按通信对象拆分 IPC**：`ipc_camera/`（引擎侧）；HMI attach 相机 `CameraService/ipc/` 布局
- **协议由相机 SHM 定义**：`CameraIpcLayout.h`（`kVersion=4`，控制块含检测元数据）；控制协议头在 `HMI/socket/camera_hmi/CameraHmiProtocol.h`
- **无 `common/` 目录**；`socket/` 与 `ipc/` 通过 CMake include 被各目标引用
