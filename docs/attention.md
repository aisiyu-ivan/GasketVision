# 常见问题与解答

工业垫片视觉质检：**HMI** / **HMI_Test** 做人机界面（双 exe、一套源码），**CameraService** 负责采图，**VisionEngine** 做检测；**三进程独立 exe**，由脚本或 HMI 分离拉起。**`ipc/` 共享内存传图**（Mutex 互斥读写 + 信号量通知新帧）；**各进程 `socket/` 本地套接字传 ready/ack 环缓反压、hello/ping 心跳**（不传像素）。

面向 GasketVision 三进程视觉质检项目。架构细节见 [architecture.md](architecture.md)。

---

## 一、架构与进程

### Q1：为什么做成三进程，而不是一个程序？

**答：** 职责分离。**CameraService** 只管采图，**VisionEngine** 只管检测，**HMI** 只管界面与编排。好处：故障隔离（相机或算法崩溃不必拖垮界面）、依赖拆分（HMI 不链 OpenCV）、可独立升级换相机或换算法、采图/检测/显示可并行。工业现场常见「上位机 + 采集 + 算法站」的缩小版。

### Q2：三个进程分别做什么？

**答：**

- **HMI / HMI_Test**：界面、OK/NG 统计；套接字就绪后 **分离拉起** VisionEngine / CameraService（`QProcess::startDetached`，非父子进程）
- **CameraService**：合成样本或 GigE 采图，持续输出原图
- **VisionEngine**：读原图 → OpenCV 检测 → 把结果交给 HMI

### Q3：为什么先启动 HMI？

**答：** HMI 需先 **listen `GasketVision.EngineHmi.Control`**，VisionEngine 才能 connect。HMI 就绪后会 **分离启动** Engine 与 Camera（间隔约 2s）；也可只运行 HMI（Qt Creator F5）或双击 **`run.bat`** / **`test/run_test.bat`**（仅启动 HMI，其余由 HMI 拉起）。关 HMI **不会**结束已分离的 Engine/Camera。

**注意：** 正式 **HMI** 与 **HMI_Test** 共用同一套 SHM 与套接字名，**不能同时运行**。切换模式前请先关闭所有 HMI 窗口；启动时会自动结束残留的 VisionEngine / CameraService，再按当前 profile 的工作目录（`build/` 或 `test/`）拉起新链路。

### Q4：每个进程几个线程？

**答：**

| 进程 | 线程 |
|------|------|
| HMI | 2：主线程（事件循环 + UI）+ 通信线程（读检测 SHM + Engine 套接字） |
| CameraService | 3：主线程（事件循环）+ 采图线程（`grabFrame`）+ 发布线程（SHM + 严格 ready/ack） |
| VisionEngine | 3：主线程（事件循环）+ 通信线程（SHM + 套接字/ping）+ 算法线程（OpenCV 检测） |

检测算法本身未多线程并行；多线程用于 **I/O 与计算分线程、主线程/UI 不卡顿**；严格模式下 **ready/ack 反压** 会在下游慢时 **主动限流**（不是全链路无阻塞）。

---

## 二、进程通信

### Q5：进程之间传什么？怎么传？

**答：** **两层通信**（详见 [architecture.md](architecture.md) §4）：

| 层次 | 路径 | 方式 | 内容 |
|------|------|------|------|
| 数据面 | 各进程 `ipc/` | 四槽 SHM + **命名 Mutex** + **新帧信号量** | 原图、标注图、OK/NG、测量值 |
| 套接字层 | 各进程 `socket/` | Qt 本地套接字行协议 | **ready/ack 环缓反压**；Engine↔HMI 另有 **hello/ping** |

具体链路：

1. **CameraService → VisionEngine**：相机 SHM +（严格）`GasketVision.Camera.Control`（`ready`/`ack`）
2. **VisionEngine → HMI**：检测 SHM + `GasketVision.EngineHmi.Control`（`hello`/`ping`/`ready`/`ack`）

### Q6：为什么用共享内存而不是 TCP 传图？

