# 机器人调试工具系统架构设计与技术选型

> 文档状态：架构基线  
> 目标平台：Linux  
> 产品形态：Desktop 为主，Web 为辅  
> 技术原则：轻量、稳定、开发效率优先；机器人高频通信与关键执行逻辑保持 C++17  
> 适用范围：本文定义可复用的系统架构、技术选型和工程边界；具体机器人产品的页面、参数、状态、控制接口和业务流程按实际需求定义

---

## 1. 项目目标与架构原则

### 1.1 项目目标

本项目面向机器人研发、联调、测试和维护场景，设计一套可长期维护、可稳定发布、开发成本可控的机器人调试工具架构。

本文中的“通用”指**架构设计和技术选型可复用**，而不是要求应用本身成为支持任意机器人型号的万能调试平台。具体产品应优先使用产品专用、强类型的数据模型和接口，不为了通用化提前引入 Capability、动态 Schema、通用命令解释器等复杂机制。

典型业务能力包括：

- 机器人连接和通信配置；
- 上电、下电、使能、失能；
- 参数读取、修改和保存；
- 关节、驱动器、控制器、传感器、电池、IO 等状态查看；
- Jog、位置控制、任务执行等运动调试；
- 零位、电角度等标定；
- 故障、告警、诊断和清错；
- 标定、固件升级等长任务。

产品以 **Desktop 为主要使用和发布形态，Web 为辅助访问形态**。Desktop 与 Web 共用唯一业务 UI。

本设计不考虑业务文件上传、业务文件下载、用户选择任意本地路径或任意业务文件读写。应用自身的配置、日志、SQLite 元数据、升级产物和受控固件 Artifact 属于运行基础设施，不属于上述业务文件能力。

### 1.2 核心原则

1. **实时与机器人通信保持 C++17**  
   ROS 2 Topic、Service、Action、状态缓存和机器人业务适配由 C++17 实现。Python 和前端不进入高频机器人通信路径。

2. **前后端正式分离**  
   前端只通过 REST/WebSocket 使用 Backend，不依赖 ROS 2，也不感知 ROS 消息、Service、Action 类型。

3. **Desktop/Web 共用唯一业务 UI**  
   `frontend/web` 是唯一 Vue 业务前端；`frontend/desktop` 只承担 Electron 桌面壳和必要的平台能力。

4. **Backend 以开发效率为优先**  
   Web API 采用 Python 3.12 + FastAPI + Pydantic v2，不在 C++ 中重复建设 Web 路由、校验、OpenAPI、WebSocket 和认证基础设施。

5. **Python/C++ 边界保持窄且稳定**  
   Python 只调用业务 Facade，不暴露 `rclcpp::Node`、ROS Message、DDS 对象和底层线程对象。

6. **接口面向产品业务，不做 ROS Proxy**  
   API 使用连接、上电、Jog、标定、升级等业务语义，不设计通用 `call_ros_service`、`publish_topic` 等接口。

7. **具体领域模型由产品定义**  
   架构只统一状态快照、Task、错误、日志、安全、版本和发布等机制，不定义统一 Robot Capability、Robot Model 或 Parameter Schema。

8. **调试工具不是最终安全执行体**  
   UI、Backend、C++ Robot Layer 乃至独立 Robot Service 都可能异常退出。凡要求调试工具进程退出后仍必须生效的 watchdog、通信失联停止、硬件互锁和 fail-safe，必须由更靠近机器人执行端的 Robot Controller / Runtime 或独立安全执行体保证。

9. **一个 Robot Control Endpoint 只有一个 Authoritative Backend**  
   多个 Desktop/Browser 可以连接同一个 Backend，但默认不允许多个 Backend 同时对同一机器人执行控制和维护操作。

10. **轻量优先**  
    V1 不引入消息队列、外部数据库服务、微服务框架、Nx/Turborepo、通用服务发现等没有明确收益的组件。只保留对可靠性和工程落地确有必要的机制。

### 1.3 V1 架构变体

V1 支持两种部署变体，默认优先使用 Variant A。

#### Variant A：Embedded Robot Layer

```text
Electron / Browser
        │
        │ REST / WebSocket
        ▼
FastAPI Backend
        │
        │ pybind11
        ▼
C++17 Robot Application
        │
        │ ROS 2 / product protocol
        ▼
Robot Controller / Runtime
```

适用于 Robot Controller / Runtime 已能独立处理调试工具失联、安全停止和必要互锁的产品。

Variant A 是默认方案，因为进程更少、部署简单、开发效率最高。

#### Variant B：Independent Robot Service

```text
Electron / Browser
        │
        │ REST / WebSocket
        ▼
FastAPI Backend
        │
        │ IPC / RPC Adapter
        ▼
Independent C++ Robot Service
        │
        │ ROS 2 / product protocol
        ▼
Robot Controller / Runtime
```

只有出现明确需求时采用 Variant B，例如：

- Robot Layer 生命周期必须独立于 FastAPI；
- Backend 与 Robot Layer 需要进程级故障隔离；
- Robot Layer 必须在 Electron/Backend 退出后继续存在；
- 产品安全分析明确要求独立 Robot 进程。

Variant B 也不能替代 Robot Controller / Runtime 的最终 watchdog/fail-safe。

#### 两种变体的代码复用原则

两种变体共享同一套 C++ Robot Application 和逻辑 Facade，仅替换边界适配层：

```text
                  C++ Robot Application
                           │
              ┌────────────┴────────────┐
              │                         │
       Pybind11 Adapter            Service Adapter
         Variant A                  Variant B
              │                         │
              └────────── FastAPI ──────┘
```

逻辑接口保持一致：

```text
Robot Core Facade
├── connect / disconnect
├── snapshot
├── task_status
├── cancel_task
└── reconcile_task

Product Facade
└── 由具体产品定义上电、使能、Jog、标定、升级等强类型接口
```

