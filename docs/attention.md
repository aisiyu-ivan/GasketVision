# 常见问题与解答

工业垫片视觉质检：**HMI** / **HMI_Test** 做人机界面（双 exe、一套源码），**CameraService** 负责采图，**VisionEngine** 做检测；**三进程独立 exe**，由脚本或 HMI 分离拉起。**`ipc/` 共享内存传图**（Mutex 互斥读写 + 信号量通知新帧）；**Camera↔HMI `socket/camera_hmi/` 本地套接字传 ready/ack、hello/ping 心跳**（不传像素）。

面向 GasketVision 三进程视觉质检项目。架构细节见 [architecture.md](architecture.md)。

---

## 一、架构与进程

### Q1：为什么做成三进程，而不是一个程序？

**答：** 职责分离。**CameraService** 只管采图，**VisionEngine** 只管检测，**HMI** 只管界面与编排。好处：故障隔离（相机或算法崩溃不必拖垮界面）、依赖拆分（HMI 不链 OpenCV）、可独立升级换相机或换算法、采图/检测/显示可并行。工业现场常见「上位机 + 采集 + 算法站」的缩小版。

### Q2：三个进程分别做什么？

**答：**

- **HMI / HMI_Test**：界面、OK/NG 统计；套接字就绪后 **分离拉起** VisionEngine / CameraService（`QProcess::startDetached`，非父子进程）
- **CameraService**：合成样本或 GigE 采图，**直写相机 SHM**（严格模式在发布线程完成 ack→采图→notify）
- **VisionEngine**：浅拷读相机 SHM → 原地检测+标注 → 写 control+notify（**无套接字**）

### Q3：为什么先启动 HMI？

**答：** HMI 需先 **listen `GasketVision.CameraHmi.Control`**，CameraService（严格模式）才能 connect。HMI 就绪后会 **分离启动** Engine 与 Camera（间隔约 2s）；也可只运行 HMI（Qt Creator F5）或双击 **`run.bat`** / **`test/run_test.bat`**（仅启动 HMI，其余由 HMI 拉起）。关 HMI **不会**结束已分离的 Engine/Camera。

**注意：** 正式 **HMI** 与 **HMI_Test** 共用同一套 SHM 与套接字名，**不能同时运行**。切换模式前请先关闭所有 HMI 窗口；启动时会自动结束残留的 VisionEngine / CameraService，再按当前 profile 的工作目录（`build/` 或 `test/`）拉起新链路。

### Q4：每个进程几个线程？

**答：**

| 进程 | 线程 |
|------|------|
| HMI | 2：主线程（事件循环 + UI）+ 通信线程（读相机 SHM 标注 + Camera 套接字） |
| CameraService | 3：主线程（事件循环）+ 采图线程（持续 `grabFrame`）+ 发布线程（写 SHM 前槽位 gate） |
| VisionEngine | 3：主线程（事件循环）+ 通信线程（SHM poll/调度）+ 算法线程（OpenCV 检测） |

检测算法本身未多线程并行；多线程用于 **I/O 与计算分线程、主线程/UI 不卡顿**；严格模式下 **ready/ack 反压** 会在下游慢时 **主动限流**（不是全链路无阻塞）。

---

## 二、进程通信

### Q5：进程之间传什么？怎么传？

**答：** **两层通信**（详见 [architecture.md](architecture.md) §4）：

| 层次 | 路径 | 方式 | 内容 |
|------|------|------|------|
| 数据面 | 各进程 `ipc/` | **一块**四槽相机存储区（SHM）+ **命名 Mutex** + **新帧信号量** | Gray→原地 BGR 标注、OK/NG、测量值 |
| 套接字层 | HMI / CameraService `socket/` | Qt 本地套接字行协议 | **Camera↔HMI ready/ack 槽位反压**；hello/ping |

具体链路：

1. **CameraService ↔ HMI**：`GasketVision.CameraHmi.Control`（`hello`/`ping`/`ready`/`ack`）
2. **CameraService / VisionEngine / HMI**：**同一块相机存储区**（Gray→原地 BGR 标注）

HMI attach **相机 SHM** 浅拷展示标注；**paint+落盘后** 向 Camera 发 **`ack slot releasedFrameId`** 释放槽位。HMI **仅** `captures/{frameId}_annot.png` 落盘。

### Q6：为什么用共享内存而不是 TCP 传图？

**答：** 同机高清图体积大，TCP 需序列化/拷贝/协议开销。SHM 多进程映射同一块内存，配合环形存储区适合高频帧；TCP 更适合跨机或文本协议。本项目历史配置里曾有 `hmiPort:9527`，当前方案已改为 SHM。

