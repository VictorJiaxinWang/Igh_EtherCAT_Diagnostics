'use strict';

const byId = (id) => document.getElementById(id);
const state = {
  snapshot: null,
  masters: [],
  selectedMaster: null,
  events: [],
  lastTimestamp: null,
  lastAdvanceAt: 0,
  lastSuccessAt: 0,
  error: ''
};

function selectMasterSnapshot() {
  return state.masters.find((item) => Number(item.master_index) === Number(state.selectedMaster)) || state.masters[0] || null;
}

function updateMasterSelector() {
  const select = byId('master-select');
  const available = state.masters.map((item) => Number(item.master_index));
  if (!available.includes(Number(state.selectedMaster))) state.selectedMaster = available[0] ?? null;
  select.replaceChildren(...available.map((index) => {
    const option = document.createElement('option');
    option.value = String(index);
    option.textContent = `Master ${index}`;
    option.selected = index === Number(state.selectedMaster);
    return option;
  }));
  select.disabled = available.length < 2;
}

const causes = {
  MASTER_LINK_FAILURE: {
    title: '主站入口链路中断',
    description: 'Master 与 EtherCAT 网络首站之间没有建立物理通信。',
    actions: ['检查 RK3588 EtherCAT 网口指示灯。', '重新插接 Master 到 Slave0 的网线。', '确认 Slave0 已供电且网口方向正确。']
  },
  BOUNDARY_LINK_FAILURE: {
    title: '从站之间的链路中断',
    description: '前面的从站仍然在线，但其下游从站已经无法到达。',
    actions: ['定位页面拓扑中的红色断点。', '重新插接断点两端的网线和接头。', '检查第一个离线从站的供电和网口指示灯。']
  },
  SLAVE_INTERNAL_ERROR: {
    title: '从站自身状态错误',
    description: '网络链路仍在，但从站报告了 EtherCAT AL 状态错误。',
    actions: ['查看页面标红的从站位置和名称。', '检查该从站供电、驱动状态和故障指示灯。', '按设备手册处理 AL 状态码，必要时重新上电。']
  },
  LINK_QUALITY_DEGRADATION: {
    title: '链路质量正在下降',
    description: '通信尚未完全中断，但 CRC、接收或 Lost Link 计数正在增加。',
    actions: ['检查相关网线是否松动、破损或弯折过度。', '检查屏蔽层、接地和强电干扰。', '更换线缆后观察错误是否停止增长。']
  },
  UNKNOWN: {
    title: '证据不足，暂时无法确定',
    description: '当前采集结果不足以形成可靠结论，请保留事件记录交给工程师。',
    actions: ['不要连续反复复位设备。', '记录当前拓扑、指示灯和操作过程。', '导出 events.jsonl 与故障黑匣子文件。']
  }
};

function setText(id, value) {
  byId(id).textContent = value;
}

function formatBoardTime(value) {
  if (value === null || value === undefined || !Number.isFinite(Number(value))) return '--';
  return new Date(Number(value)).toLocaleString('zh-CN', { hour12: false });
}

function setConnection(ok, message) {
  const element = byId('connection-state');
  element.className = `connection-pill ${ok ? 'connected' : 'error'}`;
  element.replaceChildren();
  const dot = document.createElement('span');
  dot.className = 'pulse-dot';
  element.append(dot, document.createTextNode(message));
}

function effectiveStatus(snapshot) {
  if (!snapshot) return 'UNKNOWN';
  const unchangedFor = performance.now() - state.lastAdvanceAt;
  if (state.lastAdvanceAt === 0 || unchangedFor > 5000) return 'STALE';
  return ['HEALTHY', 'DEGRADED', 'FAULT'].includes(snapshot.status)
    ? snapshot.status
    : 'UNKNOWN';
}

