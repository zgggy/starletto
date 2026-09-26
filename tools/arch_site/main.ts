// STARLETTO://ARCHITECTURE — 浏览器运行时（TS 源码）
// 语法约束（供 build.ts 的确定性类型剥离器处理）：
//   不用三元表达式、不用 as 断言、注解类型不含 Record/泛型表达式、
//   无 enum/namespace/非空断言、无 export、无 switch

interface GateInfo {
  name: string
  desc: string
}

interface LayerInfo {
  id: string
  title: string
  sub: string
  desc: string
  points: string[]
}

interface BoxDef {
  id: string
  x: number
  y: number
  w: number
  h: number
  title: string
  sub: string
  extra: string
}

interface FlowNode {
  id: string
  label: string
  sub: string
  x: number
  y: number
  w: number
  h: number
}

interface FlowEdge {
  id: string
  kind: string
  num: string
  name: string
  lx: number
  ly: number
  path: string
  packets: number
}

interface PacketState {
  el: SVGPathElement
  circle: SVGCircleElement
  len: number
  off: number
  speed: number
}

interface DecRow {
  topic: string
  choice: string
  why: string
}

interface MarkerMap {
  inproc: string
  shm: string
  rec: string
}

// ── 数据 ──────────────────────────────────────────────

const GATES: GateInfo[] = [
  { name: 'build', desc: 'Meson/Ninja 编译 native_runner 与 4 个插件，当前 0 错误 0 警告' },
  { name: 'schema-clean', desc: '校验 .s.h 生成物没有回到源码树——codegen 走 custom_target 输出 build 目录，防止源/生成物再次分叉' },
  { name: 'schema-lint', desc: '全量 .schema 结构检查：字段 id 重复/非法、字段名重复、root 存在性' },
  { name: 'schema-compat', desc: '以 git HEAD 为基线的演进兼容检查：改字段 id、类型互换、删字段即 FAIL（spec §8 演进禁令）' },
  { name: 'unit-test', desc: 'meson test：17 套 112 用例全绿（transport 48 / schema 14 / runtime 50）' },
  { name: 'selftest', desc: 'transport 自检：service soak 200 次并发、多进程 shm 覆写、routed 不重复投递、降级路径' },
  { name: 'doctor ·control', desc: 'control 模块清单诊断：conf 可解析、通道/类型一致' },
  { name: 'doctor ·planning', desc: 'planning 模块清单诊断' },
  { name: 'doctor ·setting', desc: 'setting 模块清单诊断' },
  { name: 'list', desc: '组件列表输出正常——当前 4 条真实组件，无假配置' },
]