### Q7：四槽环形存储区是干什么的？

**答：** 写端按 `frameId % 4` 选槽写入，读端慢于写端时，旧槽尚未读完不会被新帧立刻覆盖，提供约 2～3 帧量级的抗抖动余量，降低丢帧概率。

### Q8：相机路为什么用互斥锁而不是读写锁？

**答：** 当前相机路与 Engine→HMI 路均为 **1 写 1 读**。用 **命名 Mutex** 保证读写 SHM 时互斥、避免脏读；用 **命名信号量** 在 publish 后 **通知** 读端有新帧（**不是**用信号量做互斥）。读写锁适合多读者同时读同一帧；本项目暂无该场景。

### Q9：为什么 SHM 和套接字要分开？

**答：** **SHM 传大图**，**套接字传控制**。像素走共享内存；**Camera↔HMI ready/ack** 协调「何时可覆写存储区槽」，`hello`/`ping` 维护 Camera 连接。控制 **Server/Client 在 `socket/camera_hmi/`**，与 **`ipc/`** 分离。

- **Camera ↔ HMI**（`GasketVision.CameraHmi.Control`）：**`hello`** 连接握手、**`ping`** 约 1s 心跳、严格模式 **`ready <frameId> <slotIndex>` / `ack <slotIndex> <releasedFrameId>`**

**VisionEngine 无套接字**，只读 SHM 做检测。**都不传图像像素。**

### Q9b：CameraService 和 HMI 之间为什么有套接字？

**答：** 严格模式（`strictSampleAccounting: true`）在 **`HMI/socket/camera_hmi/`** 与 **`CameraService/socket/camera_hmi/`** 增加 **`GasketVision.CameraHmi.Control`**，实现 **存储区槽位反压**（数据仍只走 SHM）：

- **CameraService（client，发布线程）**：**写相机 SHM 前** 快路径（本地 `lastAcked`）或慢路径 **`ready slot`** → **等 HMI `ack`** → 再 **`captureFromSource`**
- **CameraService（采图线程）**：按 **`intervalMs` 持续 `grabFrame`**，经信号槽交给发布队列（队列满则丢弃新帧）
- **HMI（server）**：收到 `ready` 且 `frameId ≤ lastReleased + 4` 时 **`ack`**；展示+落盘后也主动 **`ack`** 释放槽

未收到 `ack` 时 **`sendReadyAndWaitAck`**：每 **3s** 重发 `ready`，总超时 **60s**。非严格模式不 connect。

### Q9c：VisionEngine 还需要握手吗？

**答：** **不需要。** Engine 只 poll 读相机 SHM、原地标注、notify；槽位释放由 **HMI → Camera `ack`** 完成，Engine 不参与套接字。

**测试环境**（`strictSampleAccounting: false`）Camera 不 connect HMI，连续推 SHM。

### Q9d：套接字报文 `hello` / `ping` / `ready` / `ack` 各是什么？

**答：**

| 报文 | 通道 | 方向 | 含义 |
|------|------|------|------|
| **hello** | Camera↔HMI | Camera→HMI | 连接成功后上线握手；HMI 不专门回复 |
| **ping** | Camera↔HMI | Camera→HMI | 约 1s 心跳；HMI 15s 无收包断连 |
| **ready** | Camera↔HMI | Camera→HMI | **申请覆写 slotIndex 槽**，写 Gray **之前** 发送 |
| **ack** | Camera↔HMI | HMI→Camera | 该槽已展示+落盘，**允许覆写**；带 `releasedFrameId` |

**无 `done` 报文**（旧 Engine↔HMI / Camera↔Engine 链已移除）。**都不传像素。**

### Q10：启动顺序错了会怎样？

**答：** 若 HMI 未 listen `GasketVision.CameraHmi.Control` 就启动 Camera（严格模式），Camera 控制连接失败。相机 SHM 未就绪时 Engine 读相机会超时空转。VisionEngine **无套接字**，可独立启动。正确顺序：**HMI → listen → VisionEngine / CameraService**（Camera 严格模式需 HMI 先 listen）。

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

**答：** `captures/{frameId}_annot.png`（仅标注图）。标注像素在相机 SHM 同槽；HMI 浅拷 `QImage` 视图落盘。

---

## 五、HMI 与线程

### Q17：Subscriber 和 DataService 有什么区别？

**答：** **CameraIpcSubscriber**（通信子线程）：attach 相机 SHM 读标注、`CameraHmiSocketServer` listen；严格模式下处理 Camera **`ready`** 并 **`ack`**（槽位反压），读 SHM 后 **`emit frameReceived`**；展示+落盘后 **`ack`** 释放槽。**InspectionDataService**（主线程）：收 `frameReceived`、聚合 OK/NG、刷 UI。

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