Variant B 的具体传输协议不在本架构中冻结。只有产品确认采用 Variant B 时，再从 Unix Domain Socket、轻量 RPC、gRPC 等方案中选择。**传输协议可以变化，但 Robot Application 的业务接口不应重新设计。**

### 1.4 Safety Admission

正式产品必须验证其调试工具失联后的安全行为。Safety Admission 对 Variant A/B 都适用；Variant B 还需额外验证 Robot Service 异常退出时的机器人行为。

至少确认：

```text
调试工具/Robot Service 失联后，机器人如何检测？
运动命令是否有 watchdog / timeout？
最长多久进入产品定义的安全状态？
关键硬件/软件互锁是否独立于调试工具？
Task 执行中客户端消失时，机器人如何处理？
kill Backend / Robot Service、断网、DDS 中断时是否满足安全要求？
```

每个正式产品版本，以及 Robot Controller/Firmware 的安全相关行为发生变化时，应重新验证。结果记录在产品安全文档或 `docs/safety/` 中，并关联调试工具版本和机器人固件版本。

### 1.5 非目标

本文不解决：

- 硬实时控制循环；
- 云端机器人集群管理；
- 多租户 SaaS；
- 通用 ROS 图形化浏览器；
- 浏览器直接访问 ROS 2；
- 通用动态 Capability/Schema 平台；
- 用户业务文件管理；
- 用调试工具替代机器人控制软件或安全控制系统。

---

## 2. 系统总体架构

### 2.1 总体分层

默认 Variant A：

```text
                         ┌──────────────────────────────┐
                         │        frontend/web          │
                         │ Vue 3 + TypeScript + Vite    │
                         │ Router + Pinia + Element Plus│
                         └──────────────┬───────────────┘
                                        │
                           ┌────────────┴────────────┐
                           │                         │
                      Browser / Web            Electron Desktop
                        辅助形态                  主产品形态
                                                     │
                                            frontend/desktop
                                             Main + Preload
                           │                         │
                           └────────────┬────────────┘
                                        │
                                 REST + WebSocket
                                        │
                                        ▼
                         ┌──────────────────────────────┐
                         │           backend            │
                         │ Python 3.12 + FastAPI        │
                         │ Pydantic v2 + Uvicorn        │
                         │ API / Task / Auth / SQLite   │
                         └──────────────┬───────────────┘
                                        │ pybind11
                                        ▼
                         ┌──────────────────────────────┐
                         │            robot             │
                         │ C++17 Robot Application      │
                         │ State / Task / Operation     │
                         │ ROS Adapter                  │
                         └──────────────┬───────────────┘
                                        │
                                        ▼
                                   ROS 2 / rclcpp
                                        │
                                        ▼
                         Robot Controller / Runtime
                         （最终安全执行边界）
```

职责边界：

- **Web UI**：业务界面和交互；
- **Desktop Shell**：窗口、进程生命周期、可信本地启动入口；
- **Backend**：REST/WebSocket、校验、认证、业务编排、Task 元数据；
- **Robot Application**：C++ 机器人业务适配、状态、Task 和 ROS 2 通信；
- **Robot Controller / Runtime**：真正的机器人执行与最终安全边界。

### 2.2 Authoritative Backend 与客户端拓扑

同一个 Robot Control Endpoint 默认只有一个 Authoritative Backend：

```text
Desktop Client A ─┐
Browser Client B ─┼──► Authoritative Backend ───► Robot
Desktop Client C ─┘
```

Desktop 支持两种模式：

```text
Local Backend Mode
Electron 启动并拥有本地 Backend
Vue UI → Local Backend

Remote Backend Mode
Electron 不启动本地 Backend
Vue UI → 已部署的 Authoritative Backend
```

Web Browser 只作为 Backend 客户端。

V1 不做自动服务发现。Remote Backend endpoint 由产品部署配置或用户明确配置。

如果实际系统无法避免多个 Backend 同时连接同一 Robot Controller，则跨 Backend 的控制所有权必须由机器人侧提供；进程内 `OperationCoordinator` 不能作为分布式锁。

### 2.3 进程与生命周期

Variant A 的 Desktop 默认进程关系：

```text
Electron Main Process
├── Renderer Process        Vue UI
└── Backend Process         Python/FastAPI
       └── pybind11 module  C++17 Robot Application
             └── ROS executor threads
```

Variant B：

```text
Electron Main Process
├── Renderer Process
├── Backend Process
└── Independent Robot Service   # 是否由 Electron/systemd 管理由具体部署决定
```

Variant B 只冻结以下生命周期约束，不在架构层规定必须使用 systemd 或 Electron 启动：

- Robot Service 是独立进程；
- 同一 Robot Control Endpoint 最多有一个有效 Robot Service 实例；
- Backend 是 Robot Service 的客户端；
- Robot Service 不可用时 Backend 禁止 CONTROL/MAINTENANCE；
- Backend 必须能检测 Robot Service 断开；
- Robot Service 不写 Backend 的 SQLite；
- Robot Service 使用独立日志，配置目录可与产品共用；
- Robot Service 的具体 ownership/restart 策略由产品部署设计确定。

Local Backend Mode 启动流程：

```text
启动 Electron
   ↓
获取 single-instance lock
   ↓
生成 local session token + 随机 loopback port
   ↓
通过 inherited pipe/stdin 等一次性 bootstrap channel 启动 Backend
   ↓
Backend preflight
   ↓
GET /api/v1/health
   ↓
Backend Recovery
   ↓
读取 /api/v1/system/info 并检查版本兼容
   ↓
加载本地 web/dist
   ↓
建立 REST / WebSocket
```

Local session token 生命周期与 Backend 进程一致，不做 token refresh。Backend 重启即生成新 token。

### 2.4 Topic、Service、Action 数据路径

#### 高频状态

```text
ROS Topic 500 Hz 或更高
      ↓
C++ callback
      ↓
StateStore / Latest Snapshot
      ↓
30 Hz 默认、最高约 60 Hz snapshot
      ↓
FastAPI WebSocket
      ↓
Pinia Store
      ↓
Vue UI
```