const LAYERS: LayerInfo[] = [
  {
    id: 'star', title: '工具链 star', sub: 'bash · 844 行统一入口',
    desc: '封装 Meson/Ninja 与运行时工具的统一入口。check 子命令是 <b>10 项 CI 门禁</b>的入口，每笔提交必须全过。',
    points: ['build / run / install / clean', 'list / graph / doctor --deep', 'monitor / record / replay / param', 'check = 10/10 PASS'],
  },
  {
    id: 'apps', title: 'apps', sub: '宿主与工具 · 纯插件装配',
    desc: '宿主程序<b>不含任何组件实现</b>：按 component.conf 声明 dlopen 插件。app.h 拆分后仅 48 行声明，实现收敛到 app.cc。',
    points: ['native_runner：单进程 / 多进程编排', 'doctor --deep 校验插件可 dlopen/dlsym', 'graph --components 输出真实拓扑', 'monitor 输出每通道 p99 延迟'],
  },
  {
    id: 'modules', title: 'modules', sub: '业务组件 · conf 声明 + 插件交付',
    desc: '每个组件由 conf 声明（读者/写者/触发器）并编译为独立 <b>lib*.so</b> 插件。4 个真实组件构成 setting→planning→control 闭环。',
    points: ['setting：Config + typed ParameterService', 'planning：Motion（SyncTrigger 双路同步）', 'control：PID（数据驱动触发）', '假配置已清：star list 4/4 真实'],
  },
  {
    id: 'runtime', title: 'basement/runtime', sub: '组件模型 · 调度 · 进程编排',
    desc: '组件生命周期（init/process_once/shutdown）与依赖注入。RuntimeContext 聚合 7 个系统能力；调度层统计逃逸异常（failed_invocations）。',
    points: ['ComponentRegistry + dlopen 插件', 'NativeScheduler：优先级 + 帧一致性', 'SyncTrigger / TimerTrigger（回调锁外派发）', 'ProcessOrchestrator：fork/exec + 监督'],
  },
  {
    id: 'transport', title: 'basement/transport', sub: 'inproc / shm / routed 三通道',
    desc: '零拷贝通信层。单进程 TypedConstPtr 指针直传（零序列化）；跨进程 4 槽环形 loan/view，读侧 <b>seqlock 复核</b>防撕裂；Routed 自动选路、失败降级。',
    points: ['QoS：depth / KEEP_LAST / DROP_NEWEST', 'DeliveryWriter：future 投递确认', '每通道 p99/p999 延迟 + 丢帧统计', 'STARREC 录制回放'],
  },
  {
    id: 'schema', title: 'basement/schema', sub: '.schema IDL · 唯一类型源',
    desc: '自研 IDL 替代 protobuf（零拷贝硬约束）。custom_target 把 .schema→.s.h 变成显式构建依赖，生成物不入库——<b>结构性消除漂移</b>。',
    points: ['16 个 .schema，清单即类型', 'BinaryCodec：unknown field 保留', 'schema-compat 以 git HEAD 为基线', 'FixedLayoutCodec 服务于 shm loan/view'],
  },
  {
    id: 'core', title: 'basement/core', sub: '基础设施',
    desc: '三类异常语义已区分（不变式破坏 / 调用非法 / 运行时状况）；futex_compat 让 shm 同步在非 Linux 平台退化为短轮询。',
    points: ['LoggerManager：多 sink 可切换', 'TypedConstPtr：带 typeid 的类型擦除', 'FdGuard / IClock 抽象', '5 类告警已提为 -Werror'],
  },
  {
    id: 'libraries', title: 'libraries', sub: '算法资产 · 整层未接线',
    desc: '约 <b>3500 行</b>真实算法（A*/Hybrid A*、piecewise jerk、Frenet/ST 图、行为树框架），等 localization/perception 等路线图模块落地时接线。',
    points: ['element：领域类型 1030 行', 'method：规划算法 900 行（依赖 OSQP）', 'machine：行为树框架 699 行', 'common：数学/黑板 1026 行'],
  },
  {
    id: 'tests', title: 'tests', sub: '17 套 · 112 用例',
    desc: '自研 Catch2 风格框架（离线替代方案）。transport 48 / schema 14 / runtime 50，含撕裂读拒绝、多 DSO id 不碰撞、重入死锁等回归锁。',
    points: ['并发压力：torn=0 / 拒绝路径必达', 'service 并发 1760 次零串台', '真实 fork/exec 进程编排', '录放逐条保真'],
  },
]

const DECISIONS: DecRow[] = [
  { topic: '构建系统', choice: 'Meson + Ninja', why: 'custom_target 声明 codegen 依赖、跨平台；生成物不入库防漂移' },
  { topic: 'IDL 格式', choice: '自研 .schema', why: '零拷贝硬约束——protobuf 不满足 inproc 指针 + shm loan/view 的统一抽象' },
  { topic: '组件交付', choice: 'conf 声明 + dlopen 插件', why: 'apps 与组件解耦；doctor --deep 校验插件符号；入口 star_register_components(ComponentRegistry*)' },
  { topic: '错误模型', choice: '状态码 / 异常 分治', why: '运行时状况（transport 失败）返回 WriteResult；编程错误（端口未声明）抛异常——消除状态码被异常化又被降级为日志的反转链' },
  { topic: '代码组织', choice: 'Header-only（库层）', why: '用户偏好 .h 组织；apps 例外——app.h 已拆 app.cc，无 header-only 理由' },
  { topic: '日志系统', choice: '自定义宏 + LoggerManager', why: '零依赖、多 sink 可切换、禁用级别编译期消除' },
  { topic: '进程编排', choice: 'fork/exec + 参数传递', why: '简单可靠、无需守护进程；wait_all 监督退出码' },
]