1. **Camera → Engine**：发布线程 **`ready`** → Engine **`ack`** → **publish** 相机 SHM（采图线程持续 grab，与 ack 解耦）
2. **Engine → HMI**：Engine **`ready`**（写标注前）→ HMI **`ack`** → Engine 原地标注 + notify → HMI 读相机 SHM 标注  
3. **HMI → Engine → Camera**：paint + annot 落盘 → **`done(hmi)`** → Engine 转发 **`done(cam)`**
3. Engine 读完相机 SHM 后可提前 **`cam_ack`**，使采图与检测部分重叠  

`sendReadyAndWaitAck` 未收到应答时每 **3s** 重发 `ready`，总 **60s**。关闭严格模式则仅 SHM 推送，无协议保证。

### Q24：如何扩展多工位？

**答：** 架构上已按进程拆分：可加多个 CameraService 实例或扩展 SHM 协议工位字段；`CameraIpc::ControlBlock` 含 `stationId`；HMI `InspectionAggregator` 按工位计数。环形存储区与分进程便于水平扩展。

---

## 八、阻塞与死锁预防

本节汇总 **GasketVision 全链路** 中可能卡住 UI、停住采图、或进程间互相等待的场景，以及 **设计约束与运维习惯**。架构细节见 [architecture.md](architecture.md) §4。

### Q25：本项目里「阻塞」和「死锁」分别指什么？

**答：**

| 概念 | 在本项目中的典型表现 |
|------|----------------------|
| **阻塞** | 某线程在 `waitFor*`、`waitAck`、`waitFrameReady`、`BlockingQueuedConnection`、SHM `lock` 上长时间不返回；界面无响应、样品不再刷新、Camera 停在「等 ack」 |
| **死锁** | 两个及以上线程/进程 **互相等待** 对方释放资源，且无超时退出；例如 A 等 B 的 ack、B 等 A 的 UI 处理完 |

本项目 **刻意避免 UI 线程参与 Blocking 等待**；严格模式下 **Camera 写 SHM 前**、**Engine 标注后通知 HMI 前** 会在 ready/ack 上等待，属 **环形存储区/展示反压**；采图与算法检测与之 **解耦**。

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
   HMI listen `CameraHmi.Control` →（约 2s 后）VisionEngine / CameraService。严格模式下 Camera 须 **connect HMI** 后才能 `sendReady`。

5. **SHM 初始化只做一次 attach/detach**  
   `CameraIpcPublisher::initialize()` / `CameraIpcReader::initialize()` 等对残留段 **单次** detach，**禁止** `while (attach) detach` 空转（历史 bug 曾导致 CPU 100% 与假死）。

6. **HMI 切换链路时 reset IPC**  
   `InspectionDataService::resetIpcConnection()` 仅置位标志，由 IPC **子线程** 执行 detach，避免主线程与子线程同时操作 `QSharedMemory`。

### Q27：严格模式（`strictSampleAccounting: true`）下如何预防采图链阻塞？

**答：** 严格模式形成 **Camera ready/ack → 写 Gray SHM** 的反压（数据走 SHM，槽位确认走 Camera↔HMI 套接字）：

```
Camera:  grab(采图线程，持续) → frameGrabbed → 发布队列
         发布线程: [快/慢路径] → captureFromSource(相机 SHM, Gray)
Engine:  读相机 SHM(浅拷) → inspectDispatchAsync(原地标注) → finishInspectPublish(notify)
HMI:     读相机 SHM 标注 → annot 落盘 → paint → ack(slot, frameId) → Camera
HMI:     on Camera ready → ack（槽位未满时）
```

**预防要点：**

| 风险 | 后果 | 预防 |
|------|------|------|
| HMI 未 listen CameraHmi | Camera `waitAck` 失败 | 正式配置 `strictSampleAccounting: true`；HMI 先 `startComm` |
| 长时间收不到 ack | 发布写 SHM 暂停 | **`sendReadyAndWaitAck`** 每 3s 重发 `ready`，总 60s；失败后再 `intervalMs` 重试 |
| Engine 检测过慢 | HMI 展示滞后、Camera 反压 | 采图可继续；Engine 待检队列最多 4 帧 |
| HMI 展示过慢 | Camera 槽位 gate 阻塞 | paint 完成后 **`ack`** 释放槽；pending ready 自动重试 ack |
| 算法 `inspectFrame` 永久阻塞 | 整条链停 | **当前无算法超时**；现场需 taskkill 或后续加看门狗 |