**答：** 同机高清图体积大，TCP 需序列化/拷贝/协议开销。SHM 多进程映射同一块内存，配合环缓适合高频帧；TCP 更适合跨机或文本协议。本项目历史配置里曾有 `hmiPort:9527`，当前方案已改为 SHM。

### Q7：四槽环形缓冲是干什么的？

**答：** 写端按 `frameId % 4` 选槽写入，读端慢于写端时，旧槽尚未读完不会被新帧立刻覆盖，提供约 2～3 帧量级的抗抖动缓冲，降低丢帧概率。

### Q8：相机路为什么用互斥锁而不是读写锁？

**答：** 当前相机路与 Engine→HMI 路均为 **1 写 1 读**。用 **命名 Mutex** 保证读写 SHM 时互斥、避免脏读；用 **命名信号量** 在 publish 后 **通知** 读端有新帧（**不是**用信号量做互斥）。读写锁适合多读者同时读同一帧；本项目暂无该场景。

### Q9：为什么 SHM 和套接字要分开？

**答：** **SHM 传大图**，**套接字传控制**。像素走共享内存；**ready/ack** 协调「何时可写环缓」，**hello/ping** 维护 Engine↔HMI 连接。控制 **Server/Client 在各进程 `socket/`**，与 **`ipc/`** 分离。

- **Camera ↔ Engine**（`GasketVision.Camera.Control`）：仅 **`ready` / `ack`**（无 hello/ping）
- **Engine ↔ HMI**（`GasketVision.EngineHmi.Control`）：**`hello`** 连接握手、**`ping`** 约 1s 心跳、严格模式 **`ready` / `ack`**；可选 **`result`**（非严格）

**都不传图像像素。**

### Q9b：CameraService 和 VisionEngine 之间为什么也有套接字？

**答：** 严格模式（`strictSampleAccounting: true`）在 **`VisionEngine/socket/camera_engine/`** 与 **`CameraService/socket/camera_engine/`** 增加 **`GasketVision.Camera.Control`**，实现 **环缓反压**（数据仍只走 SHM）：

- **CameraService（client）**：发 **`ready <frameId>`** 申请空位 → **等** Engine **`ack`** → 再 **`grabFrame` + 写相机 SHM**
- **VisionEngine（server）**：解析 `ready`；当 `frameId ≤ lastRead + 4` 时回 **`ack`**（读相机 SHM 后也会再次处理 pending `ready`）

未收到 `ack` 时 **`sendReadyAndWaitAck`**：每 **3s** 重发 `ready`，总超时 **60s**。非严格模式不 listen/不握手。

### Q9c：严格模式下 Engine 与 HMI 如何握手？

**答：** 严格模式下 **写 HMI SHM 之前** 做 **ready/ack 环缓反压**（与 Camera 路对称）：

- **Engine（client）**：检测完成后发 **`ready frameId station ok`** → **`sendReadyAndWaitAck`**（3s/次、总 60s）→ 收到 **`ack`** 后 **`publish` HMI SHM**
- **HMI（server）**：收到 `ready` 且环缓未满则 **`ack`**；之后读 SHM、`record`、刷 UI

**Engine 连接后**先发 **`hello`**；运行中约 **1s** 发 **`ping`**；HMI **15s** 无收包则断开。  
**测试环境**（`strictSampleAccounting: false`）无 HMI 写前握手，连续推 SHM。

### Q9d：套接字报文 `hello` / `ping` / `ready` / `ack` 各是什么？

**答：**

| 报文 | 通道 | 方向 | 含义 |
|------|------|------|------|
| **hello** | Engine↔HMI | Engine→HMI | 连接成功后上线握手；HMI 不专门回复 |
| **ping** | Engine↔HMI | Engine→HMI | 约 1s 心跳；HMI 15s 无收包断连 |
| **ready** | 两条控制链 | 写 SHM 一侧（client）→ 监听侧（server） | **申请环缓空位**，写 SHM **之前** 发送 |
| **ack** | 两条控制链 | server→client | 环缓未满，**允许写 SHM** |
| **result** | Engine↔HMI | Engine→HMI | 非严格可选；严格主流程用 ready/ack |