高频样本只在 C++ 中更新。UI 使用最新一致状态，不把 500 Hz 通信频率直接传播到 Python/Browser。

#### Service / 短命令

```text
Vue → REST → FastAPI → Application Service
                         ↓ worker thread
                    Robot Facade
                         ↓
                   pybind11 / IPC
                         ↓
                C++ Robot Application
                         ↓
                     ROS Service
```

约束：

- Service/Command 必须有 timeout；
- HTTP 连接断开不等于机器人操作自动取消；
- 同步 C++ 调用通过 worker thread 执行；
- `snapshot()` 只有在严格有界且足够轻量时才允许直接执行；大 payload 转换也不得阻塞 event loop。

#### Action / 长任务

```text
产品强类型 REST API
      ↓
TaskManager 创建 task_id
      ↓
持久化 pending Task
      ↓
Product Facade(context with task_id)
      ↓
C++ / ROS Action / Robot Runtime
      ↓
返回 robot-side execution_handle
      ↓
Task 状态 / progress / result
```

Task 生命周期可以通用，但业务入口不统一成 `start_task(type, args)`。标定、固件升级等仍使用产品专用强类型 API。

### 2.5 Backend Phase 与 Recovery

Backend 使用一个简单的运行阶段表示启动、恢复、控制可用性和升级准备状态：

```text
starting
   ↓
preflight
   ↓
recovering
   ├── 全部状态可确认 → ready_for_control
   └── 存在未知状态   → ready_read_only

ready_read_only
   └── 产品恢复成功 → recovering → ready_for_control

ready_for_control
   ↓ prepare-upgrade
preparing_upgrade
   ├── 正常路径 → stop Backend → 启动新版本
   └── 放弃升级 → restart Backend → starting
```

语义：

- `starting`：Backend 进程初始化；
- `preflight`：检查运行环境和依赖；
- `recovering`：恢复 Task 和机器人执行状态，禁止新的 CONTROL/MAINTENANCE；
- `ready_read_only`：读取能力可用，只允许读取和产品定义的恢复操作；
- `ready_for_control`：读取、控制和维护能力可按权限正常使用；
- `preparing_upgrade`：已原子进入应用升级窗口，读取能力仍可用，但拒绝新的 CONTROL/MAINTENANCE 和新 Task。

`preparing_upgrade` 是**当前 Backend 实例的临时运行状态**：

- 不写入 SQLite；
- 不跨 Backend 重启恢复；
- 正常路径是尽快停止当前 Backend 并启动新版本；
- 如果用户放弃升级或升级器在停止 Backend 前失败，不设计额外 Upgrade Cancel/Lease 协议，直接重启 Backend；
- 任意 Backend 新进程都必须从 `starting` 开始，并重新执行 preflight/recovery，不能从上一个进程恢复为 `preparing_upgrade`。

`GET /api/v1/health` 直接暴露 `phase`，不设计第二套 Readiness API。`ready` 只表示 Backend 是否已具备正常业务读取能力，**不表示机器人当前是否允许控制**：

| phase | ready | CONTROL / MAINTENANCE |
| --- | --- | --- |
| `starting` | `false` | 禁止 |
| `preflight` | `false` | 禁止 |
| `recovering` | `false` | 禁止 |
| `ready_read_only` | `true` | 禁止 |
| `ready_for_control` | `true` | 按权限和产品状态允许 |
| `preparing_upgrade` | `true` | 禁止 |

示例：

```json
{
  "status": "ok",
  "ready": true,
  "phase": "ready_for_control"
}
```

只要存在无法确认的旧任务、机器人状态 stale/disconnected 或 control owner 冲突，就不能进入 `ready_for_control`。

### 2.6 故障行为

| 故障 | 预期行为 |
| --- | --- |
| Renderer 崩溃 | Backend/C++ 不受影响；重载后重新读取 snapshot/task |
| Electron Main 崩溃 | Local Backend 按 ownership 策略退出；Robot Controller 保证失联安全 |
| Backend/C++ 崩溃 | Robot Controller watchdog/fail-safe 生效；重启后进入 recovering |
| Robot Service 崩溃 | Variant B 中 Backend 标记 Robot Service unavailable；禁止控制 |
| ROS 连接中断 | State 标记 disconnected/stale；拒绝控制/维护 |
| WebSocket 断开 | 自动重连并重新获取 REST snapshot |
| Service timeout | 返回统一错误；不得假定请求成功或失败 |
| Task 状态无法确认 | 标记 `unknown` 或 `interrupted`；保持 read-only，等待产品恢复流程 |

---

## 3. 技术选型与工程组织

### 3.1 技术基线

| 层级 | 技术选型 | 主要职责 |
| --- | --- | --- |
| Web | Vue 3 + TypeScript | 唯一业务 UI |
| Build | Vite | Web 构建与开发 |
| Router | Vue Router | 页面路由 |
| State | Pinia | UI/Robot/Task 状态 |
| UI Library | Element Plus | 基础 UI 组件 |
| Desktop | Electron | 主产品桌面壳 |
| Frontend Package | pnpm | 依赖和 Workspace |
| Backend | Python 3.12 + FastAPI | REST/WebSocket/业务编排 |
| Schema | Pydantic v2 | API/事件模型和校验 |
| ASGI | Uvicorn | Backend Server |
| Variant A Binding | pybind11 | Python/C++ 绑定 |
| Robot | C++17 | Robot Application |
| ROS | ROS 2 / rclcpp | Topic/Service/Action |
| Metadata | SQLite | Task/Audit/Recovery 元数据 |
| 发布 | AppImage 主、`.deb` 辅 | Linux 交付 |

### 3.2 Frontend

`frontend/web` 固定采用：

```text
Vue 3
TypeScript
Vite
Vue Router
Pinia
Element Plus
```

统一使用 Composition API 和 `<script setup lang="ts">`。