function renderOverall(snapshot) {
  const status = effectiveStatus(snapshot);
  const hero = byId('overall-status');
  const copy = {
    HEALTHY: ['✓', '网络正常', 'Master 链路和当前从站状态正常。', 'healthy'],
    DEGRADED: ['!', '网络异常，请关注', '网络仍在通信，但已发现异常状态或质量问题。', 'degraded'],
    FAULT: ['×', '网络故障，需要处理', '请查看下方红色断点和恢复步骤。', 'fault'],
    STALE: ['…', '诊断数据已经停止更新', '诊断程序可能已停止，请先检查 RK3588 服务。', 'unknown'],
    UNKNOWN: ['?', '无法判断网络状态', '等待有效诊断数据。', 'unknown']
  }[status];

  hero.className = `status-hero state-${copy[3]}`;
  hero.querySelector('.status-mark').textContent = copy[0];
  setText('status-title', copy[1]);
  setText('status-summary', copy[2]);
  setText('updated-time', snapshot ? formatBoardTime(snapshot.updated_ms) : '--');
  setText('refresh-time', state.lastSuccessAt ? `网页刷新 ${new Date(state.lastSuccessAt).toLocaleTimeString('zh-CN', {hour12:false})}` : '尚未成功刷新');
}

function renderMetrics(snapshot) {
  if (!snapshot) return;
  setText('master-link', snapshot.link_up ? 'UP · 已连接' : 'DOWN · 已断开');
  setText('master-index', `Master ${snapshot.master_index ?? 0}`);
  setText('slave-count', String(snapshot.slave_count ?? snapshot.slaves?.length ?? 0));
  setText('master-phase', snapshot.phase || 'UNKNOWN');
  setText('master-active', `Active ${snapshot.active ? 'yes' : 'no'}`);
  setText('last-fault', snapshot.last_fault_timestamp_ms ? formatBoardTime(snapshot.last_fault_timestamp_ms) : '无记录');
}

function nodeClass(slave, offline) {
  if (offline || slave.online === false) return 'offline';
  if (slave.has_error) return 'fault';
  if (slave.state && slave.state !== 'OP') return 'warning';
  return 'healthy';
}

function topologyNode(title, subtitle, stateText, className) {
  const node = document.createElement('article');
  node.className = `topology-node ${className}`;
  node.setAttribute('role', 'listitem');
  const heading = document.createElement('strong');
  const detail = document.createElement('span');
  const status = document.createElement('small');
  heading.textContent = title;
  detail.textContent = subtitle;
  status.textContent = stateText;
  node.append(heading, detail, status);
  return node;
}

function topologyLink(broken) {
  const link = document.createElement('div');
  link.className = `topology-link${broken ? ' broken' : ''}`;
  link.setAttribute('aria-label', broken ? '此处链路中断' : '链路连接');
  return link;
}

function renderTopology(snapshot) {
  const container = byId('topology');
  container.replaceChildren();
  if (!snapshot) {
    const empty = document.createElement('p');
    empty.className = 'empty-state';
    empty.textContent = '等待网络快照';
    container.append(empty);
    return;
  }

  const cause = snapshot.root_cause || {};
  const boundary = cause.boundary || null;
  container.append(topologyNode('Master', 'RK3588 / IgH', snapshot.link_up ? 'LINK UP' : 'LINK DOWN', snapshot.link_up ? 'healthy' : 'fault'));

  const slaves = Array.isArray(snapshot.slaves) ? [...snapshot.slaves] : [];
  if (boundary && Number.isInteger(boundary.first_lost_alias) && !slaves.some((item) => Number(item.alias) === Number(boundary.first_lost_alias))) {
    slaves.push({ position: boundary.first_lost_slave, alias: boundary.first_lost_alias, relative_position: boundary.first_lost_relative_position, name: '第一个离线从站', state: 'OFFLINE', online: false });
  }
  slaves.sort((a, b) => Number(a.position) - Number(b.position));

  slaves.forEach((slave, index) => {
    const position = Number(slave.position);
    const brokenAtMaster = cause.kind === 'MASTER_LINK_FAILURE' && index === 0;
    const brokenAtBoundary = boundary && Number(slave.alias) === Number(boundary.first_lost_alias);
    container.append(topologyLink(Boolean(brokenAtMaster || brokenAtBoundary)));
    const offline = slave.online === false;
    const identity = Number(slave.alias) > 0 ? `Alias ${slave.alias}:${slave.relative_position ?? 0}` : `Slave ${position}`;
    container.append(topologyNode(identity, `${slave.name || '未知设备'} · Position ${position}`, offline ? 'OFFLINE' : (slave.state || 'UNKNOWN'), nodeClass(slave, offline)));
  });

  if (slaves.length === 0) {
    container.append(topologyLink(!snapshot.link_up));
    container.append(topologyNode('Slave 0', '没有可响应从站', 'OFFLINE', 'offline'));
  }

  setText('topology-hint', boundary
    ? `红色断点：Alias ${boundary.last_alive_alias}:${boundary.last_alive_relative_position} 与 Alias ${boundary.first_lost_alias}:${boundary.first_lost_relative_position} 之间`
    : (snapshot.link_up ? '当前未定位到链路断点' : 'Master 入口链路已经断开'));
}