Camera↔Engine **只有 ready/ack**，无 hello/ping。**都不传像素。**

### Q10：启动顺序错了会怎样？

**答：** 若 HMI 未 listen `GasketVision.EngineHmi.Control` 就启动 Engine，控制连接失败；若 VisionEngine 未 listen `GasketVision.Camera.Control` 就启动 Camera，Camera 控制客户端连接失败（严格模式）。相机 SHM 未就绪时 Engine 读相机会超时空转。正确顺序：**HMI → listen → CameraService → VisionEngine**（Engine 内先 listen 相机控制端，再 connect HMI 控制端）。

### Q10b：为什么做成 HMI.exe 和 HMI_Test.exe 两个程序？

**答：** 正式（`gige`）与测试（`synthetic`）共用同一套检测链路，但配置与工作目录不同。原先在一个 HMI 里运行时切换模式需要重启子进程、重置 IPC，易卡死且结果易混淆。现改为**编译期 profile**：两个 exe、两个启动脚本，无 UI 切换；CameraService / VisionEngine 仍各一份，由对应 HMI 按工作目录传 `vision_engine.json` 路径。

---

## 三、相机与样品

### Q11：没有真机时怎么调试？

**答：** 双击 **`test/run_test.bat`**（生成样品并启动 HMI_Test，Engine/Camera 由 HMI 分离拉起）。测试配置 `test/vision_engine.json` 中 **`strictSampleAccounting: false`**，合成样品约每 800ms 连续播放，无需 Camera 控制套接字握手。正式环境 `build/vision_engine.json` 保持 **`strictSampleAccounting: true`**。 **HMI.exe 与 HMI_Test.exe 不要同时运行。**

### Q12：样品图是 HMI 读还是 Engine 读？

**答：** **都不是直接读。** 样品由 **CameraService** 的 `SyntheticImageSource` 用 OpenCV `imread` 读取，经 **CameraService → VisionEngine** 的 SHM 交给引擎。HMI 只看到检测后的结果 SHM（或路径回退）。

### Q13：GigE 相机实现了吗？

**答：** **骨架已有**（`GigEVisionCamera`），`grabFrame` 未接完整 SDK。**正式部署**用 `config/vision_engine.json`（`gige`）；**合成联调**用 `test/vision_engine.json`（`synthetic`）。IPC 不变。

---

## 四、检测算法

### Q14：检测流程是什么？

**答：** OpenCV **模板匹配定位** → ROI **径向扫描**测外径/内径及中心 → 与配置 **公差** 比较 OK/NG → 输出 **标注图**及外径、内径、XY 偏心、匹配得分。匹配分低于阈值判 **缺件**。不是霍夫圆或深度学习。

### Q15：为什么用径向扫描而不是霍夫圆？

**答：** 垫片是圆环，在已知 ROI 内沿半径方向找灰度跳变更直接、可控；配合模板匹配先定位，对轻微偏移更稳。答辩时可说「按项目精度与实时性选的工程方案」。

### Q16：标注图存在哪？

**答：** 与原图同目录，文件名 `原路径.annotated.png`。同时通过 HMI SHM 传标注图像素；若 SHM 为空，HMI 可按路径读文件回退。

---

## 五、HMI 与线程

### Q17：Subscriber 和 DataService 有什么区别？

**答：** **HmiIpcSubscriber**（通信子线程）：读检测 SHM、`EngineHmiSocketServer` listen；严格模式下在 **`ready`** 时 **`ack`**（环缓反压），读 SHM 后 **`emit frameReceived`**。**InspectionDataService**（主线程）：收 `frameReceived`、聚合 OK/NG、刷 UI。

### Q18：为什么子线程读 SHM 还要用信号槽？