const FLOW_NODES: FlowNode[] = [
  { id: 'config', label: 'ConfigComponent', sub: 'libconfig.so · 读 files/*.conf', x: 52, y: 112, w: 244, h: 64 },
  { id: 'param', label: 'ParameterComponent', sub: 'libparameter.so · +3 个参数 service', x: 52, y: 198, w: 244, h: 64 },
  { id: 'motion', label: 'MotionComponent', sub: 'libmotion.so · SyncTrigger 双路同步', x: 480, y: 128, w: 244, h: 64 },
  { id: 'pid', label: 'ControlComponent', sub: 'libpid.so · 数据驱动触发', x: 884, y: 128, w: 252, h: 64 },
  { id: 'rec', label: 'STARREC 录制', sub: 'star replay · 时间戳回放', x: 884, y: 300, w: 252, h: 56 },
]

const FLOW_EDGES: FlowEdge[] = [
  { id: 'e1', kind: 'inproc', num: '①', name: 'setting.config.snapshot', lx: 330, ly: 96, path: 'M 296 144 C 380 144 400 150 480 154', packets: 2 },
  { id: 'e2', kind: 'inproc', num: '②', name: 'setting.parameter.snapshot', lx: 322, ly: 254, path: 'M 296 230 C 390 230 410 186 480 178', packets: 2 },
  { id: 'e3', kind: 'shm', num: '③', name: 'planning.trajectory', lx: 762, ly: 96, path: 'M 724 154 C 790 154 820 152 884 154', packets: 3 },
  { id: 'e4', kind: 'shm', num: '④', name: 'control.command', lx: 1022, ly: 240, path: 'M 1010 192 C 1010 240 1010 260 1010 300', packets: 2 },
  { id: 'e5', kind: 'rec', num: '', name: 'record = true', lx: 668, ly: 344, path: 'M 602 192 C 602 300 800 330 884 326', packets: 1 },
]

const BADGES: string[][] = [
  ['ok', 'P0–P2 CLOSED', '审计问题全闭环'],
  ['cy', '17 SUITES · 112 TESTS', '全绿'],
  ['ok', '10/10 GATES', 'star check'],
  ['vi', '4 PLUGINS', 'lib*.so 纯插件装配'],
  ['ok', '0 WARNINGS', '5 类 -Werror 门禁'],
]

const STATS: number[][] = [
  [112, 'TEST CASES', '用例'],
  [17, 'SUITES', '测试套件'],
  [10, 'CI GATES', '门禁'],
  [4, 'PLUGINS', '插件'],
  [16, 'SCHEMA TYPES', '类型源'],
]

// ── 工具 ──────────────────────────────────────────────

function $id(id: string) {
  return document.getElementById(id)
}

function esc(s: string) {
  return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
}

// ── hero / stats ──────────────────────────────────────

function renderBadgesHtml() {
  let html = ''
  for (const b of BADGES) {
    html = html + '<div class="badge ' + b[0] + '" title="' + b[2] + '"><i></i><b>' + b[1] + '</b></div>'
  }
  return html
}

function renderStatsHtml() {
  let html = ''
  for (const s of STATS) {
    html = html + '<div class="stat"><div class="num" data-n="' + s[0] + '">0</div><div class="lbl">' + s[1] + ' · ' + s[2] + '</div></div>'
  }
  return html
}

function animateStats() {
  const nums = document.querySelectorAll('.stat .num')
  for (let i = 0; i < nums.length; i++) {
    const el = nums[i]
    const target = parseInt(el.getAttribute('data-n'), 10)
    const t0 = performance.now()
    const dur = 900
    const step = function(now: number) {
      const t = (now - t0) / dur
      if (t >= 1) {
        el.textContent = String(target)
      } else {
        const eased = 1 - (1 - t) * (1 - t) * (1 - t)
        el.textContent = String(Math.round(target * eased))
        requestAnimationFrame(step)
      }
    }
    requestAnimationFrame(step)
  }
}

// ── 01 layers ─────────────────────────────────────────