Element Plus 采用按需导入，仅作为控件基础。产品视觉通过项目自己的 Design Tokens 管理，包括颜色、字体、间距、组件高度、告警状态和危险操作语义。

`frontend/desktop` 不维护第二套 Vue Renderer，只加载 `frontend/web/dist`。

pnpm Workspace 只覆盖 Frontend：

```text
frontend/
├── package.json
├── pnpm-lock.yaml
├── pnpm-workspace.yaml
├── web/
└── desktop/
```

V1 不引入 Nx/Turborepo，也不把 Python/C++ 强行放进 JS monorepo。

### 3.3 Backend 与异步边界

Backend：

```text
Python 3.12
FastAPI
Pydantic v2
Uvicorn
SQLite
```

调用链：

```text
FastAPI Router
    ↓
Application Service
    ↓
robot_facade/
    ↓
pybind11 Adapter / Robot Service Client
    ↓
C++ Robot Application
```

Backend 不使用 `rclpy`，机器人通信只维护一份 C++ 实现。

线程规则：

- 短小、严格有界的读取可以直接执行；
- 普通同步 C++ 调用进入 worker thread；
- 长操作统一 Task 化；
- ROS Executor 完全在 C++ 线程中；
- 高频 ROS callback 不回调 Python；
- C++ 释放 GIL 不能替代 FastAPI 对阻塞调用的 worker 调度。

### 3.4 Core Facade 与 Product Facade

Core Facade 只定义架构层真正通用的能力：

```text
RobotCoreFacade
├── connect(context)
├── disconnect(context)
├── snapshot()
├── task_status(task_id)
├── cancel_task(context, task_id)
└── reconcile_task(execution_handle)
```

其中两个 Task 查询接口职责必须区分：

- `task_status(task_id)`：正常运行期间查询 Robot Application **当前已知**的 Task 运行态信息；输入是调试工具自己的 `task_id`，不主动执行 Backend crash recovery，也不以此替代 Backend TaskManager/SQLite 的持久化 Task View；
- `reconcile_task(execution_handle)`：仅用于 `recovering` 阶段，通过持久化的 robot-side `execution_handle` 向机器人执行端重新确认旧执行的真实状态。

`reconcile_task()` 在架构层只冻结四种逻辑结果语义，不提前规定具体 C++ 返回类型：

```text
confirmed_running
confirmed_terminal
not_found
indeterminate
```

含义：

- `confirmed_running`：机器人执行端确认该执行仍在运行；
- `confirmed_terminal`：机器人执行端确认执行已结束，并返回可用的终态/结果信息；
- `not_found`：执行端无法找到该 handle；不能在架构层直接等价为“已经停止”，由产品适配规则映射为 `interrupted` 或 `unknown`；
- `indeterminate`：执行端无法可靠确认，Backend 将 Task 置为 `unknown`。

Backend 对外 `GET /api/v1/tasks/{task_id}` 的 Task View 可以组合以下三层信息：

```text
Backend TaskManager / SQLite   调试工具持久化记录
C++ TaskStore                  当前进程内机器人执行映射
Robot Controller / Action     最终执行真相
```

Product Facade 由具体产品定义：

```text
ProductRobotFacade
├── power_on(context)
├── power_off(context)
├── enable(context)
├── disable(context)
├── set_joint_parameters(context, request)
├── jog(context, request)
├── start_calibration(context, request) -> StartTaskResult
├── start_firmware_upgrade(context, request) -> StartTaskResult
└── clear_fault(context, request)
```

长任务的 `StartTaskResult` 只返回 robot-side `execution_handle` 等启动结果；应用级 `task_id` 已由 Backend TaskManager 预先创建。

不在 Core Facade 中预设：

```text
capabilities()
model()
parameter_schema()
set_parameter(name, value)
execute_command(name, args)
```

### 3.5 OperationContext 与 Task ID

所有有副作用的调用显式携带：

```text
OperationContext
├── request_id
├── task_id              # 长任务时预先生成
├── actor
├── operation
├── backend_instance_id
└── client_id            # 可选
```

ID 所有权固定为：

```text
request_id
  FastAPI 每个业务请求生成

task_id
  Backend TaskManager 在调用 C++ 前生成
  属于调试工具自己的 Task ID

execution_handle
  Robot Controller / ROS Action 创建
  属于机器人侧执行 ID
```

创建长任务时：

```text
FastAPI 生成 request_id
   ↓
TaskManager 生成 task_id
   ↓
写入 pending Task
   ↓
OperationContext(request_id, task_id, ...)
   ↓
Product Facade
   ↓
robot-side execution_handle
   ↓
更新 Task
```

`type` 是产品定义的稳定字符串，仅用于查询、日志、审计和 UI 展示，不作为通用任务调度机制。

### 3.6 StateStore、Task 与 OperationCoordinator

C++ Robot Application 至少包含：

- `StateStore`：保存最新一致 Robot State；
- `TaskStore`：当前机器人侧 Task 映射；
- `OperationCoordinator`：单 Backend 内的操作互斥；
- ROS/Product Adapter；
- Product Application。

Operation Lease 保持简单：

```text
OperationLease
├── lease_id
├── operation_class
├── owner_kind        # task | request
├── owner_id          # task_id | request_id
├── operation
└── acquired_at
```

基础类别：

```text
READ            可并发
CONFIGURATION   按产品规则
CONTROL         默认排他
MAINTENANCE     默认排他
```

Lease 只表达当前 Backend 内部互斥，不是持久化执行真相，不设计 TTL/续约协议。

Backend 崩溃后不恢复旧 lease，而是通过 persisted Task、execution handle 和当前 Robot State 重新判断有效占用。

错误语义：

- `ROBOT_BUSY`：存在冲突 Operation；
- `ROBOT_STATE_CONFLICT`：机器人当前状态不满足前置条件；
- `OPERATION_REJECTED`：本地条件满足，但机器人执行端拒绝。

### 3.7 SQLite

SQLite 只保存真正需要恢复或查询的轻量元数据：