**答：** Qt 规定 **UI 只能在主线程**更新。子线程 `emit frameReceived`，队列连接到主线程 `onFrame`，避免跨线程直接改 `QLabel` 崩溃或花屏。

---

## 六、构建与部署

### Q19：config/ 和根目录 .bat 是什么关系？

**答：** **`config/`** 是正式环境配置的**源码**（`vision_engine.json`、饼图 JSON、模板）。**`.bat` 不直接读 config**，而是触发 CMake 编译；编译后 CMake 把 `config/` **复制到 `build/`**，运行时程序读的是 `build/` 里的副本。

| 脚本 | 作用 |
|------|------|
| `run.bat` | 正式：编译 + 启动 HMI（Engine/Camera 自动分离拉起） |
| `test/run_test.bat` | 测试：生成样品 + 启动 HMI_Test |

测试配置源码为 **`config/vision_engine_test.json`**；`test/run_test.bat` 每次启动会将其复制到 **`test/vision_engine.json`**（合成样品 + `strictSampleAccounting: false`）。正式配置源码为 **`config/vision_engine.json`**，编译后复制到 **`build/vision_engine.json`**。常用字段：`strictSampleAccounting`（是否严格逐件 ack）、`intervalMs` / `grabIntervalMs`（采图间隔）。改正式参数改 `config/vision_engine.json` 后需重编；改测试参数改 `config/vision_engine_test.json` 后重新运行 `test/run_test.bat` 即可。

### Q20：OpenCV 为什么 HMI 不用？

**答：** HMI 只显示 `QImage` 和统计，图像已在 Engine 侧处理好。不链 OpenCV 可减小依赖、加快启动、降低部署复杂度。

### Q21：MinGW 和 MSVC 的 OpenCV 能混用吗？

**答：** **不能。** 须与 Qt 套件工具链一致。MinGW 工程用 MinGW 编译的 OpenCV；运行时将 `libopencv_*.dll` 放到 `CameraService.exe`、`VisionEngine.exe` 同目录。

### Q22：部署要拷哪些 exe？

**答：** 项目根目录双击 **`run.bat`**（正式）；测试目录双击 **`test/run_test.bat`**。首次会自动 CMake。**HMI.exe 与 HMI_Test.exe 不要同时运行。**

---

## 七、局限与扩展（主动说明加分）

### Q23：当前已知局限？

**答：**

- GigE 未接完整 SDK
- SHM 限同机；跨机需改网络方案
- `strictSampleAccounting: false` 时仍无法严格保证每件样品进统计
- `HMI.exe` 与 `HMI_Test.exe` 不可同时运行（IPC 名冲突）
- 主路径以 **stationId = 1** 为主

### Q23b：如何保证每个样品都进 OK/NG 统计？

**答：** 配置 `strictSampleAccounting: true`（正式默认）。**数据**走 `ipc/`；**写 SHM 前**走 **ready/ack**：

1. **Camera → Engine**：Camera **`ready`** → Engine **`ack`** → Camera 采图并写相机 SHM  
2. **Engine → HMI**：Engine **`ready`** → HMI **`ack`** → Engine 写 HMI SHM  
3. Engine 读完相机 SHM 后可提前 **`cam_ack`**，使采图与检测部分重叠  

`sendReadyAndWaitAck` 未收到应答时每 **3s** 重发 `ready`，总 **60s**。关闭严格模式则仅 SHM 推送，无协议保证。

### Q24：如何扩展多工位？

**答：** 架构上已按进程拆分：可加多个 CameraService 实例或扩展 SHM 协议工位字段；Engine 侧 `StationCommWorker` 按 `stationId` 发布 HMI SHM；HMI `InspectionAggregator` 按工位计数。环缓与分进程便于水平扩展。

---

## 八、阻塞与死锁预防

本节汇总 **GasketVision 全链路** 中可能卡住 UI、停住采图、或进程间互相等待的场景，以及 **设计约束与运维习惯**。架构细节见 [architecture.md](architecture.md) §4。

### Q25：本项目里「阻塞」和「死锁」分别指什么？

**答：**