const LAYER_BOXES: BoxDef[] = [
  { id: 'star', x: 20, y: 16, w: 820, h: 60, title: 'star', sub: 'bash 统一入口 · 10 项 CI 门禁 · monitor / doctor / graph', extra: '' },
  { id: 'apps', x: 20, y: 96, w: 820, h: 64, title: 'apps', sub: '宿主与工具 · 纯插件装配（不含组件实现）', extra: '' },
  { id: 'modules', x: 20, y: 180, w: 820, h: 60, title: 'modules', sub: '业务组件 · conf 声明 + 插件交付', extra: '' },
  { id: 'runtime', x: 20, y: 312, w: 820, h: 76, title: 'basement / runtime', sub: '组件模型 · 调度器 · 触发器 · 进程编排 · 健康观测', extra: '' },
  { id: 'transport', x: 20, y: 404, w: 820, h: 84, title: 'basement / transport', sub: 'Native inproc · Shm 4 槽环形 + seqlock · Routed 自动选路', extra: '' },
  { id: 'schema', x: 20, y: 504, w: 820, h: 76, title: 'basement / schema', sub: '.schema IDL · codegen(custom_target) · BinaryCodec · FixedLayoutCodec', extra: '' },
  { id: 'core', x: 20, y: 596, w: 820, h: 76, title: 'basement / core', sub: '日志 · 异常 · 时钟 · TypedConstPtr · futex_compat', extra: '' },
  { id: 'libraries', x: 880, y: 96, w: 280, h: 130, title: 'libraries', sub: '算法资产 · 整层未接线', extra: '≈3500 行 · 接线前置: OSQP' },
  { id: 'tests', x: 880, y: 246, w: 280, h: 130, title: 'tests', sub: '17 套 · 112 用例', extra: 'transport 48 · schema 14 · runtime 50' },
]

function layerRect(b: BoxDef) {
  let s = '<g class="layer-box" data-layer="' + b.id + '">'
  s = s + '<rect class="boxrect" x="' + b.x + '" y="' + b.y + '" width="' + b.w + '" height="' + b.h + '" rx="2"/>'
  s = s + '<text class="btitle" x="' + (b.x + 16) + '" y="' + (b.y + 27) + '">' + b.title + '</text>'
  s = s + '<text class="bsub" x="' + (b.x + 16) + '" y="' + (b.y + 46) + '">' + b.sub + '</text>'
  if (b.extra !== '') {
    s = s + '<text class="bsub" x="' + (b.x + 16) + '" y="' + (b.y + 64) + '" fill="#22d3ee" opacity="0.85">' + b.extra + '</text>'
  }
  s = s + '</g>'
  return s
}

function renderLayersSvg() {
  let svg = '<svg viewBox="0 0 1180 692" xmlns="http://www.w3.org/2000/svg">'
  svg = svg + '<defs>'
  svg = svg + '<marker id="arrowhead" markerWidth="7" markerHeight="7" refX="6" refY="3.5" orient="auto"><path d="M0,0 L7,3.5 L0,7" fill="none" stroke="#22d3ee" stroke-width="1.2"/></marker>'
  svg = svg + '</defs>'

  svg = svg + '<path class="dep-edge" data-dep="d0" d="M 430 76 L 430 92"/>'
  svg = svg + '<path class="dep-edge" data-dep="d1" d="M 430 160 L 430 176"/>'
  svg = svg + '<path class="dep-edge" data-dep="d2" d="M 430 240 L 430 308"/>'
  svg = svg + '<path class="dep-edge" data-dep="d3" d="M 430 388 L 430 400"/>'
  svg = svg + '<path class="dep-edge" data-dep="d4" d="M 430 488 L 430 500"/>'
  svg = svg + '<path class="dep-edge" data-dep="d5" d="M 430 580 L 430 592"/>'
  svg = svg + '<path class="dep-edge soft" data-dep="d6" d="M 1020 226 L 1020 542 L 844 542"/>'
  svg = svg + '<path class="dep-edge soft" data-dep="d7" d="M 1020 376 L 1020 390 L 848 390"/>'
  svg = svg + '<path class="dep-edge soft" data-dep="d8" d="M 1166 76 L 1166 242"/>'

  for (const b of LAYER_BOXES) {
    svg = svg + layerRect(b)
  }
  svg = svg + '</svg>'

  return svg
}

// ── 02 data flow ──────────────────────────────────────

const FLOW_GROUPS: string[][] = [
  ['24', '64', '300', '226', 'PROCESS GROUP · setting'],
  ['452', '64', '300', '156', 'PROCESS GROUP · planning'],
  ['856', '64', '308', '156', 'PROCESS GROUP · control'],
]

const MARKERS: MarkerMap = {}
MARKERS.inproc = '#22d3ee'
MARKERS.shm = '#34d399'
MARKERS.rec = '#a78bfa'