```text
Task metadata
robot-side execution_handle
Task terminal result
关键 Task checkpoint
Audit index
Recovery metadata
```

不保存：

```text
500 Hz Robot State
Telemetry
ROS 原始样本
30/60 Hz WebSocket state
高频 Task progress
```

SQLite 只由 Authoritative Backend 写入，不作为跨 Backend 共享数据库。

Task progress 主要保存在内存；只在创建、execution handle 建立、状态转换、关键 checkpoint、低频周期和 terminal state 时持久化。

### 3.8 推荐目录

```text
robot-debugger/
├── frontend/
│   ├── package.json
│   ├── pnpm-lock.yaml
│   ├── pnpm-workspace.yaml
│   ├── web/
│   │   └── src/
│   └── desktop/
│       ├── src/main/
│       ├── src/preload/
│       └── resources/
│
├── backend/
│   ├── src/robot_debugger/
│   │   ├── api/
│   │   ├── models/
│   │   ├── services/
│   │   ├── auth/
│   │   ├── websocket/
│   │   ├── robot_facade/
│   │   ├── persistence/
│   │   └── main.py
│   └── pyproject.toml
│
├── robot/
│   ├── include/
│   ├── src/
│   ├── ros/
│   ├── bindings/python/       # Variant A
│   ├── service/               # Variant B 采用时增加
│   └── CMakeLists.txt
│
├── packaging/
├── scripts/
├── docs/
└── README.md
```

不创建没有明确职责的 `core/`、`common/`、`utils/` 大杂烩目录。

---

## 4. 核心接口与数据模型

### 4.1 API 基线

Backend 使用：

```text
REST        查询、配置、命令、Task 控制
WebSocket   状态流、Task 进度、异步事件
JSON        默认序列化
OpenAPI     REST 契约
JSON Schema WebSocket payload 契约
```

统一前缀：

```text
/api/v1
```

系统级基础端点：

```text
GET  /api/v1/health
GET  /api/v1/system/info
GET  /api/v1/robot/state
GET  /api/v1/tasks/{task_id}
POST /api/v1/tasks/{task_id}/cancel
GET  /api/v1/system/upgrade-readiness
POST /api/v1/system/prepare-upgrade
WS   /api/v1/events
```

连接、上下电、参数、运动、标定、诊断、固件升级等 API 由具体产品定义。

REST 原则：

- GET 只读，可受控重试；
- PUT 用于可重复设置，尽量幂等；
- POST 用于命令、状态转换和创建 Task；
- 有副作用的 POST 不自动重试；
- Service/Command 必须有 timeout；
- HTTP 断开不自动取消机器人操作。

### 4.2 Health 与 System Info

`GET /api/v1/health` 是最小探针：

```json
{
  "status": "ok",
  "ready": true,
  "phase": "ready_for_control"
}
```

Health 不返回机器人型号、ROS 版本、认证配置等详细信息。

`GET /api/v1/system/info` 返回版本和兼容信息：

```json
{
  "product_version": "1.0.0",
  "backend_build_id": "20260916.1",
  "api_version": "1.0",
  "event_version": "1.0",
  "backend_instance_id": "01J..."
}
```

Remote 模式下 `system/info` 需要正常认证；Local Desktop 使用 session token。

Frontend 自身编译进：

```text
ui_build_id
ui_product_version
supported_api_range
supported_event_range
```

Backend 不返回 `ui_build_id`。

### 4.3 WebSocket Event Contract

统一 Envelope：

```json
{
  "version": "1.0",
  "stream_id": "01JSTREAM...",
  "sequence": 1234,
  "event": "robot.state.updated",
  "timestamp": "2026-09-16T07:30:12.125Z",
  "data": {}
}
```

V1 采用最简单的多客户端模型：

- 每个 WebSocket 连接拥有独立 `stream_id`；
- 每个连接的 `sequence` 独立递增；
- Robot State 的 `state_sequence` 来自全局 StateStore，因此所有客户端观察到同一状态版本；
- Backend 向已授权客户端广播其可见事件；
- V1 不设计 per-client topic subscription/filter protocol；
- 断线重连后重新读取 REST snapshot，不依赖 WebSocket 历史回放。

Robot State 事件：

```json
{
  "version": "1.0",
  "stream_id": "01JSTREAM...",
  "sequence": 23,
  "event": "robot.state.updated",
  "timestamp": "2026-09-16T07:30:12.125Z",
  "data": {
    "state_sequence": 84721,
    "stale": false,
    "state": {}
  }
}
```

Event version 使用 `major.minor`：新增可选字段属于兼容 minor 变更；删除/重命名字段、改变类型或语义需要增加 major。

### 4.4 Robot State

通用元数据：

```json
{
  "state_sequence": 84721,
  "timestamp": "2026-09-16T07:30:12.125Z",
  "stale": false,
  "state": {}
}
```

`state` 内容由产品定义。

要求：

- StateStore 返回一致 snapshot，不允许 torn state；
- `state_sequence` 与 WebSocket sequence 无关；
- 高频中间样本允许覆盖；
- `stale` 根据产品 freshness threshold 判断；
- StateStore 不是历史 Recorder。

### 4.5 Task 与 Recovery

Task 生命周期：

```text
pending
running
succeeded
failed
cancelling
cancelled
interrupted
unknown
```

简化语义：

- `interrupted`：已确认原 Task 上下文不能继续，需要产品恢复流程确认机器人状态；
- `unknown`：无法确认机器人侧执行是否仍存在；
- 两者默认都阻止新的 CONTROL/MAINTENANCE 和应用升级，直到产品恢复流程明确解除。

Task：

```json
{
  "task_id": "01J...",
  "type": "calibration",
  "state": "running",
  "progress": 0.42,
  "execution": {
    "kind": "ros_action",
    "id": "7a9f..."
  },
  "started_at": "2026-09-16T07:30:00Z",
  "updated_at": "2026-09-16T07:30:12Z",
  "result": null,
  "error": null
}
```