function replaceList(id, values, fallback) {
  const list = byId(id);
  list.replaceChildren();
  const items = values.length ? values : [fallback];
  items.forEach((value) => {
    const item = document.createElement('li');
    item.textContent = value;
    list.append(item);
  });
}

function renderDiagnosis(snapshot) {
  const report = snapshot?.root_cause;
  const healthy = effectiveStatus(snapshot) === 'HEALTHY';
  const kind = report?.kind || 'UNKNOWN';
  const cause = causes[kind] || causes.UNKNOWN;
  const certainty = byId('certainty');

  if (!report && healthy) {
    setText('cause-title', '当前没有活动故障');
    setText('cause-description', '网络状态正常，诊断系统正在持续观察链路和从站。');
    certainty.className = 'tag good';
    certainty.textContent = '正常';
    setText('confidence-text', '--');
    byId('confidence-bar').style.width = '0%';
    replaceList('evidence-list', [], '暂无故障证据');
    replaceList('recovery-steps', ['无需处理，保持设备正常运行。'], '无需处理');
    return;
  }

  setText('cause-title', report ? cause.title : '等待根因分析');
  setText('cause-description', report ? cause.description : '故障发生后，系统将读取 ESC 并汇总诊断证据。');
  const confidence = Math.max(0, Math.min(1, Number(report?.confidence || 0)));
  const conclusive = Boolean(report?.conclusive);
  certainty.className = `tag ${conclusive ? 'bad' : 'warn'}`;
  certainty.textContent = conclusive ? '结论明确' : '当前为推测';
  setText('confidence-text', report ? `${Math.round(confidence * 100)}%` : '--');
  byId('confidence-bar').style.width = `${confidence * 100}%`;
  replaceList('evidence-list', Array.isArray(report?.supporting_evidence) ? report.supporting_evidence : [], '正在收集支持证据');

  const actions = [...cause.actions];
  const boundary = report?.boundary;
  if (kind === 'BOUNDARY_LINK_FAILURE' && boundary) {
    actions[0] = `定位 Alias ${boundary.last_alive_alias}:${boundary.last_alive_relative_position} 与 Alias ${boundary.first_lost_alias}:${boundary.first_lost_relative_position} 之间的红色断点。`;
  }
  actions.push('重新连接后等待至少 2 秒，确认顶部变绿且从站数量恢复。');
  replaceList('recovery-steps', actions, '保留现场并联系工程师');
}