function renderFlowSvg() {
  let svg = '<svg viewBox="0 0 1200 400" xmlns="http://www.w3.org/2000/svg">'
  svg = svg + '<defs>'
  for (const kind in MARKERS) {
    svg = svg + '<marker id="farrow-' + kind + '" markerWidth="7" markerHeight="7" refX="6" refY="3.5" orient="auto"><path d="M0,0 L7,3.5 L0,7" fill="none" stroke="' + MARKERS[kind] + '" stroke-width="1.2"/></marker>'
  }
  svg = svg + '</defs>'

  for (const grp of FLOW_GROUPS) {
    svg = svg + '<rect class="fgroup" x="' + grp[0] + '" y="' + grp[1] + '" width="' + grp[2] + '" height="' + grp[3] + '" rx="2"/>'
    svg = svg + '<text class="fgroup-label" x="' + (Number(grp[0]) + 14) + '" y="' + (Number(grp[1]) + 24) + '">' + grp[4] + '</text>'
  }

  for (const e of FLOW_EDGES) {
    svg = svg + '<path id="' + e.id + '" class="fedge ' + e.kind + '" data-kind="' + e.kind + '" d="' + e.path + '" marker-end="url(#farrow-' + e.kind + ')"/>'
    let labelHtml = ''
    if (e.num !== '') {
      labelHtml = '<tspan fill="' + MARKERS[e.kind] + '">' + e.num + '</tspan><tspan class="chn"> ' + e.name + '</tspan>'
    } else {
      labelHtml = '<tspan fill="' + MARKERS[e.kind] + '">' + e.name + '</tspan>'
    }
    svg = svg + '<text class="elabel" x="' + e.lx + '" y="' + e.ly + '">' + labelHtml + '</text>'
  }

  for (const n of FLOW_NODES) {
    svg = svg + '<g class="fnode" data-node="' + n.id + '">'
    if (n.id === 'rec') {
      svg = svg + '<rect x="' + n.x + '" y="' + n.y + '" width="' + n.w + '" height="' + n.h + '" rx="2" fill="rgba(167,139,250,0.06)" stroke="#a78bfa" stroke-dasharray="4 3"/>'
    } else {
      svg = svg + '<rect x="' + n.x + '" y="' + n.y + '" width="' + n.w + '" height="' + n.h + '" rx="2"/>'
    }
    svg = svg + '<text class="nlabel" x="' + (n.x + 16) + '" y="' + (n.y + 27) + '">' + n.label + '</text>'
    svg = svg + '<text class="nsub" x="' + (n.x + 16) + '" y="' + (n.y + 46) + '">' + n.sub + '</text>'
    svg = svg + '</g>'
  }

  for (const e of FLOW_EDGES) {
    let cls = 'packet'
    if (e.kind === 'shm') {
      cls = 'packet g'
    }
    for (let i = 0; i < e.packets; i++) {
      svg = svg + '<circle class="' + cls + '" data-edge="' + e.id + '" r="2.6" opacity="0"/>'
    }
  }
  svg = svg + '</svg>'

  return svg
}

function renderFlowControlsHtml() {
  const modes: string[][] = [
    ['all', 'ALL PATHS'],
    ['inproc', 'INPROC · 同组'],
    ['shm', 'SHM · 跨组'],
  ]
  let html = ''
  for (let i = 0; i < modes.length; i++) {
    let act = ''
    if (i === 0) {
      act = ' active'
    }
    html = html + '<button class="fbtn' + act + '" data-mode="' + modes[i][0] + '">' + modes[i][1] + '</button>'
  }
  return html
}

function bindFlowControls() {
  const btns = document.querySelectorAll('.fbtn')
  for (let i = 0; i < btns.length; i++) {
    const b = btns[i]
    b.addEventListener('click', function() {
      for (let j = 0; j < btns.length; j++) {
        btns[j].className = 'fbtn'
      }
      b.className = 'fbtn active'
      $id('flow-svg').className = 'flow-svg mode-' + b.getAttribute('data-mode')
    })
  }
}

// 包点动画：沿 edge path 匀速流动
const packetState: PacketState[] = []

function initPackets() {
  for (const e of FLOW_EDGES) {
    const path = document.getElementById(e.id)
    if (path === null) {
      continue
    }
    const pe = path
    const len = pe.getTotalLength()
    const circles = document.querySelectorAll('[data-edge="' + e.id + '"]')
    for (let i = 0; i < circles.length; i++) {
      const c = circles[i]
      packetState.push({ el: pe, circle: c, len: len, off: i / circles.length, speed: 46 + i * 7 })
    }
  }
  let last = performance.now()
  const tick = function(now: number) {
    const dt = (now - last) / 1000
    last = now
    for (const p of packetState) {
      p.off = p.off + (p.speed * dt) / p.len
      if (p.off > 1) {
        p.off = p.off - 1
      }
      const pt = p.el.getPointAtLength(p.off * p.len)
      p.circle.setAttribute('cx', String(pt.x))
      p.circle.setAttribute('cy', String(pt.y))
      p.circle.setAttribute('opacity', '0.9')
    }
    requestAnimationFrame(tick)
  }
  requestAnimationFrame(tick)
}