其中：

- `type` 是产品定义的稳定字符串，仅用于查询、显示、日志和审计；
- `execution` 是 robot-side `execution_handle`；
- `result` 由具体产品定义，可通过产品专用 Pydantic Schema 强类型化；
- generic Task 层只保存/返回结果，不解释产品业务内容。

#### execution_handle

架构层把 `execution_handle` 定义为由具体 Robot Application 创建和解释的 **opaque robot-side execution identifier**。V1 的最小公共形态为：

```text
execution_handle
├── kind
└── id
```

要求：

- 在当前 **Robot Control Endpoint** 的恢复上下文中能够唯一定位一次机器人侧执行；
- 必须可以持久化；
- 声称支持 Backend restart reconciliation 的长任务，必须能使用该 handle 再次查询机器人执行端；
- 架构层不预设通用 `controller_id`、`action_server_id` 等字段；
- 多 Controller 或更复杂产品如果单靠 `kind + id` 无法唯一定位，可在**产品自己的 execution handle** 中增加 controller/endpoint 等必要信息，而不扩张全局通用 Schema。

例如 ROS Action 产品可以使用：

```text
kind = ros_action
id   = goal_uuid
```

#### 正常查询与重启恢复

正常运行期间：

```text
GET /api/v1/tasks/{task_id}
        ↓
Backend TaskManager / SQLite
        +
C++ task_status(task_id)（如需要刷新运行态）
        ↓
Task View
```

Backend 重启时才执行 reconciliation：

```text
读取 non-terminal Task
      ↓
读取 execution_handle
      ↓
reconcile_task(execution_handle)
      ↓
confirmed_running  → 更新为 running/cancelling
confirmed_terminal → 恢复真实 terminal state/result
not_found          → 按产品规则映射 interrupted / unknown
indeterminate      → unknown
      ↓
决定 ready_for_control / ready_read_only
```

如果底层协议不能通过 execution handle 查询旧任务，则 Backend 崩溃后不能假装恢复该任务，只能按产品规则进入 `interrupted` 或 `unknown`。

机器人执行端是真实执行状态的最终来源，SQLite 不是。

本架构不定义通用 `/recovery/...` API。`unknown/interrupted` 的解除由具体产品设计显式恢复流程，并必须形成 Audit 记录；不能仅修改本地数据库状态后直接放行控制。

### 4.6 API Error Model

使用：

```text
HTTP Status
+ Problem Details
+ stable string error code
+ request_id
```

示例：

```json
{
  "type": "urn:robot-debugger:error:robot-not-enabled",
  "title": "Robot is not enabled",
  "status": 409,
  "detail": "Motion command requires the robot to be enabled.",
  "code": "ROBOT_NOT_ENABLED",
  "request_id": "7f3b...",
  "context": {
    "operation": "motion.jog",
    "task_id": null,
    "resource": "joint-1"
  }
}
```

`context` 只固定少量可选字段：

```text
operation
 task_id
resource
```

不把它设计成任意无类型 payload。

基础错误码：

```text
INVALID_ARGUMENT
VALIDATION_FAILED
UNAUTHENTICATED
PERMISSION_DENIED
RESOURCE_NOT_FOUND

ROBOT_NOT_CONNECTED
ROBOT_NOT_POWERED
ROBOT_NOT_ENABLED
ROBOT_BUSY
ROBOT_STATE_CONFLICT

OPERATION_NOT_SUPPORTED
PARAMETER_OUT_OF_RANGE
OPERATION_REJECTED
OPERATION_TIMEOUT
OPERATION_CANCELLED
OPERATION_FAILED

ROS_UNAVAILABLE
SYSTEM_UNAVAILABLE
INTERNAL_ERROR
```

推荐 HTTP 映射：

| HTTP | 场景 |
| --- | --- |
| 400 | `INVALID_ARGUMENT` |
| 401 | `UNAUTHENTICATED` |
| 403 | `PERMISSION_DENIED` |
| 404 | `RESOURCE_NOT_FOUND` |
| 409 | `ROBOT_BUSY`、`ROBOT_STATE_CONFLICT`、`ROBOT_NOT_CONNECTED`、`ROBOT_NOT_POWERED`、`ROBOT_NOT_ENABLED`、`OPERATION_REJECTED` |
| 422 | `VALIDATION_FAILED`、`PARAMETER_OUT_OF_RANGE` |
| 500 | `INTERNAL_ERROR` |
| 503 | `ROS_UNAVAILABLE`、`SYSTEM_UNAVAILABLE` |
| 504 | `OPERATION_TIMEOUT` |

`ROBOT_NOT_CONNECTED` 表示机器人当前业务状态未连接，保留 409；底层 ROS/DDS/依赖不可用使用 503。

机器人硬件 Fault Code 属于 Diagnostics 数据，不直接映射为 HTTP 500。

### 4.7 固件 Artifact

不提供用户上传 firmware 文件或选择任意本地路径。

固件来自受控来源：

```text
AppImage/.deb 随产品发布
企业 Artifact 仓库受控同步
运维预部署固定目录
```

UI 只选择 `artifact_id`，不访问本地路径。

最小 Catalog：

```json
{
  "artifact_id": "motor-fw-1.4.2",
  "version": "1.4.2",
  "target": "motor",
  "sha256": "..."
}
```

Backend/C++ 在升级前校验存在性、hash/signature、目标兼容性和机器人安全状态。

权限：

- 查看 Catalog：`robot.read`；
- 执行固件升级：`robot.maintain`。

Artifact 读取和传输由 Backend/C++ 内部完成，不增加通用文件下载 API。

### 4.8 应用升级准备

`GET /api/v1/system/upgrade-readiness` 用于展示当前 blockers：

```json
{
  "safe": false,
  "blockers": [
    {"code": "UNKNOWN_TASK", "task_id": "01J..."}
  ]
}
```

为了避免读取 readiness 后又产生新控制操作，真正开始升级前必须调用：