**测试联调**用 `strictSampleAccounting: false`，无 ready/ack，按 `intervalMs` 连续推帧。

### Q28：Qt 线程与信号槽如何避免死锁？

**答：**

| 位置 | 做法 | 禁止 |
|------|------|------|
| **HMI 主线程** | UI 更新、`onFrame`、`InspectionAggregator` | 主线程 `BlockingQueuedConnection` 等通信线程；主线程 `waitFor*` |
| **HMI 通信线程** | `CameraIpcSubscriberWorker` 读相机 SHM + `CameraHmiSocketServer` listen/ack（同线程） | 直接操作 `QLabel` / `QWidget` |
| **frameReceived → UI** | 通信线程 `emit`；主线程 Queued `onFrame` | 主线程读 SHM |
| **VisionEngine 通信线程** | `QTimer` 轮询读 SHM、派发检测 | 不在 `onPollFrame` 内 Blocking 等算法 |
| **VisionEngine 算法线程** | `inspectDispatchAsync` | 完成后 **finishInspectPublish + notify** |
| **CameraService 采图线程** | 按间隔 **`grabFrame`** | **不**调用 `requestCaptureSlot` |
| **CameraService 发布线程** | **`requestCaptureSlot` 后 publish** | 存储区槽满时在套接字上等 ack，采图不阻塞 |

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
| `waitAck` / `sendReadyAndWaitAck` | Camera↔HMI（严格） | 单次 3s，总 60s，超时 **重发 ready** | HMI 须 listen；存储区槽满时暂不回 ack |
| `hello` / `ping` | Engine→HMI | — / 约 1s | 连接与保活；HMI 15s 无包断连 |
| 心跳超时 15s | HMI Server | 断开 client | Engine `onPingTimer` 重连并再发 hello |

**本地套接字全在同一台机器**；不要用阻塞读占满 Camera 单线程 event loop——Camera 主循环在 `waitAck` 内分段 `waitForReadyRead(200)`，仍可能被 strict 模式长时间占满，故测试环境关闭 strict。

### Q31：VisionEngine 三线程分工如何防止通信与算法互相拖死？

**答：**

```
主线程：     exec() 事件循环
通信线程：   poll 读相机 SHM → 待检队列 → Queued inspectDispatchAsync
             onReadyTick → takeDone(HMI) → releaseFrame(Camera) + processReadyRequests
             tryDispatchInspect → sendReadyAndWaitAck(HMI) → inspectDispatchAsync
             onInspectCompleted → finishInspectPublish(notify)
算法线程：   inspectDispatchAsync → emit inspectCompleted
```

- **通信 ↔ 算法**：`QueuedConnection` + `inspectCompleted` 回调，**不 Blocking 占满通信线程**。
- **严格模式**：HMI **ready/ack** 在 **Engine 写标注前**；Camera **ready/ack** 在 **发布线程写相机 SHM 前**；Camera **`done`** 在 **HMI `done` 经 Engine 转发后**。

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

---



---

## 九、图像核心信息与合成样品（我对图像的理解）

图像的全部核心信息可客观梳理为 **三个板块**：内存与几何结构（算法直接操作的对象）、光学与成像特征（质量与预处理依据）、文件及物理元数据（读取、解析与尺寸还原）。以下先给出通用框架，再说明本项目合成样品如何对应。

### 9.1 内存与几何结构信息（算法处理的直接对象）

这类信息决定图像在计算机内存中的矩阵形态、边界与寻址方式，是编写图像处理代码时最核心的参数。

**分辨率（Dimensions）**

- 图像的 **宽度（Width）** 与 **高度（Height）**，单位为 **像素（Pixel）**。
- 在矩阵中通常：**行数（Rows）= 高度**，**列数（Cols）= 宽度**。

**通道数（Channels）**

- 单个像素包含的数值分量个数。
- 灰度图为 **1 通道**；RGB 工业相机采集的彩色图为 **3 通道**；带透明度（Alpha）的图像为 **4 通道**。

**数据类型与位深度（Data Type & Depth）**

- 每个像素中每个通道占用的二进制位数及内存表示形式。
- 最常用：**8-bit unsigned char**（取值 0～255）。
- 工业高精度测量中常用：**16-bit unsigned short**（0～65535），或 **32-bit float**（0.0～1.0）。

**步长（Stride / Step）**

- 图像内存中 **每一行像素实际占用的字节数（Bytes）**。
- 为实现内存对齐（通常对齐到 4 或 8 字节）以提升 CPU/GPU 读取效率，步长往往 **≥ 宽度 × 通道数 ×（位深度 / 8）**。
- 直接进行指针寻址操作像素时，**必须使用步长** 计算偏移量。