| 概念 | 在本项目中的典型表现 |
|------|----------------------|
| **阻塞** | 某线程在 `waitFor*`、`waitAck`、`waitFrameReady`、`BlockingQueuedConnection`、SHM `lock` 上长时间不返回；界面无响应、样品不再刷新、Camera 停在「等 ack」 |
| **死锁** | 两个及以上线程/进程 **互相等待** 对方释放资源，且无超时退出；例如 A 等 B 的 ack、B 等 A 的 UI 处理完 |

本项目 **刻意避免 UI 线程参与 Blocking 等待**；严格模式下 **Camera / Engine 通信线程** 会在 **ready/ack** 与 **算法** 上阻塞，属 **环缓反压与逐件限流**，与「多线程防 UI 卡顿」并存而非矛盾。

### Q26：进程与 IPC 层面如何预防卡住？

**答：**

1. **不要同时运行 `HMI.exe` 与 `HMI_Test.exe`**  
   二者共用 `GasketVision.*` SHM 名与本地套接字名。双开会导致 attach 失败、读到旧帧、或一方 detach 另一方仍写，表现为「只出一件」「界面假死」「等待 VisionEngine…」循环。

2. **切换正式/测试前先关干净**  
   启动脚本与 `InspectionWindow::stopBackendProcesses()` 会 `taskkill` 残留的 `VisionEngine.exe` / `CameraService.exe`；若手动调试过，任务管理器里确认 **四个 exe 都已结束** 再启动。

3. **工作目录与配置必须成对**  
   - 正式：`build/` + `build/vision_engine.json`（`gige`，`strictSampleAccounting: true`）  
   - 测试：`test/` + `test/vision_engine.json`（`synthetic`，`strictSampleAccounting: false`）  
   **`test/run_test.bat` 必须从 `config/vision_engine_test.json` 复制测试配置**；若误用正式配置，Camera 会走 GigE + 严格 ack，测试环境极易 **只出一帧或长时间无图**。

4. **启动顺序**（见 Q10）  
   HMI listen →（约 2s 后）CameraService → VisionEngine 已先起。严格模式下 Engine 须 **先** listen `GasketVision.Camera.Control`，Camera 才能 `connect` 并 `sendReady`。

5. **SHM 初始化只做一次 attach/detach**  
   `HmiIpcPublisher::initialize()` 等对残留段 **单次** detach，**禁止** `while (attach) detach` 空转（历史 bug 曾导致 CPU 100% 与假死）。

6. **HMI 切换链路时 reset IPC**  
   `InspectionDataService::resetIpcConnection()` 仅置位标志，由 IPC **子线程** 执行 detach，避免主线程与子线程同时操作 `QSharedMemory`。

### Q27：严格模式（`strictSampleAccounting: true`）下如何预防采图链阻塞？

**答：** 严格模式形成 **ready → ack → 写 SHM** 的反压（数据走 SHM，确认走套接字）：

```
Camera:  sendReadyAndWaitAck → grabFrame → publish(相机 SHM) → 间隔 → 下一 ready
Engine:  processReady(可 cam_ack) → waitAndRead → inspect → sendReadyAndWaitAck(HMI) → publish(HMI SHM)
HMI:     on ready → ack（环缓未满）→ read SHM → UI
```

**预防要点：**

| 风险 | 后果 | 预防 |
|------|------|------|
| Engine 未 listen 相机套接字 | Camera `waitAck` 失败 | 正式配置 `strictSampleAccounting: true`；Engine 先 `startComm` |
| 长时间收不到 ack | 采图/写 HMI 暂停 | **`sendReadyAndWaitAck`** 每 3s 重发 `ready`，总 60s；失败后再 `intervalMs` 重试 |
| Engine 检测过慢 | `cam_ack` 延迟 | 优化算法；读 SHM 后 `processReadyRequests` 可提前 ack 下一采图 |
| Engine 等 HMI ack 时通信线程占用 | 本轮无法处理新 `ready` | 环缓允许多帧在途；HMI 读 SHM 后释放空位 |
| 算法 `inspectFrame` 永久阻塞 | 整条链停 | **当前无算法超时**；现场需 taskkill 或后续加看门狗 |