// ── 03 pipeline ───────────────────────────────────────

function renderPipelineHtml() {
  let html = ''
  for (let i = 0; i < GATES.length; i++) {
    const g = GATES[i]
    html = html + '<div class="gate"><span class="gidx">' + String(i + 1).padStart(2, '0') + '</span>'
    html = html + '<div class="gi">✔</div><div class="gn">' + esc(g.name) + '</div>'
    html = html + '<div class="tip"><b>PASS · ' + esc(g.name) + '</b>' + esc(g.desc) + '</div>'
    html = html + '</div>'
  }
  return html
}

// ── 04 decisions ──────────────────────────────────────

function renderDecisionsHtml() {
  let html = '<div class="dec-row"><div class="h">DECISION</div><div class="h">CHOICE</div><div class="h">RATIONALE</div></div>'
  for (const d of DECISIONS) {
    html = html + '<div class="dec-row"><div class="d1">' + d.topic + '</div><div class="d2">' + esc(d.choice) + '</div><div class="d3">' + esc(d.why) + '</div></div>'
  }
  return html
}

// ── 05 terminal ───────────────────────────────────────

function renderTermHtml() {
  const L: string[] = []
  L.push('<span class="pr">$</span> star check')
  L.push('<span class="dm">[I] --- CI Guard: Step 1/6 Build ---</span>')
  L.push('<span class="dm">[I] --- CI Guard: Step 2/8 Schema ---</span>')
  L.push('<span class="dm">[I] --- CI Guard: Step 3/8 Unit Test ---</span>  <span class="ok">17 suites · 112 tests</span>')
  L.push('<span class="dm">[I] --- CI Guard: Step 4/8 Selftest ---</span> <span class="ok">soak 200 · shm overwrite ok</span>')
  L.push('<span class="dm">[I] --- CI Guard: Step 5/8 Doctor ---</span>   <span class="ok">control · planning · setting</span>')
  L.push('<span class="dm">[I] --- CI Guard: Step 6/8 List ---</span>     <span class="ok">4 real components</span>')
  L.push('<span class="cy">[I] =========================================</span>')
  L.push('<span class="cy">[I] CI Guard 结果: 10/10 PASS, 0 FAIL</span>')
  L.push('<span class="cy">[I] =========================================</span>')
  L.push('<span class="pr">$</span> star run --frames=3 planning <span class="dm"># 插件加载 · 全链路</span>')
  L.push('<span class="ok">MotionComponent processing frame=3 · TrajectoryOutput points=20</span>')
  L.push('<span class="cursor2"></span>')
  let html = ''
  for (const l of L) {
    html = html + l + '\n'
  }
  return html
}

// ── layer hover / info panel ──────────────────────────

let lockedLayer = ''

function layerInfoHtml(id: string) {
  let info: LayerInfo | null = null
  for (const l of LAYERS) {
    if (l.id === id) {
      info = l
    }
  }
  if (info === null) {
    return '<div class="ip-tag">// INFO</div><div class="ip-title">悬停查看层级</div><div class="ip-desc">左侧为整改后的实际分层，依赖严格单向。悬停高亮依赖边，点击锁定详情。</div>'
  }
  let html = '<div class="ip-tag">// ' + info.id.toUpperCase() + '</div>'
  html = html + '<div class="ip-title">' + info.title + '</div>'
  html = html + '<div class="ip-sub">' + info.sub + '</div>'
  html = html + '<div class="ip-desc">' + info.desc + '</div>'
  if (info.points.length > 0) {
    html = html + '<ul>'
    for (const p of info.points) {
      html = html + '<li>' + p + '</li>'
    }
    html = html + '</ul>'
  }
  return html
}

function showLayerInfo(id: string) {
  $id('layer-info').innerHTML = layerInfoHtml(id)
}