function eventPresentation(event) {
  if (event.record === 'recovery') {
    return ['recovery', '网络已经恢复', event.description || '完整拓扑重新在线'];
  }
  if (event.record === 'root_cause') {
    const cause = causes[event.kind] || causes.UNKNOWN;
    return ['root-cause', cause.title, `置信度 ${Math.round(Number(event.confidence || 0) * 100)}%`];
  }
  const labels = {
    MASTER_LINK_DOWN: 'Master 链路断开',
    MASTER_LINK_UP: 'Master 链路恢复',
    SLAVE_COUNT_CHANGED: '在线从站数量变化',
    SLAVE_LOST: '从站掉线',
    SLAVE_STATE_CHANGED: '从站状态变化',
    PORT_INVALID_FRAME_INCREASED: '无效帧计数增加',
    PORT_RX_ERROR_INCREASED: '接收错误计数增加',
    PORT_FORWARDED_RX_ERROR_INCREASED: '转发错误计数增加',
    PORT_LOST_LINK_INCREASED: 'Lost Link 计数增加'
  };
  return ['fault', labels[event.type] || '诊断事件', event.description || `old=${event.old_value ?? '-'} new=${event.new_value ?? '-'}`];
}

function renderEvents(events) {
  const timeline = byId('event-timeline');
  timeline.replaceChildren();
  setText('event-count', `${events.length} 条`);
  if (!events.length) {
    const empty = document.createElement('p');
    empty.className = 'empty-state';
    empty.textContent = '暂时没有故障或恢复事件';
    timeline.append(empty);
    return;
  }

  [...events].reverse().forEach((event) => {
    const [className, title, description] = eventPresentation(event);
    const row = document.createElement('article');
    row.className = `event-row ${className}`;
    const time = document.createElement('time');
    time.className = 'event-time';
    time.textContent = formatBoardTime(event.timestamp_ms).split(' ').pop();
    const rail = document.createElement('div');
    rail.className = 'event-rail';
    const copy = document.createElement('div');
    copy.className = 'event-copy';
    const heading = document.createElement('strong');
    const body = document.createElement('p');
    heading.textContent = title;
    body.textContent = description;
    copy.append(heading, body);
    row.append(time, rail, copy);
    timeline.append(row);
  });
}

function render() {
  renderOverall(state.snapshot);
  renderMetrics(state.snapshot);
  renderTopology(state.snapshot);
  renderDiagnosis(state.snapshot);
  renderEvents(state.events);
}

async function fetchJson(path) {
  const response = await fetch(path, { cache: 'no-store' });
  if (!response.ok) {
    let message = `HTTP ${response.status}`;
    try {
      const error = await response.json();
      if (error.error) message = error.error;
    } catch (_) { /* response may not be JSON */ }
    throw new Error(message);
  }
  return response.json();
}

async function refresh() {
  const [statusResult, eventsResult] = await Promise.allSettled([
    fetchJson('/api/status'),
    fetchJson('/api/events?limit=50')
  ]);

  if (statusResult.status === 'fulfilled') {
    const payload = statusResult.value;
    state.masters = Array.isArray(payload.masters) ? payload.masters : [payload];
    updateMasterSelector();
    const snapshot = selectMasterSnapshot();
    if (snapshot.updated_ms !== state.lastTimestamp) {
      state.lastTimestamp = snapshot.updated_ms;
      state.lastAdvanceAt = performance.now();
    }
    state.snapshot = snapshot;
    state.lastSuccessAt = Date.now();
    state.error = '';
    setConnection(true, '诊断数据已连接');
  } else {
    state.error = statusResult.reason?.message || '无法读取诊断状态';
    setConnection(false, `连接异常：${state.error}`);
  }

  if (eventsResult.status === 'fulfilled' && Array.isArray(eventsResult.value)) {
    state.events = eventsResult.value.filter((event) =>
      event.master_index === undefined || Number(event.master_index) === Number(state.selectedMaster));
  }
  render();
}

refresh();
byId('master-select').addEventListener('change', (event) => {
  state.selectedMaster = Number(event.target.value);
  state.snapshot = selectMasterSnapshot();
  state.lastTimestamp = null;
  state.lastAdvanceAt = performance.now();
  refresh();
});
setInterval(refresh, 1000);