**测试联调**用 `strictSampleAccounting: false`，无 ready/ack，按 `intervalMs` 连续推帧。

### Q28：Qt 线程与信号槽如何避免死锁？

**答：**

| 位置 | 做法 | 禁止 |
|------|------|------|
| **HMI 主线程** | UI 更新、`onFrame`、`InspectionAggregator` | 主线程 `BlockingQueuedConnection` 等通信线程；主线程 `waitFor*` |
| **HMI 通信线程** | `HmiIpcSubscriberWorker` 读 SHM + `EngineHmiSocketServer` listen/ack（同线程） | 直接操作 `QLabel` / `QWidget` |
| **frameReceived → UI** | 通信线程 `emit`；主线程 Queued `onFrame` | 主线程读 SHM |
| **VisionEngine 通信线程** | `QTimer` 轮询 `onPollFrame` | 在 `onPollFrame` 内 **BlockingQueued** 等算法 + **sendReadyAndWaitAck(HMI)** |
| **VisionEngine 算法线程** | 仅 `inspectFrame` | 算法线程访问套接字 |
| **CameraService 采图线程** | `grabFrame`；严格模式 **`BlockingQueued` 调 `requestCaptureSlot`** 后再采 | 在采图线程直接 `waitAck` |
| **CameraService 发布线程** | SHM `publish`、`sendReadyAndWaitAck` | — |

**原则：** UI 线程不参与 Blocking 等待；严格反压下 **会阻塞** 采图/发布/通信线程，属 **故意限流**，不是 UI 卡顿防护失败。

### Q29：SHM 互斥锁与 notify/read 竞态如何预防？

**答：** 相机路与 HMI 路均为 **1 写 1 读 + 互斥锁 + 新帧信号量**。

1. **持锁时间尽量短**  
   锁内只做拷贝/校验；OpenCV 检测、大图 `QImage::copy` 在锁外完成（Engine 算法线程、Publisher 写槽布局）。

2. **所有分支必须 unlock**  
   `readFrame` / `publish` 每个 `return false` 路径都要 `unlock`，否则对端 `lock` 超时（默认 5000ms）后丢帧或假死。

3. **notify 先于 payload 可见的竞态**  
   写端：`lock` → 写槽与 control → `unlock` → `notifyFrameReady`。  
   读端：`waitFrameReady` 返回后若 `readFrame` 失败（`frameId` 未变、槽头不一致），**短窗口内重试**（Engine `waitAndRead` 循环、HMI Subscriber 100ms 重试），避免 **白消费信号量** 导致永久丢帧。

4. **frameId 去重**  
   `m_lastFrameId` 防止重复处理；勿在 reset 后忘记清零，否则新会话首帧可能被拒。

5. **读端 attach 失败**  
   HMI Subscriber 200ms 退避重试 attach，**不**在主线程 spin；Engine `CameraIpcReader` lazy attach，Camera 未起时 poll 超时空转而非崩溃。

### Q30：套接字层哪些调用会阻塞？如何预防？

**答：**

| 调用 | 位置 | 超时 | 预防 |
|------|------|------|------|
| `waitForConnected` | Engine→HMI Client | 可配置 | HMI 须先 listen；Engine `onPingTimer` 断线重连 |
| `waitForBytesWritten(3000)` | 各端写行 | 3s | 对端进程存在且 event loop 运行；避免在已断开 socket 上写 |
| `waitAck` / `sendReadyAndWaitAck` | Camera→Engine、Engine→HMI（严格） | 单次 3s，总 60s，超时 **重发 ready** | Engine/HMI 须 listen；环缓满时暂不回 ack |
| `hello` / `ping` | Engine→HMI | — / 约 1s | 连接与保活；HMI 15s 无包断连 |
| 心跳超时 15s | HMI Server | 断开 client | Engine `onPingTimer` 重连并再发 hello |