// 层 → 高亮的依赖边（与 renderLayers 中 data-dep 对应）
const LAYER_DEPS = {}
LAYER_DEPS.star = ['d0']
LAYER_DEPS.apps = ['d0', 'd1']
LAYER_DEPS.modules = ['d1', 'd2']
LAYER_DEPS.runtime = ['d2', 'd3']
LAYER_DEPS.transport = ['d3', 'd4']
LAYER_DEPS.schema = ['d4', 'd5', 'd6']
LAYER_DEPS.core = ['d5']
LAYER_DEPS.libraries = ['d6']
LAYER_DEPS.tests = ['d7', 'd8']

function highlightDeps(id: string) {
  const edges = document.querySelectorAll('.dep-edge')
  for (let i = 0; i < edges.length; i++) {
    edges[i].classList.remove('hl')
  }
  const boxes = document.querySelectorAll('.layer-box')
  for (let i = 0; i < boxes.length; i++) {
    boxes[i].classList.remove('dimmed')
  }
  if (id === '') {
    return
  }
  const hit = LAYER_DEPS[id]
  if (hit !== undefined) {
    for (const dep of hit) {
      const e = document.querySelector('[data-dep="' + dep + '"]')
      if (e !== null) {
        e.classList.add('hl')
      }
    }
  }
  if (id === 'transport' || id === 'schema' || id === 'core') {
    const lib = document.querySelector('[data-layer="libraries"]')
    if (lib !== null) {
      lib.classList.add('dimmed')
    }
  }
}

function bindLayers() {
  const boxes = document.querySelectorAll('.layer-box')
  for (let i = 0; i < boxes.length; i++) {
    const el = boxes[i]
    const layerId = el.getAttribute('data-layer')
    el.addEventListener('mouseenter', function() {
      showLayerInfo(layerId)
      highlightDeps(layerId)
    })
    el.addEventListener('mouseleave', function() {
      showLayerInfo(lockedLayer)
      highlightDeps(lockedLayer)
    })
    el.addEventListener('click', function() {
      if (lockedLayer === layerId) {
        lockedLayer = ''
      } else {
        lockedLayer = layerId
      }
      for (let j = 0; j < boxes.length; j++) {
        const ob = boxes[j]
        if (ob.getAttribute('data-layer') === lockedLayer) {
          ob.classList.add('active')
        } else {
          ob.classList.remove('active')
        }
      }
      showLayerInfo(lockedLayer)
      highlightDeps(lockedLayer)
    })
  }
  showLayerInfo('')
}

// ── reveal on scroll ──────────────────────────────────

function initReveal() {
  const els = document.querySelectorAll('.reveal')
  const io = new IntersectionObserver(function(entries) {
    for (const en of entries) {
      if (en.isIntersecting) {
        en.target.classList.add('in')
      }
    }
  }, { threshold: 0.08 })
  for (let i = 0; i < els.length; i++) {
    io.observe(els[i])
  }
}

// ── boot ──────────────────────────────────────────────

// 全部容器内容的纯函数出口：构建期（Node）烘焙进 HTML，浏览器期重复渲染为幂等
function renderAll() {
  const out = {}
  out.badges = renderBadgesHtml()
  out.stats = renderStatsHtml()
  out['layers-svg'] = renderLayersSvg()
  out['layer-info'] = layerInfoHtml('')
  out['flow-controls'] = renderFlowControlsHtml()
  out['flow-svg'] = renderFlowSvg()
  out.pipeline = renderPipelineHtml()
  out.decisions = renderDecisionsHtml()
  out['term-body'] = renderTermHtml()
  return out
}

globalThis.__STARLETTO = { renderAll: renderAll }

function boot() {
  // 内容已在构建期烘焙；此处重渲染（幂等）后绑定交互
  const baked = renderAll()
  for (const key in baked) {
    const host = $id(key)
    if (host !== null) {
      host.innerHTML = baked[key]
    }
  }
  // 渐显动画仅在 JS 可用时启用：无 JS 环境下内容保持完整可见
  const sections = document.querySelectorAll('section')
  for (let i = 0; i < sections.length; i++) {
    sections[i].classList.add('reveal')
  }
  bindLayers()
  bindFlowControls()
  initPackets()
  initReveal()

  const io2 = new IntersectionObserver(function(entries) {
    for (const en of entries) {
      if (en.isIntersecting) {
        animateStats()
      }
    }
  }, { threshold: 0.3 })
  io2.observe($id('stats'))
}

if (typeof document !== 'undefined') {
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', boot)
  } else {
    boot()
  }
}