### 9.2 光学与成像特征信息（决定图像质量与特征）

这类信息由相机硬件和拍摄环境决定，直接影响噪点、对比度、亮度与细节，是图像预处理（去噪、增强）和相机标定必须考虑的参数。

**曝光时间（Exposure Time）**

- 快门打开的时间长短。
- 曝光时间长 → 易 **运动模糊（拖影）**；曝光时间短 → 图像可能 **偏暗、噪点多**。

**增益（Gain / ISO）**

- 感光元件对光信号的放大倍数。
- 增益过高会引入明显的 **感光噪声**（信号放大时噪点同时被放大）。

**光圈（Aperture）**

- 控制进光量的机械结构，决定 **景深**。
- 光圈大 → **景深浅**（背景易模糊）；光圈小 → **景深大**（前后景物都较清晰）。

**色彩空间（Color Space）**

- 像素数值的物理意义；处理前需明确是 **RGB**、**BGR**（OpenCV 默认）、**灰度（Gray）**，还是适合颜色分割的 **HSV/HSI**，亦或是视频常用的 **YUV**。

**白平衡（White Balance）**

- 调整 R/G/B 三通道相对比例，使在不同光源（日光、日光灯等）下，**白色物体在图像中仍呈现白色**。

### 9.3 文件及物理元数据（外设与传输关联）

这类信息属于封装在文件头或配置中的附加数据，用于图像读取、解析以及物理尺寸的还原。

**编码格式（Codec / Format）**

- 如 **BMP**（无压缩，可近似内存直接映射）、**JPEG**（有损压缩）、**PNG**（无损压缩）。
- 算法运行时须先 **解压** 还原为内存中的非压缩矩阵再处理。

**像素实际物理尺寸（Resolution Unit / DPI）**

- 单个像素在现实世界中代表的物理长度（如 DPI，或工业检测中 **μm/px**、**px/mm**）。
- 将图像中的 **像素距离** 转化为 **现实测量尺寸** 的基准。

**EXIF 标签（EXIF Tags）**

- 拍摄设备型号、时间戳、GPS 等。
- 多相机同步或时间序列处理时，**时间戳** 是关键信息。

### 9.4 本项目中的实际取值（汇总）

上文三类基础信息，在 GasketVision 三进程链路里 **具体是什么、写在哪、谁用**，如下表。

#### 一、内存与几何结构

| 基础项 | 本项目实际取值 | 配置 / 代码位置 | 说明 |
|--------|----------------|-----------------|------|
| **分辨率（宽×高）** | **1920 × 1080** | `generate_samples.py` 默认；`manifest.json` 的 `imageWidth` / `imageHeight`；SHM `ImagePlaneHeader.width/height` | OpenCV：`cv::Mat` **cols=1920、rows=1080**；HMI `QImage` 同宽高 |
| **分辨率上限** | **1920 × 1080**（写死） | `CameraIpc::kMaxImageBytes = 1920×1080×3`（[`CameraIpcLayout.h`](CameraService/ipc/CameraIpcLayout.h)） | 单槽像素区上限 = **编译期最大分辨率**；Gray→原地 BGR 整条链按 **1080p BGR** 卡上限 |
| **图像中心（像素）** | **(960, 540)** | `vision_engine.json` → `imageCenterPx`；`manifest.json` 同字段 | `GasketInspector` 测径 ROI、偏心基准；合成图垫片几何中心 |
| **通道数** | 采图 **1**；标注/显示 **3** | SHM `ImageFormat::Gray8` → 原地 `Bgr888` | Camera 发布 Gray8；Engine 在同槽 payload 上画 BGR；HMI 读 BGR 转 RGB 显示 |
| **数据类型 / 位深** | **8-bit unsigned** | `CV_8UC1`（Gray）、`CV_8UC3`（BGR）；`QImage::Format_RGB888` | 无 16-bit / float 图；阈值、模板匹配、径向扫描均按 0～255 |
| **步长（Stride）** | Gray：`bytesPerLine = width`；BGR：`bytesPerLine = width×3` | SHM `ImagePlaneHeader.bytesPerLine`；`GasketInspector` 标注时 `width*3` | `cv::Mat` 非连续时 `clone()` 再写入 SHM；HMI 按 `bytesPerLine` 逐行拷贝 BGR→RGB |
| **单槽 / 四槽容量** | 单槽 **≈5.93 MiB** 像素区 + 32B 头；四槽 + 控制块 **≈23.7 MiB** | `slotBytes()` × `kRingSlots=4` | 与 §9.1 分辨率上限一致；队列深度、反压窗口也对齐 **4 槽** |