```text
POST /api/v1/system/prepare-upgrade
```

该操作在 Backend 内部原子执行：

```text
检查当前 blockers
      ↓
存在 blocker → 拒绝，phase 不变
      ↓
无 blocker
      ↓
phase = preparing_upgrade
      ↓
拒绝新的 CONTROL / MAINTENANCE / new Task
```

`preparing_upgrade` 不引入额外 Upgrade Lease、TTL、readiness token 或 Cancel 协议。它只属于当前 Backend 进程：

```text
ready_for_control
      ↓ POST prepare-upgrade
preparing_upgrade
      ├── 正常 → 停止 Backend → 启动新版本
      └── 放弃/失败 → 重启 Backend
                         ↓
                      starting
                         ↓
                      preflight
                         ↓
                      recovering
```

约束：

- `preparing_upgrade` 不持久化到 SQLite；
- Backend 重启后永远从 `starting` 开始，不能恢复为 `preparing_upgrade`；
- 推荐升级器先完成新 AppImage 的下载、SHA-256/签名和 Host baseline 校验，再调用 `prepare-upgrade`，使升级封闭窗口尽可能短；
- 进入 `preparing_upgrade` 后仍允许必要的只读请求，例如 health、状态和 Task 查询，但禁止新的副作用操作；
- 升级器不得为了完成软件升级自动发送 robot stop、cancel、power-off 等机器人控制命令。

权限：

- 查看 upgrade readiness：`robot.read`；
- `prepare-upgrade`：`system.admin`；
- Local Desktop updater 可使用受信任的 system actor 身份。

---

## 5. 运行、安全与可观测性

### 5.1 Local Desktop 安全模型

Local Backend：

```text
127.0.0.1
random TCP port
ephemeral session token
strict Origin
CSP
local trusted web/dist
```

token：

- 每次 Backend 启动生成；
- 生命周期等于 Backend 进程生命周期；
- 不写配置文件；
- 不通过 command-line argument 传递；
- 使用 inherited pipe/stdin 等一次性 bootstrap channel；
- 不设计 refresh token。

Electron：

```text
nodeIntegration = false
contextIsolation = true
sandbox = true（兼容条件允许时）
```

Preload 只暴露必要窄接口，不提供通用 shell、文件系统或任意 IPC 能力。

### 5.2 Remote Web 安全模型

Remote Backend 的架构基线：

- HTTPS/WSS；
- OIDC/OAuth 2.0 + PKCE；
- Backend 校验 token 和 scope；
- CORS 使用明确 allowlist；
- WebSocket 校验 Origin；
- Access Token 不放 URL/query；
- 静态资源使用 hash 文件名，`index.html` 不做长期缓存；
- UI 启动检查 API/Event 版本兼容。

JWT/JWKS 刷新、Cookie/CSRF、Token refresh 等细节放到具体安全设计，不在系统架构层继续展开。

### 5.3 Authorization 与操作安全

基础 Scope：

```text
robot.read
robot.control
robot.maintain
system.admin
```

Backend 是最终授权点；前端按钮隐藏/禁用只用于 UX。

有副作用操作依次经过：

```text
Authentication
    ↓
Authorization
    ↓
Backend phase
    ↓
Product State Preconditions
    ↓
OperationCoordinator
    ↓
Robot Controller final acceptance
```

例如：

```text
motion
  → robot.control
  → ready_for_control
  → connected + powered + enabled
  → no conflicting operation
  → controller accepts

firmware
  → robot.maintain
  → ready_for_control
  → safe robot state
  → no conflicting operation
  → valid artifact
  → controller accepts
```

前端不得自动重试上电、使能、Jog、运动、标定和固件升级等副作用操作。

### 5.4 日志与审计

各层使用自身成熟日志：

- Electron：`electron-log`；
- Python：标准 `logging`；
- C++：`rclcpp/rcutils` logging。

统一关联字段：

```text
timestamp
level
component
event
request_id
task_id
backend_instance_id
actor
operation
message
```

Runtime Log 记录启动、连接变化、异常、Task 生命周期和 recovery；Operation/Audit Log 记录上电、参数写入、运动、标定、清错、固件升级和人工恢复动作。

禁止记录 Access Token、私钥、500 Hz 每帧状态和不必要的大 payload。

日志目录遵循 XDG：

```text
$XDG_STATE_HOME/robot-debugger/logs
```

SQLite 只保存必要的 Task/Audit 索引和恢复元数据。

### 5.5 Preflight 与性能

Preflight 至少检查：

- OS/Architecture；
- ROS 2 / RMW/DDS baseline；
- Native library；
- pybind11 module 或 Robot Service 可用性；
- SQLite/配置目录；
- 必需设备权限；
- 产品选择的 Variant 与 Safety Admission 记录。

性能基线：

- ROS 内部状态可为 500 Hz 或更高；
- C++ StateStore 保存最新一致状态；
- UI state 默认 30 Hz，可配置到约 60 Hz；
- 中间状态允许 coalesce/drop；
- Task result、Fault transition、Connection transition 不因 UI 节流丢失；
- 高频 Task progress 不写 SQLite。

如果未来需要示波、曲线、数据记录，再独立设计 Telemetry Recorder，不扩张当前 UI state stream。

---

## 6. 构建、发布、安装与升级

### 6.1 构建与测试

各技术栈使用自身工具：

```text
frontend → pnpm + Vite + Electron build
backend  → Python dependency/runtime assembly
robot    → CMake + C++17
```

根工程只提供轻量编排脚本：

```text
scripts/dev.sh
scripts/build.sh
scripts/test.sh
scripts/package.sh
```

推荐构建顺序：

```text
1. C++ Robot Application + pybind11 / Robot Service
2. Backend runtime
3. frontend/web
4. frontend/desktop
5. integration / contract tests
6. AppImage / .deb
```

重点测试：