**本地套接字全在同一台机器**；不要用阻塞读占满 Camera 单线程 event loop——Camera 主循环在 `waitAck` 内分段 `waitForReadyRead(200)`，仍可能被 strict 模式长时间占满，故测试环境关闭 strict。

### Q31：VisionEngine 三线程分工如何防止通信与算法互相拖死？

**答：**

```
主线程：     exec() 事件循环
通信线程：   poll → processReady → waitAndRead → [BlockingQueued] inspectFrame
             → sendReadyAndWaitAck(HMI) → publish(HMI SHM)
算法线程：   inspectFrame（OpenCV，无套接字/SHM 写）
```

- **唯一跨线程同步点**：通信 → 算法的 `BlockingQueuedConnection`。
- **严格模式**：通信线程在 **HMI ready/ack** 与 **算法** 上均可能长时间阻塞；算法线程本身不阻塞在 I/O 上。

### Q32：HMI 界面「假死」或长时间无刷新如何预防？

**答：**

1. **子线程读 SHM，主线程刷 UI**（Q17、Q18）；`frameReceived` → `onFrame` 使用 **QueuedConnection**（默认），不在 IPC 线程里 `applyInspectionResult`。
2. **不在主线程做文件对话框/同步 IO 阻塞帧路径**；加载样品图等用户操作与 IPC 路径分离。
3. **通信单线程**：读 SHM 与套接字同在通信线程；严格模式下 **`ready` 时 `ack`**（写许可），`readFrame` 后 **`emit`** 到主线程刷 UI。
4. **防抖**：`flushTimer` 50ms 合并刷新，避免每帧多次重绘 pie/大图。
5. **若整窗无响应**：先查是否 **双 HMI**、**残留 Engine/Camera**、或 **误用正式配置跑测试**（见 Q26），再查任务管理器 CPU 是否 100%（SHM attach 死循环）。

### Q33：构建、部署与运维上如何避免「Permission denied」与链路半启动？

**答：**

| 现象 | 原因 | 预防 |
|------|------|------|
| 链接 `Permission denied` | exe 仍在运行 | 编译前 `taskkill` 四个进程 |
| Engine/Camera 起不来 | 工作目录无 `vision_engine.json` | 正式从 `build/` 启动 HMI；测试从 `test/` 启动 HMI_Test |
| 只编译了 HMI 未编 Engine | 旧 Engine 行为与 HMI 不匹配 | `run.bat` / `run_test.bat` 会编齐目标 |
| 关 HMI 后 Engine/Camera 仍在 | 分离启动设计 | 切换模式前手动结束或重新开 HMI（会 kill 后再拉） |

### Q34：现场快速排查清单（按顺序）

**答：**

1. 任务管理器：是否 **仅一个 HMI** + 至多各一个 VisionEngine / CameraService？  
2. `test/vision_engine.json`：`imageSource` 是否为 `synthetic`，`strictSampleAccounting` 是否为 `false`（测试）？  
3. HMI 状态栏：是否出现「本地套接字已就绪」「VisionEngine 已连接」？  
4. 严格正式模式：无新样品 → 查 Camera 是否在 **`sendReadyAndWaitAck`**、Engine 是否在 **等 HMI ack** 或 **算法阻塞**。
5. 测试模式仍只出一帧：确认 `test/vision_engine.json` 为 **`synthetic` + `strictSampleAccounting: false`**（`run_test.bat` 应从 `config/vision_engine_test.json` 复制）。
6. 仍异常：`taskkill /F /IM HMI.exe /IM HMI_Test.exe /IM VisionEngine.exe /IM CameraService.exe` 后冷启动。

**设计总纲（改代码时自检）：** 数据面（SHM + Mutex + 信号量）；控制面 **ready/ack 在写 SHM 前**、`sendReadyAndWaitAck` 带重试；UI 线程不参与 Blocking；严格模式 **允许业务线程反压等待**，与「UI 不卡」目标分离。