#### 二、光学与成像特征

| 基础项 | 本项目实际取值 | 配置 / 代码位置 | 说明 |
|--------|----------------|-----------------|------|
| **曝光时间** | 合成：**无**（脚本绘制）；GigE：**未接入 SDK** | `GigEVisionCamera` 骨架 | 测试链路不受快门影响；正式 GigE 待 SDK 后再配 |
| **增益 / ISO** | **未使用** | — | 合成图无传感器噪声模型 |
| **光圈 / 景深** | **未使用** | — | 合成图为平面几何，无光学景深 |
| **色彩空间** | 检测：**Gray**；算法/OpenCV 内部：**BGR**；HMI 显示：**RGB**（由 BGR 手动交换） | `GasketInspector`、`CameraIpcSubscriberWorker::planeToImageView` | 全流程不做 HSV/YUV；HMI **不链 OpenCV**，只收 `QImage` |
| **白平衡** | **未使用** | — | 灰度合成 + 固定灰度值（背景 38、垫片 205 等），无 RGB 白平衡 |

#### 三、文件及物理元数据

| 基础项 | 本项目实际取值 | 配置 / 代码位置 | 说明 |
|--------|----------------|-----------------|------|
| **编码格式** | 样品：**8 位灰度 PNG**（无损）；落盘标注：**PNG** | `test/station1/*.png`；HMI `captures/{frameId}_annot.png` | 运行时：`SyntheticImageSource` / `cv::imread` **解压**为 `cv::Mat`；SHM 内为 **未压缩** 像素 |
| **像素物理尺寸** | **`pxPerMm = 10.0`** → **1 px = 0.1 mm** | `vision_engine.json`、`manifest.json` | 外径 12 mm ≈ 120 px 直径；测径、偏心、`manifest` 真值均按此换算 |
| **EXIF** | **未使用** | — | 样品 PNG 无 EXIF；时间用 **`VisionFrame.timestampMs`**、SHM `ControlBlock.timestampMs`，非 EXIF |
| **样品真值** | `manifest.json`：`case`、`expectedOk`、OD/ID（mm） | `test/manifest.json` | 与 `GasketInspector` 公差、`diagnose_samples.py` 对照验收 |
| **定位模板** | **`templates/fiducial_L.png`**（约 **96×72**，随 1080p 缩放） | `vision_engine.json` → `templatePath` | `matchTemplate` 缺件判断；角标在合成图左上角按 `SCALE_X/Y` 放大 |

#### 四、跨进程传递（补充）

| 环节 | 图像形态 |
|------|----------|
| Camera → SHM | Gray8，`payloadSize = 1920×1080` |
| Engine 读/检 | `cv::Mat` Gray 浅拷 → 同槽原地 BGR 标注，`payloadSize = 1920×1080×3` |
| HMI 读 SHM | BGR → `QImage` RGB888 浅拷 → `ShmImageLabel` / 落盘 PNG |
| 套接字 | **不传像素**；仅 Camera↔HMI `ready/ack` |

**面试可一句话：** 本项目图像是 **1080p、8-bit**；链路 **Gray 进、BGR 标注出**；物理尺寸靠 **`pxPerMm=10`**；文件侧 **PNG 样品 + manifest 真值**；**单槽上限即写死的 1920×1080**，与合成脚本、SHM、队列四槽一致。

---

## 十、项目使用的容器

本项目 **以 Qt 容器为主**；STL 仅 **`std::vector`** 与 **`std::unique_ptr`**。**未使用** `std::deque` / `std::list` / `std::map` / `std::set` / `QVector`。

以下按 **容器类型** 列出：当前存什么、为何选这种结构。

### 9.1 队列（FIFO）：`QQueue`

| 成员 | 进程 | 存放内容 | 为何用队列 |
|------|------|----------|------------|
| `CameraPublishWorker::m_pendingFrames` | CameraService | 已 `grab`、**尚未写入 SHM** 的 `VisionFrame`（含 `cv::Mat`、路径、时间戳） | 采图线程与发布线程解耦；**先进先出** 保 `frameId` 顺序；非严格模式深度 ≤2，满则丢新帧 |
| `StationCommWorker::m_pendingInspect` | VisionEngine | 已从 SHM **读出 Gray**、**尚未交给算法** 的 `VisionFrame` | 通信线程 poll 读 SHM 与 OpenCV 检测解耦；深度 ≤4 与四槽环对齐；`m_inspectRunning` 保证串行出队 |

**操作习惯：** 正常 `enqueue` / `dequeue`；Camera 发布失败时 **`prepend` 回队首** 保序重试。