- C++ 单元测试；
- Backend API 测试；
- Vue 关键 Store/API 测试；
- REST/WebSocket contract test；
- OperationCoordinator 并发测试；
- Task crash/recovery 测试；
- Safety Admission fault-injection；
- multi-client；
- control ownership conflict；
- AppImage/.deb smoke test；
- Host Runtime baseline test。

不追求为了覆盖率数字堆积无价值测试。

### 6.2 发布产物

Linux Desktop：

> **AppImage 为主，`.deb` 为辅。**

```text
RobotDebugger-1.0.0-x86_64.AppImage
robot-debugger_1.0.0_amd64.deb
```

AppImage 包含应用自身运行时：

```text
Electron
frontend/web/dist
FastAPI Backend
Python 3.12 runtime + dependencies
pybind11 extension / Robot Service client
Robot Debugger C++ libraries
SQLite schema/migrations
默认配置
可选受控固件 Artifact
```

Host 仍需满足产品声明的：

```text
OS / Architecture
ROS 2
RMW/DDS
系统 ABI
Driver / Device permission
Robot Firmware baseline（如适用）
```

因此 AppImage 的“无需安装”只表示 Robot Debugger 应用本身无需安装，不表示 Host 零依赖。

`.deb` 用于固定安装、Desktop Entry、系统依赖声明、批量部署和 apt 管理。

### 6.3 Product Version 与兼容性

整个产品使用一个 Product Version，并冻结：

```text
Web Build
Electron
FastAPI Backend
Python dependency set
C++ Robot Application
pybind11 / Robot Service
SQLite schema
API/Event version
Host Runtime baseline
```

Frontend 编译进：

```text
ui_build_id
ui_product_version
supported_api_range
supported_event_range
```

Backend 返回：

```text
product_version
backend_build_id
api_version
event_version
backend_instance_id
```

Local Desktop 整包原则上版本一致；Remote Web 必须做 API/Event 兼容检查。

### 6.4 Host Runtime Compatibility Matrix

每个正式版本维护经过测试的支持矩阵，不声明未经验证的“理论兼容”：

```text
Architecture
OS
ROS 2
RMW/DDS
glibc / libstdc++ ABI
Kernel/Driver（如需要）
Robot Firmware（如需要）
```

启动 preflight 与 Release CI/QA 使用同一 baseline。

### 6.5 AppImage 升级与回滚

V1 不做后台静默升级。

推荐流程：

```text
检查新版本
   ↓
下载新的 AppImage
   ↓
验证 SHA-256 / 发布签名 / Host baseline
   ↓
GET upgrade-readiness
   ↓
存在 blockers → 提示用户处理，停止升级
   ↓
POST prepare-upgrade
   ↓
Backend 原子进入 preparing_upgrade
并拒绝新的 CONTROL/MAINTENANCE
   ↓
正常停止 Backend / Robot Service（按产品部署方式）
   ↓
退出旧版本
   ↓
启动新版本
   ↓
preflight + health + version check
```

升级器不得为了升级自动发送机器人 stop、cancel、power-off 等命令。

以下状态默认阻止升级：

```text
recovering
active control
running/cancelling task
unknown/interrupted task
robot state stale/unsafe
control owner conflict
```

回滚主要依靠保留上一版本 AppImage。SQLite migration 如果不可逆，必须在发布说明中明确限制旧版本回滚。

### 6.6 固件 Artifact 更新

固件 Artifact 更新不是普通 UI 文件上传。

允许：

```text
随 AppImage/.deb 发布
企业 Artifact 仓库受控同步
运维预部署固定目录
```

普通 UI 只看到 Artifact Catalog，不看到任意本地路径。

### 6.7 后续演进边界

只有出现明确需求时，再评估：

```text
gRPC
标准化 Unix Domain Socket protocol
分布式 control ownership protocol
AsyncAPI
Telemetry Recorder
外部 observability stack
```

不得为了“架构更完整”提前引入这些基础设施。

---

## 架构基线总结

```text
Product
────────────────────────────────
Linux
Desktop-first
Web-secondary

Frontend
────────────────────────────────
Vue 3 + TypeScript + Vite
Vue Router + Pinia + Element Plus
pnpm frontend-only Workspace
Electron Desktop Shell

Backend
────────────────────────────────
Python 3.12
FastAPI + Pydantic v2 + Uvicorn
SQLite（仅轻量 Task/Audit/Recovery 元数据）

Robot
────────────────────────────────
C++17
ROS 2 / rclcpp
StateStore
TaskStore
OperationCoordinator

V1 Variant A
────────────────────────────────
FastAPI → pybind11 → C++ Robot Application
默认优先

V1 Variant B
────────────────────────────────
FastAPI → IPC/RPC Adapter → Independent Robot Service
仅明确需要进程隔离时采用
与 Variant A 复用同一 Robot Application / Facade

Safety
────────────────────────────────
Debugger 不是最终安全执行体
最终 watchdog / fail-safe 位于 Robot Controller / Runtime
正式产品必须通过 Safety Admission

Control Topology
────────────────────────────────
一个 Robot Control Endpoint
一个 Authoritative Backend
多个 UI Client

Communication
────────────────────────────────
REST + WebSocket + JSON
OpenAPI + JSON Schema
UI State 30~60 Hz
高频 ROS 数据停留在 C++

Recovery
────────────────────────────────
Backend phase:
starting → preflight → recovering
→ ready_for_control / ready_read_only

Task:
调试工具 task_id
+ opaque robot-side execution_handle

unknown/interrupted 默认阻止新的控制/维护

Upgrade
────────────────────────────────
AppImage 主
.deb 辅
先下载/验证，再 prepare-upgrade
preparing_upgrade 阻止新的控制/维护
该 phase 不跨 Backend 重启恢复
显式升级、易回滚
```

本架构的核心取舍是：**把机器人高频通信和执行适配留给 C++，把 Web 开发效率交给 FastAPI/Vue，把 Desktop 能力限制在 Electron 壳层；只保留对安全、恢复和发布真正必要的机制，不为潜在需求提前建设复杂基础设施。**