### 9.2 哈希映射：`QHash`

| 成员 / 用途 | 进程 | Key → Value | 为何用 QHash |
|-------------|------|-------------|--------------|
| `InspectionAggregator::m_perStation` | HMI | `stationId` →（`"OK"`/`"NG"` → 计数） | 按工位累加 OK/NG；**O(1)** 查找，工位数少、键为 int/字符串 |
| `OkNgStatsPanel::m_counts` | HMI | 变量名 → 计数 | 刷 OK/NG 面板；键无序、只查改计数 |
| `PieChartConfig::m_varCounts` | HMI（饼图） | 变量名 → 计数 | 与 `InspectionWindow::setPieCounts` 对接；频繁按名更新 |
| `PieChartConfigDialog::m_nameToCountSpin` | HMI（饼图） | 变量名 → `QSpinBox*` | 编辑对话框按名找控件 |
| `SinglePieChart::applySectorDataCountsFromHash` | HMI（饼图） | 外部传入的 变量名→计数 | 扇区顺序固定，用 Hash **按名取 count** 再组装扇区列表 |

**为何不用 `QMap`：** 不需要按键排序；`QHash` 均摊查找更快，与 Qt 信号槽传参习惯一致。

### 9.3 有序映射：`QMap`

| 成员 / 用途 | 进程 | Key → Value | 为何用 QMap |
|-------------|------|-------------|-------------|
| `interleaveByCase` 局部 `groups` | CameraService | 缺陷 case 前缀（`ok` / `od_oversize` / …）→ 该 case 的 **`QStringList` 路径** | 按 **固定 kOrder 顺序** 轮询各 case；`QMap` 键有序，便于按名取组 |
| （仅此一处业务 `QMap`） | | | 样品分组后还要 **组内 sort**，组间 **交错出队** |

### 9.4 哈希集合：`QSet`

| 成员 / 用途 | 进程 | 存放内容 | 为何用 QSet |
|-------------|------|----------|-------------|
| `PieChartConfig::validate` 内 `seen` | HMI（饼图） | 当前饼内已出现的变量名 | **去重**：同一饼不允许重复变量名 |
| `PieChartConfigDialog` 内 `s` / `uniq` | HMI（饼图） | 各编辑框汇总后的不重复变量名 | 合并多饼变量名时 **去重**，再 `values()` 转列表排序 |

**为何不用 `QList` 线性查重：** 变量名个数少但可能重复输入，Set 语义更清晰、O(1) 插入查重。

### 9.5 序列列表：`QList` / `QStringList`

| 成员 / 用途 | 进程 | 存放内容 | 为何用 QList / QStringList |
|-------------|------|----------|---------------------------|
| `SyntheticImageSource::m_files` | CameraService | 交错排序后的 **样品 PNG 路径** | 顺序播放；`QStringList` 与 `QDir::entryList` 直接对接 |
| `PieChartConfig::m_varsPerPie` | HMI（饼图） | `QList<QStringList>`：每个饼图的变量名列表 | 保留下标与饼图一一对应；内层 `QStringList` 保扇区顺序 |
| `SinglePieChart::m_sectorData` / `m_sectorColors` | HMI（饼图） | 扇区数据 `VarCountData`、扇区颜色 | 绘制顺序 = 列表顺序；与 `QPainter::drawPie` 角序一致 |
| `SinglePieChart` 局部 `start16` / `span16` | HMI（饼图） | 各扇区起始角、跨度（1/16 度单位） | 一次布局、多次绘制（内标签/外引线） |
| `widget.h::m_charts` | HMI（饼图） | `SinglePieChart*` 指针列表 | 多饼控件按索引管理 |
| `CameraHmiProtocol::parseReady/Ack` | HMI / Camera | 行协议按空格拆出的字段 | `QString::split` 自然得到 `QStringList` |

**说明：** `QList` 与 `QVector` 在 Qt6 常等价；本项目 **未显式使用 `QVector`**。

### 9.6 STL 动态数组：`std::vector`

| 成员 / 用途 | 进程 | 存放内容 | 为何用 vector |
|-------------|------|----------|---------------|
| `GasketInspector::measureRing` 内 `innerRadii` / `outerRadii` | VisionEngine | 360° 径向扫描得到的 **内外径像素半径样本** | 长度随有效射线变化；配合 **`std::nth_element` 取中位数**（O(n) 部分排序） |
| `Widget::applyExternalVarCountData(const std::vector<VarCountData>&)` | HMI（饼图） | 外部 API 传入的扇区数据 | 接口层用 STL 向量；内部 **立刻转成 `QHash`** 写入 `PieChartConfig` |

**为何不用 `QVector`：** 测径在 OpenCV/STL 算法侧，`vector` + `nth_element` 更惯用；饼图入口仅一处跨模块 API。

### 9.7 字节缓冲：`QByteArray`

| 成员 / 用途 | 进程 | 存放内容 | 为何用 QByteArray |
|-------------|------|----------|-------------------|
| `CameraHmiSocketClient::m_readBuffer` | CameraService | 套接字 **未拆行的原始字节** | 行协议可能半包到达；累积后再 `takeLine` |
| `CameraHmiSocketServer::m_readBuffer` | HMI | 同上 | 与 Client 对称 |
| `SocketLineCodec::encodeLine` | HMI / Camera | UTF-8 行文本 + `\n` | Qt 套接字 `write`/`read` 原生类型 |
| 配置文件 / SHM 路径写入 | 各进程 | `QString` → UTF-8 字节写入 IPC 定长 `char[]` | 与 C 风格 IPC 布局衔接 |

### 9.8 图像矩阵：`cv::Mat` / `QImage`

| 类型 | 进程 | 存放内容 | 为何用这种类型 |
|------|------|----------|----------------|
| `VisionFrame::image`（`cv::Mat`） | Camera / Engine | 采图或 SHM 浅拷的 **Gray8** 矩阵 | OpenCV 检测全链路；可与 SHM payload **共享内存视图**（不二次拷贝） |
| `GasketInspector::m_template` / `annotated` | VisionEngine | 定位模板、标注结果 | `matchTemplate`、`circle`、`putText` 等 API 原生类型 |
| `InspectionResult::annotatedImage`（`QImage`） | HMI | SHM BGR 平面转成的 **显示用图像** | HMI **不链 OpenCV**；`QImage` 供 `QLabel`/`ShmImageLabel` 绘制 |
| `ShmImageLabel::m_view` | HMI | 当前帧浅拷视图 + `frameId` | 避免 `QPixmap::fromImage` 整图深拷；paint 时直接 `drawImage` |

**分工：** 算法侧 **`cv::Mat`**，界面侧 **`QImage`**，在 IPC/信号槽边界做一次格式转换。

### 9.9 配置树：`QJsonObject` / `QJsonArray`

| 用途 | 存放内容 | 为何用 JSON 树 |
|------|----------|----------------|
| `vision_engine.json` 加载 | 工位、公差、`strictSampleAccounting`、`intervalMs` 等 | 三进程共用一份配置；Qt 内置解析 |
| `PieChartConfig::toJson` / `fromJson` | 饼图变量名列表、各变量计数 | 持久化到 `.json`；`QJsonArray` 表数组，`QJsonObject` 表键值 |
| `CameraPublishWorker::configure` 读 `stations` | 工位数组 | 嵌套配置，数组遍历选 `stationId==1` |

**说明：** 只读配置，**非常驻业务队列**；运行时计数走 `QHash`，不走 JSON。

### 9.10 所有权与 IPC 定长区

| 类型 | 成员 / 布局 | 存放内容 | 为何用这种结构 |
|------|-------------|----------|----------------|
| `std::unique_ptr<IVisionImageSource>` | `CameraGrabWorker` / `CameraPublishWorker` / Factory | 合成或 GigE **采图源** | 工厂创建、独占所有权；进程内无需共享 |
| `char path[260]` 等 | `CameraIpc::ControlBlock` | 源路径、标注路径、相机状态、缺陷文本 | SHM **固定布局**、跨进程 C 兼容；长度上限防越界 |
| **四槽环形区**（非 STL） | `CameraIpc::kRingSlots = 4` | 槽头 + Gray/BGR 像素区 | `frameId % 4` 选槽；**不是** `std::deque`，是共享内存上的手动环 |

### 9.11 小结（按容器选型原则）

| 需求 | 本项目选用 |
|------|------------|
| 跨线程帧流水线、保序 | **`QQueue<VisionFrame>`** |
| 按名统计 OK/NG / 饼图计数 | **`QHash<QString, uint>`** |
| 按 case 分组再固定顺序轮播 | **`QMap` + `QStringList`** |
| 变量名去重 | **`QSet<QString>`** |
| 有序 UI / 扇区 / 路径列表 | **`QList` / `QStringList`** |
| 算法临时样本 + 中位数 | **`std::vector<double>`** |
| 套接字半包 | **`QByteArray`** |
| 检测 vs 显示 | **`cv::Mat` vs `QImage`** |
| 跨进程像素与控制块 | **SHM 四槽环 + 定长 `char[]`** |

