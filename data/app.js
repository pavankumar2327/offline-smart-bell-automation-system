const state = {
  token: localStorage.getItem('sb_token') || '',
  role: localStorage.getItem('sb_role') || 'admin',
  page: localStorage.getItem('sb_page') || 'home',
  status: null,
  settings: null,
  profiles: null,
  editingProfileId: localStorage.getItem('sb_editing_profile') || '',
  clockBaseEpoch: 0,
  clockBaseMs: 0,
  scheduleSaveTimer: 0,
  holidaySearch: '',
  busy: false
};

const $ = (selector, root = document) => root.querySelector(selector);
const $$ = (selector, root = document) => [...root.querySelectorAll(selector)];
const page = id => document.getElementById(id);

function escapeHtml(value) {
  return String(value ?? '').replace(/[&<>"']/g, ch => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[ch]));
}

function toast(message, type = 'ok') {
  const box = $('#toast');
  box.textContent = message;
  box.className = `toast ${type}`;
  clearTimeout(toast.timer);
  toast.timer = setTimeout(() => box.classList.add('hidden'), 2800);
}

async function api(path, options = {}) {
  const headers = Object.assign({'Content-Type': 'application/json'}, options.headers || {});
  if (state.token) headers.Authorization = 'Bearer ' + state.token;
  const response = await fetch(path, Object.assign({}, options, {headers}));
  const text = await response.text();
  let data = {};
  if (text) {
    try { data = JSON.parse(text); } catch (_) { data = {message: text}; }
  }
  if (response.status === 401) {
    showLogin('Please login again.');
    throw new Error('Please login again.');
  }
  if (!response.ok) throw new Error(data.error || data.message || 'Request failed');
  return data;
}

function showLogin(message = '') {
  $('#loginView').classList.remove('hidden');
  $('#appView').classList.add('hidden');
  $('#loginMsg').textContent = message;
  $('#loginPass').value = '';
}

function showApp() {
  $('#loginView').classList.add('hidden');
  $('#appView').classList.remove('hidden');
  activatePage('home', true);
}

function setBusy(on) {
  state.busy = on;
  $$('button').forEach(button => {
    if (!button.classList.contains('nav-button')) button.disabled = on;
  });
}

$('#loginForm').addEventListener('submit', async event => {
  event.preventDefault();
  $('#loginMsg').textContent = '';
  try {
    const result = await api('/api/auth/login', {
      method: 'POST',
      body: JSON.stringify({username: $('#loginUser').value.trim(), password: $('#loginPass').value})
    });
    state.token = result.token;
    state.role = result.role || 'admin';
    localStorage.setItem('sb_token', state.token);
    localStorage.setItem('sb_role', state.role);
    await loadBaseData();
    $('#loginPass').value = '';
    showApp();
    await syncFromBrowserIfNeeded();
  } catch (error) {
    $('#loginMsg').textContent = error.message;
  }
});

$('#logoutBtn').addEventListener('click', async () => {
  try { await api('/api/auth/logout', {method: 'POST'}); } catch (_) {}
  localStorage.removeItem('sb_token');
  localStorage.removeItem('sb_role');
  state.token = '';
  showLogin('');
});

$$('.nav [data-page]').forEach(button => {
  button.classList.add('nav-button');
  button.addEventListener('click', () => activatePage(button.dataset.page));
});

function activatePage(id, render = true) {
  if (state.scheduleSaveTimer) {
    clearTimeout(state.scheduleSaveTimer);
    state.scheduleSaveTimer = 0;
  }
  state.page = id;
  localStorage.setItem('sb_page', id);
  $$('.nav [data-page]').forEach(button => button.classList.toggle('active', button.dataset.page === id));
  $$('.page').forEach(section => section.classList.add('hidden'));
  page(id).classList.remove('hidden');
  if (render) renderCurrent();
}

async function loadBaseData() {
  state.status = await api('/api/status');
  updateClockBase();
  state.settings = await api('/api/settings');
  state.profiles = await api('/api/profiles');
  $('#deviceName').textContent = state.settings.deviceName || 'Smart Bell';
}

async function refreshStatus(renderActive = false) {
  state.status = await api('/api/status');
  updateClockBase();
  if (state.page === 'home') renderHome();
  if (state.page === 'time') renderTime();
  if (renderActive) renderCurrent();
}

function updateClockBase() {
  state.clockBaseEpoch = state.status?.epoch || 0;
  state.clockBaseMs = Date.now();
}

function renderCurrent() {
  const renderers = {
    home: renderHome,
    schedules: renderSchedules,
    profiles: renderProfiles,
    holidays: renderHolidays,
    time: renderTime,
    settings: renderSettings
  };
  (renderers[state.page] || renderHome)();
}

function liveClockText() {
  if (!state.clockBaseEpoch) return state.status?.time || '-';
  const elapsed = Math.floor((Date.now() - state.clockBaseMs) / 1000);
  return new Date((state.clockBaseEpoch + elapsed) * 1000).toLocaleString();
}

function statusLabel(value) {
  return `<span class="status-pill ${value === 'ok' || value === 'connected' || value === 'synced' ? 'good' : ''}">${escapeHtml(value || '-')}</span>`;
}

function metric(label, value) {
  return `<article class="card metric"><span>${label}</span><strong>${value}</strong></article>`;
}

function renderHome() {
  const s = state.status || {};
  const next = s.nextBell ? `${escapeHtml(s.nextBell.time)}<small>${escapeHtml(s.nextBell.label || 'Bell')}</small>` : 'No bell scheduled';
  page('home').innerHTML = `
    <div class="hero-card">
      <div>
        <span class="eyebrow">Current Time</span>
        <strong id="liveClock" class="home-clock">${liveClockText()}</strong>
      </div>
      <button class="primary big" onclick="ringBell()">Ring Bell</button>
    </div>
    <div class="grid home-grid">
      ${metric('RTC Status', statusLabel(s.rtcStatus))}
      ${metric('Wi-Fi Status', escapeHtml(s.wifiStatus || '-'))}
      ${metric('Active Profile', escapeHtml(s.activeProfile || '-'))}
      ${metric('Next Bell', next)}
    </div>
    <div id="holidayReminder"></div>
    <div class="card action-row">
      <button onclick="syncBrowserTime()">Sync Time</button>
      <button class="danger" onclick="restartDevice()">Restart Device</button>
    </div>`;
  renderHolidayReminder();
}

async function ringBell() {
  await runAction(async () => {
    await api('/api/bell', {method: 'POST', body: JSON.stringify({action: 'ring', duration: 3000})});
    toast('Bell started.');
    await refreshStatus();
  });
}

async function syncBrowserTime() {
  await runAction(async () => {
    await api('/api/time/sync-browser', {method: 'POST', body: JSON.stringify({epoch: Math.floor(Date.now() / 1000)})});
    toast('Time synced from this device.');
    await refreshStatus();
  });
}

async function restartDevice() {
  if (!confirm('Restart the bell controller now?')) return;
  await runAction(async () => {
    await api('/api/device/restart', {method: 'POST'});
    toast('Device restarting.');
  });
}

async function renderSchedules() {
  const profileId = selectedScheduleProfileId();
  const profile = profileById(profileId);
  const schedule = await api('/api/schedules?profile=' + encodeURIComponent(profileId));
  const rows = (schedule.entries || []).map((item, index) => scheduleRow(item, index)).join('');
  const profileOptions = (state.profiles?.profiles || []).map(item =>
    `<option value="${escapeHtml(item.id)}" ${item.id === profileId ? 'selected' : ''}>${escapeHtml(item.name)}</option>`
  ).join('');
  page('schedules').innerHTML = `
    <div class="page-title">
      <div>
        <h2>Schedules</h2>
        <p>Each profile has its own timetable. Choose a profile, edit its bells, then save.</p>
      </div>
      <button onclick="addScheduleRow()">Add Schedule</button>
    </div>
    <div class="card profile-editor">
      <div>
        <span>Currently Editing Profile</span>
        <strong>${escapeHtml(profile?.name || profileId)}</strong>
        ${profileId === state.profiles?.activeProfileId ? '<em>Active now</em>' : '<em>Not active now</em>'}
      </div>
      <label>Choose timetable profile
        <select id="scheduleProfileSelect" onchange="changeScheduleProfile(this.value)">${profileOptions}</select>
      </label>
    </div>
    <div class="card note-card">
      Repeat means extra rings after the first ring. Repeat 2 means the bell rings 3 times total.
    </div>
    <div class="table-card">
      <table class="schedule-table">
        <thead><tr><th>Time</th><th>Duration</th><th>Repeat</th><th>Label</th><th>Enabled</th><th>Delete</th></tr></thead>
        <tbody id="scheduleRows">${rows || scheduleRow({time:'09:00', duration:3000, repeatCount:1, repeatInterval:1000, label:'Morning Bell', enabled:true}, 0)}</tbody>
      </table>
    </div>
    <div class="card save-bar">
      <span id="scheduleMessage">Changes auto-save for the ${escapeHtml(profile?.name || profileId)} timetable.</span>
      <button onclick="saveScheduleNow()">Save Now</button>
    </div>`;
  attachScheduleAutosave();
}

function selectedScheduleProfileId() {
  const available = state.profiles?.profiles || [];
  const active = state.profiles?.activeProfileId || 'regular';
  if (available.some(profile => profile.id === state.editingProfileId)) return state.editingProfileId;
  state.editingProfileId = active;
  localStorage.setItem('sb_editing_profile', state.editingProfileId);
  return state.editingProfileId;
}

function profileById(id) {
  return (state.profiles?.profiles || []).find(profile => profile.id === id);
}

async function changeScheduleProfile(profileId) {
  state.editingProfileId = profileId;
  localStorage.setItem('sb_editing_profile', profileId);
  await renderSchedules();
  toast(`Now editing ${profileById(profileId)?.name || profileId}.`);
}

function scheduleRow(item, index) {
  const durationSec = Math.max(1, Math.round((Number(item.duration) || 3000) / 1000));
  const repeatExtra = Math.max(0, (Number(item.repeatCount) || 1) - 1);
  return `<tr class="schedule-row" data-row="${index}">
    <td><input type="time" class="sch-time" value="${escapeHtml(item.time || '09:00')}"></td>
    <td><input type="number" class="sch-duration" min="1" max="60" value="${durationSec}"><small>seconds</small></td>
    <td><input type="number" class="sch-repeat" min="0" max="10" value="${repeatExtra}"><small>extra rings</small></td>
    <td><input type="text" class="sch-label" maxlength="32" value="${escapeHtml(item.label || 'Bell')}"></td>
    <td><label class="switch"><input type="checkbox" class="sch-enabled" ${item.enabled === false ? '' : 'checked'}><span></span></label></td>
    <td><button class="danger small" onclick="deleteScheduleRow(this)">Delete</button></td>
  </tr>`;
}

function addScheduleRow() {
  const body = $('#scheduleRows');
  body.insertAdjacentHTML('beforeend', scheduleRow({time:'09:00', duration:3000, repeatCount:1, repeatInterval:1000, label:'Bell', enabled:true}, body.children.length));
  attachScheduleAutosave();
  markScheduleDirty('New schedule added. Auto-saving...');
}

function deleteScheduleRow(button) {
  button.closest('tr').remove();
  markScheduleDirty('Schedule removed. Auto-saving...');
}

function collectScheduleRows() {
  const entries = $$('#scheduleRows tr').map(row => {
    const time = $('.sch-time', row).value;
    const durationSec = Number($('.sch-duration', row).value);
    const repeatExtra = Number($('.sch-repeat', row).value);
    const label = $('.sch-label', row).value.trim() || 'Bell';
    return {
      time,
      duration: Math.round(durationSec * 1000),
      repeatCount: repeatExtra + 1,
      repeatInterval: 1000,
      enabled: $('.sch-enabled', row).checked,
      label
    };
  });

  const seen = new Set();
  for (const entry of entries) {
    if (!/^\d{2}:\d{2}$/.test(entry.time)) throw new Error('Please enter a valid time for every row.');
    if (seen.has(entry.time)) throw new Error('Two schedules cannot use the same time.');
    if (entry.duration < 1000 || entry.duration > 60000) throw new Error('Duration must be between 1 and 60 seconds.');
    if (entry.repeatCount < 1 || entry.repeatCount > 11) throw new Error('Repeat must be between 0 and 10 extra rings.');
    seen.add(entry.time);
  }
  return entries.sort((a, b) => a.time.localeCompare(b.time));
}

function attachScheduleAutosave() {
  $$('#scheduleRows input').forEach(input => {
    input.addEventListener('input', () => markScheduleDirty());
    input.addEventListener('change', () => markScheduleDirty());
  });
}

function markScheduleDirty(message = 'Unsaved changes. Auto-saving...') {
  const label = $('#scheduleMessage');
  if (label) label.textContent = message;
  if (state.scheduleSaveTimer) clearTimeout(state.scheduleSaveTimer);
  state.scheduleSaveTimer = setTimeout(() => saveSchedule(false), 1600);
}

async function saveScheduleNow() {
  if (state.scheduleSaveTimer) {
    clearTimeout(state.scheduleSaveTimer);
    state.scheduleSaveTimer = 0;
  }
  await saveSchedule(true);
}

async function saveSchedule(showToast = true) {
  await runAction(async () => {
    const profileId = selectedScheduleProfileId();
    const profile = profileById(profileId);
    const entries = collectScheduleRows();
    await api('/api/schedules', {method: 'PUT', body: JSON.stringify({profileId, entries})});
    state.scheduleSaveTimer = 0;
    $('#scheduleMessage').textContent = `${profile?.name || profileId} timetable saved successfully.`;
    if (showToast) toast(`${profile?.name || 'Profile'} schedule saved.`);
    await refreshStatus();
  });
}

function renderProfiles() {
  const p = state.profiles || {profiles: [], activeProfileId: ''};
  page('profiles').innerHTML = `
    <div class="page-title"><div><h2>Profiles</h2><p>Select the timetable the bell should follow today.</p></div></div>
    <div class="profile-list">
      ${p.profiles.map(profile => `
        <article class="card profile-card">
          <div><strong>${escapeHtml(profile.name)}</strong>${profile.id === p.activeProfileId ? '<span class="status-pill good">Active</span>' : ''}</div>
          <button onclick="activateProfileId('${escapeHtml(profile.id)}')" ${profile.id === p.activeProfileId ? 'disabled' : ''}>Use This Profile</button>
        </article>`).join('')}
    </div>
    <div class="card compact-form">
      <label>New profile name<input id="newProfileName" placeholder="Example: Exam Timetable"></label>
      <button onclick="createProfile()">Create Profile</button>
    </div>`;
}

async function saveProfilesDoc() {
  await api('/api/profiles', {method: 'PUT', body: JSON.stringify(state.profiles)});
  state.profiles = await api('/api/profiles');
}

async function createProfile() {
  await runAction(async () => {
    const name = $('#newProfileName').value.trim();
    if (!name) throw new Error('Please enter a profile name.');
    const id = name.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '') + '-' + Date.now().toString(36);
    state.profiles.profiles.push({id, name, locked: false});
    await saveProfilesDoc();
    await api('/api/schedules', {method: 'PUT', body: JSON.stringify({profileId: id, entries: []})});
    toast('Profile created.');
    renderProfiles();
  });
}

async function activateProfileId(id) {
  await runAction(async () => {
    await api('/api/profiles/activate', {method: 'POST', body: JSON.stringify({id})});
    state.profiles = await api('/api/profiles');
    state.editingProfileId = id;
    localStorage.setItem('sb_editing_profile', id);
    toast('Profile changed.');
    await refreshStatus();
    renderProfiles();
  });
}

async function renderHolidays() {
  const holidays = await api('/api/holidays');
  const rows = sortedHolidayItems(holidays)
    .map(item => `
      <tr data-holiday-row data-search="${escapeHtml(`${item.date || ''} ${item.end || ''} ${item.label || ''}`.toLowerCase())}">
        <td><strong>${escapeHtml(item.range ? `${formatHolidayDate(item.start)} - ${formatHolidayDate(item.end)}` : formatHolidayDate(item.date))}</strong><small>${escapeHtml(item.range ? `${item.start} to ${item.end}` : item.date)}</small></td>
        <td>${escapeHtml(item.label || 'Holiday')}${item.range ? '<small>Vacation range</small>' : item.repeatYearly ? '<small>Repeats every year</small>' : ''}</td>
        <td><button class="danger small" onclick="${item.range ? `deleteVacationRange(${item.sourceIndex})` : `deleteHoliday(${item.sourceIndex})`}">Delete</button></td>
      </tr>`).join('');
  page('holidays').innerHTML = `
    <div class="page-title"><div><h2>Holidays</h2><p>No automatic bells will ring on these dates.</p></div></div>
    <div class="card holiday-form">
      <label>Name<input id="holidayLabel" placeholder="Example: Annual Day"></label>
      <label>Date<input id="holidayDate" type="date"></label>
      <label class="check-row"><input id="holidayRepeatYearly" type="checkbox">Repeat every year</label>
      <button onclick="addHoliday()">Add Holiday</button>
    </div>
    <div class="card vacation-form">
      <div>
        <h3>Vacation Range</h3>
        <p>Use this for summer holidays, exam breaks, or multi-day closures.</p>
      </div>
      <label>Name<input id="rangeLabel" placeholder="Example: Summer Break"></label>
      <label>Start Date<input id="rangeStart" type="date"></label>
      <label>End Date<input id="rangeEnd" type="date"></label>
      <button onclick="addVacationRange()">Add Range</button>
    </div>
    <div class="card weekly-form">
      <div>
        <h3>Weekly Off Days</h3>
        <p>Automatic bells stay off every selected weekday.</p>
      </div>
      <div class="weekday-grid">${weekdayOptions(holidays.weekly || [])}</div>
      <button onclick="saveWeeklyHolidays()">Save Weekly Off Days</button>
    </div>
    <div class="card import-export">
      <div>
        <h3>Import / Export</h3>
        <p>Download a backup or import holidays from CSV/JSON.</p>
      </div>
      <div class="action-row">
        <button onclick="exportHolidaysJson()">Download JSON</button>
        <button onclick="exportHolidaysCsv()">Download CSV</button>
        <label class="file-button">Import File<input id="holidayImportFile" type="file" accept=".json,.csv,text/csv,application/json"></label>
      </div>
    </div>
    <div class="card holiday-tools">
      <label>Search holidays<input id="holidaySearch" value="${escapeHtml(state.holidaySearch)}" placeholder="Search by name or date"></label>
    </div>
    <div class="table-card">
      <table><thead><tr><th>Date</th><th>Name</th><th>Delete</th></tr></thead><tbody>${rows || '<tr><td colspan="3">No holidays added.</td></tr>'}</tbody></table>
    </div>`;
  $('#holidaySearch').addEventListener('input', event => {
    state.holidaySearch = event.target.value;
    applyHolidayFilter();
  });
  $('#holidayImportFile').addEventListener('change', importHolidayFile);
  applyHolidayFilter();
}

async function getHolidays() { return api('/api/holidays'); }
async function saveHolidays(doc) { await api('/api/holidays', {method: 'PUT', body: JSON.stringify(doc)}); }

async function renderHolidayReminder() {
  const target = $('#holidayReminder');
  if (!target) return;
  try {
    const doc = await getHolidays();
    const next = nextHolidaySummary(doc);
    if (!next) {
      target.innerHTML = '';
      return;
    }
    target.innerHTML = `<article class="card holiday-reminder">
      <span>Next Holiday</span>
      <strong>${escapeHtml(next.label)}</strong>
      <small>${escapeHtml(next.when)}</small>
    </article>`;
  } catch (_) {
    target.innerHTML = '';
  }
}

async function addHoliday() {
  await runAction(async () => {
    const date = $('#holidayDate').value;
    const label = $('#holidayLabel').value.trim() || 'Holiday';
    const repeatYearly = $('#holidayRepeatYearly').checked;
    if (!date) throw new Error('Please choose a date.');
    const doc = await getHolidays();
    doc.single = doc.single || [];
    if (doc.single.some(item => sameHolidayDate(item, date, repeatYearly))) throw new Error('This holiday is already added.');
    doc.single.push(repeatYearly ? {date, label, repeatYearly: true} : {date, label});
    doc.single.sort((a, b) => a.date.localeCompare(b.date));
    await saveHolidays(doc);
    toast('Holiday saved.');
    renderHolidays();
  });
}

async function addVacationRange() {
  await runAction(async () => {
    const start = $('#rangeStart').value;
    const end = $('#rangeEnd').value;
    const label = $('#rangeLabel').value.trim() || 'Vacation';
    if (!start || !end) throw new Error('Please choose start and end dates.');
    if (end < start) throw new Error('End date must be after start date.');
    const doc = await getHolidays();
    doc.ranges = doc.ranges || [];
    if (doc.ranges.some(range => range.start === start && range.end === end)) throw new Error('This vacation range is already added.');
    doc.ranges.push({start, end, label});
    doc.ranges.sort((a, b) => String(a.start).localeCompare(String(b.start)));
    await saveHolidays(doc);
    toast('Vacation range saved.');
    renderHolidays();
  });
}

function weekdayOptions(selected) {
  const names = ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'];
  const selectedDays = selected.map(Number);
  return names.map((name, day) => `
    <label class="check-row weekday-option">
      <input type="checkbox" value="${day}" ${selectedDays.includes(day) ? 'checked' : ''}>${name}
    </label>`).join('');
}

async function saveWeeklyHolidays() {
  await runAction(async () => {
    const doc = await getHolidays();
    doc.weekly = $$('.weekday-option input:checked').map(input => Number(input.value)).sort((a, b) => a - b);
    await saveHolidays(doc);
    toast('Weekly off days saved.');
    renderHolidays();
  });
}

function sortedHolidayItems(doc) {
  const today = new Date();
  today.setHours(0, 0, 0, 0);
  const singles = (doc.single || []).map((item, sourceIndex) => {
    const nextDate = nextHolidayDate(item, today);
    return Object.assign({}, item, {sourceIndex, nextTime: nextDate.getTime()});
  });
  const ranges = (doc.ranges || []).map((range, rangeIndex) => {
    const date = nextRangeDate(range, today);
    return {
      date: range.start,
      label: range.label || 'Vacation',
      repeatYearly: false,
      sourceIndex: rangeIndex,
      range: true,
      start: range.start,
      end: range.end,
      nextTime: date.getTime()
    };
  });
  return singles.concat(ranges).sort((a, b) => a.nextTime - b.nextTime || String(a.label || '').localeCompare(String(b.label || '')));
}

function nextHolidayDate(item, today) {
  const [year, month, day] = String(item.date || '').split('-').map(Number);
  if (item.repeatYearly && month && day) {
    let date = new Date(today.getFullYear(), month - 1, day);
    if (date < today) date = new Date(today.getFullYear() + 1, month - 1, day);
    return date;
  }
  return new Date(year || today.getFullYear(), (month || 1) - 1, day || 1);
}

function nextRangeDate(range, today) {
  const start = parseYmd(range.start);
  const end = parseYmd(range.end);
  if (!start || !end) return new Date(2999, 0, 1);
  if (today >= start && today <= end) return today;
  return start;
}

function formatHolidayDate(dateText) {
  const [year, month, day] = String(dateText || '').split('-').map(Number);
  if (!year || !month || !day) return dateText || '-';
  return new Date(year, month - 1, day).toLocaleDateString(undefined, {day: 'numeric', month: 'short', year: 'numeric'});
}

function parseYmd(dateText) {
  const [year, month, day] = String(dateText || '').split('-').map(Number);
  if (!year || !month || !day) return null;
  const date = new Date(year, month - 1, day);
  date.setHours(0, 0, 0, 0);
  return date;
}

function holidayMatchesSearch(item) {
  const query = state.holidaySearch.trim().toLowerCase();
  if (!query) return true;
  return String(item.label || '').toLowerCase().includes(query) || String(item.date || '').includes(query);
}

function applyHolidayFilter() {
  const query = state.holidaySearch.trim().toLowerCase();
  $$('[data-holiday-row]').forEach(row => {
    row.classList.toggle('hidden', query && !row.dataset.search.includes(query));
  });
}

function nextHolidaySummary(doc) {
  const today = new Date();
  today.setHours(0, 0, 0, 0);
  const items = sortedHolidayItems(doc).filter(item => item.nextTime >= today.getTime());
  if (!items.length) return null;
  const item = items[0];
  if (item.range) return {label: item.label, when: `${formatHolidayDate(item.start)} to ${formatHolidayDate(item.end)}`};
  return {label: item.label || 'Holiday', when: formatHolidayDate(item.date)};
}

function sameHolidayDate(item, date, repeatYearly) {
  if (item.date === date) return true;
  if (!repeatYearly && !item.repeatYearly) return false;
  return String(item.date || '').slice(5) === String(date || '').slice(5);
}

async function deleteVacationRange(index) {
  await runAction(async () => {
    const doc = await getHolidays();
    doc.ranges = doc.ranges || [];
    doc.ranges.splice(index, 1);
    await saveHolidays(doc);
    toast('Vacation range deleted.');
    renderHolidays();
  });
}

async function deleteHoliday(index) {
  await runAction(async () => {
    const doc = await getHolidays();
    doc.single.splice(index, 1);
    await saveHolidays(doc);
    toast('Holiday deleted.');
    renderHolidays();
  });
}

function exportHolidaysJson() {
  getHolidays().then(doc => downloadText('smart-bell-holidays.json', JSON.stringify(doc, null, 2), 'application/json'));
}

async function exportHolidaysCsv() {
  const doc = await getHolidays();
  const rows = [['type', 'date', 'endDate', 'name', 'repeatYearly']];
  (doc.single || []).forEach(item => rows.push(['single', item.date || '', '', item.label || '', item.repeatYearly ? 'true' : 'false']));
  (doc.ranges || []).forEach(item => rows.push(['range', item.start || '', item.end || '', item.label || '', 'false']));
  (doc.weekly || []).forEach(day => rows.push(['weekly', String(day), '', weekdayName(day), 'true']));
  const csv = rows.map(row => row.map(csvCell).join(',')).join('\r\n');
  downloadText('smart-bell-holidays.csv', csv, 'text/csv');
}

function downloadText(filename, text, type) {
  const blob = new Blob([text], {type});
  const url = URL.createObjectURL(blob);
  const link = document.createElement('a');
  link.href = url;
  link.download = filename;
  link.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}

function csvCell(value) {
  const text = String(value ?? '');
  return /[",\n\r]/.test(text) ? `"${text.replace(/"/g, '""')}"` : text;
}

function weekdayName(day) {
  return ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'][Number(day)] || '';
}

async function importHolidayFile(event) {
  const file = event.target.files?.[0];
  if (!file) return;
  await runAction(async () => {
    const text = await file.text();
    const incoming = file.name.toLowerCase().endsWith('.json') ? holidaysFromJson(JSON.parse(text)) : holidaysFromCsv(text);
    const normalized = normalizeHolidayDoc(incoming);
    const before = await getHolidays();
    const merged = mergeHolidayDocs(before, normalized);
    const counts = importCounts(before, merged);
    await saveHolidays(merged);
    toast(`Imported ${counts.total} holiday item${counts.total === 1 ? '' : 's'}.`);
    renderHolidays();
  });
  event.target.value = '';
}

function holidaysFromJson(value) {
  if (Array.isArray(value)) {
    return {single: value, weekly: [], ranges: []};
  }
  if (value.holidays && Array.isArray(value.holidays)) {
    return {single: value.holidays, weekly: value.weekly || [], ranges: value.ranges || []};
  }
  if (value.single || value.weekly || value.ranges) {
    return value;
  }
  if (value.date || value.name || value.label) {
    return {single: [value], weekly: [], ranges: []};
  }
  return {single: [], weekly: [], ranges: []};
}

function holidaysFromCsv(text) {
  const lines = text.split(/\r?\n/).map(line => line.trim()).filter(Boolean);
  const doc = {single: [], weekly: [], ranges: []};
  const startIndex = lines[0]?.toLowerCase().includes('date') ? 1 : 0;
  for (let i = startIndex; i < lines.length; i++) {
    const [typeOrDate, dateOrName, endDate, name, repeat] = parseCsvLine(lines[i]);
    const type = String(typeOrDate || '').toLowerCase();
    if (type === 'range') doc.ranges.push({start: dateOrName, end: endDate, label: name || 'Vacation'});
    else if (type === 'weekly') doc.weekly.push(Number(dateOrName));
    else if (type === 'single') doc.single.push({date: dateOrName, label: name || 'Holiday', repeatYearly: String(repeat).toLowerCase() === 'true'});
    else doc.single.push({date: typeOrDate, label: dateOrName || 'Holiday', repeatYearly: String(endDate).toLowerCase() === 'true'});
  }
  return doc;
}

function parseCsvLine(line) {
  const cells = [];
  let value = '';
  let quoted = false;
  for (let i = 0; i < line.length; i++) {
    const ch = line[i];
    if (ch === '"' && line[i + 1] === '"') {
      value += '"';
      i++;
    } else if (ch === '"') {
      quoted = !quoted;
    } else if (ch === ',' && !quoted) {
      cells.push(value);
      value = '';
    } else {
      value += ch;
    }
  }
  cells.push(value);
  return cells.map(cell => cell.trim());
}

function normalizeHolidayDoc(doc) {
  return {
    single: (doc.single || []).map(normalizeSingleHoliday).filter(Boolean),
    weekly: [...new Set((doc.weekly || []).map(Number).filter(day => day >= 0 && day <= 6))],
    ranges: (doc.ranges || []).map(normalizeRangeHoliday).filter(Boolean)
  };
}

function normalizeSingleHoliday(item) {
  if (!item) return null;
  const date = item.date || item.day || item.holidayDate;
  if (!isYmd(date)) return null;
  return {
    date,
    label: item.label || item.name || item.title || item.holiday || 'Holiday',
    repeatYearly: item.repeatYearly === true || item.repeat === true || String(item.repeatYearly || item.repeat || '').toLowerCase() === 'true'
  };
}

function normalizeRangeHoliday(item) {
  if (!item) return null;
  const start = item.start || item.startDate || item.from;
  const end = item.end || item.endDate || item.to;
  if (!isYmd(start) || !isYmd(end)) return null;
  return {
    start,
    end,
    label: item.label || item.name || item.title || 'Vacation'
  };
}

function isYmd(value) {
  return /^\d{4}-\d{2}-\d{2}$/.test(String(value || ''));
}

function importCounts(before, after) {
  return {
    single: Math.max(0, (after.single || []).length - (before.single || []).length),
    weekly: Math.max(0, (after.weekly || []).length - (before.weekly || []).length),
    ranges: Math.max(0, (after.ranges || []).length - (before.ranges || []).length),
    get total() {
      return this.single + this.weekly + this.ranges;
    }
  };
}
    })),
    weekly: [...new Set((doc.weekly || []).map(Number).filter(day => day >= 0 && day <= 6))],
    ranges: (doc.ranges || []).filter(item => item.start && item.end).map(item => ({
      start: item.start,
      end: item.end,
      label: item.label || item.name || 'Vacation'
    }))
  };
}

function mergeHolidayDocs(current, incoming) {
  const doc = normalizeHolidayDoc(current);
  incoming.single.forEach(item => {
    if (!doc.single.some(existing => sameHolidayDate(existing, item.date, item.repeatYearly))) doc.single.push(item);
  });
  incoming.ranges.forEach(item => {
    if (!doc.ranges.some(existing => existing.start === item.start && existing.end === item.end)) doc.ranges.push(item);
  });
  doc.weekly = [...new Set(doc.weekly.concat(incoming.weekly))].sort((a, b) => a - b);
  doc.single.sort((a, b) => a.date.localeCompare(b.date));
  doc.ranges.sort((a, b) => a.start.localeCompare(b.start));
  return doc;
}

function renderTime() {
  const s = state.status || {};
  page('time').innerHTML = `
    <div class="grid time-grid">
      ${metric('RTC Time', `<span id="liveClock">${liveClockText()}</span>`)}
      ${metric('Internet Sync', escapeHtml(s.ntpStatus || 'not-synced'))}
      ${metric('Last Sync Source', escapeHtml(s.lastSyncSource || '-'))}
    </div>
    <div class="card action-row">
      <button onclick="syncBrowserTime()">Sync Now</button>
    </div>`;
}

function renderSettings() {
  const s = state.settings || {};
  page('settings').innerHTML = `
    <div class="page-title"><div><h2>Settings</h2><p>Basic device setup for the bell controller.</p></div></div>
    <div class="card settings-form">
      <label>Device Name<input id="setName" value="${escapeHtml(s.deviceName || '')}"></label>
      <label>Wi-Fi SSID<input id="setSsid" value="${escapeHtml(s.wifi?.staSsid || '')}"></label>
      <label>Wi-Fi Password<input id="setPass" type="password" value="${escapeHtml(s.wifi?.staPassword || '')}"></label>
      <label class="check-row"><input id="setAutoSync" type="checkbox" ${s.autoBrowserSync !== false ? 'checked' : ''}>Auto sync time when dashboard opens</label>
      <div class="action-row">
        <button onclick="saveSettings()">Save Settings</button>
        <button class="danger" onclick="restartDevice()">Restart Device</button>
      </div>
    </div>`;
}

async function saveSettings() {
  await runAction(async () => {
    state.settings.deviceName = $('#setName').value.trim() || 'Smart Bell Controller';
    state.settings.wifi = state.settings.wifi || {};
    state.settings.wifi.staSsid = $('#setSsid').value.trim();
    state.settings.wifi.staPassword = $('#setPass').value;
    state.settings.autoBrowserSync = $('#setAutoSync').checked;
    await api('/api/settings', {method: 'PUT', body: JSON.stringify(state.settings)});
    $('#deviceName').textContent = state.settings.deviceName;
    toast('Settings saved.');
  });
}

async function syncFromBrowserIfNeeded() {
  if (state.settings?.autoBrowserSync === false) return;
  const browserEpoch = Math.floor(Date.now() / 1000);
  const deviceEpoch = state.status?.epoch || 0;
  if (!deviceEpoch || Math.abs(browserEpoch - deviceEpoch) > (state.settings?.browserSyncThresholdSec || 30)) {
    await api('/api/time/sync-browser', {method: 'POST', body: JSON.stringify({epoch: browserEpoch})});
    await refreshStatus();
  }
}

async function runAction(action) {
  if (state.busy) return;
  setBusy(true);
  try {
    await action();
  } catch (error) {
    toast(error.message, 'error');
  } finally {
    setBusy(false);
  }
}

setInterval(() => {
  const clock = $('#liveClock');
  if (clock) clock.textContent = liveClockText();
}, 1000);

setInterval(() => {
  if (!state.token || state.page !== 'home') return;
  refreshStatus(false).catch(error => console.warn(error.message));
}, 20000);

(async function boot() {
  if (!state.token) return showLogin('');
  try {
    await loadBaseData();
    showApp();
    await syncFromBrowserIfNeeded();
  } catch (_) {
    showLogin('Please login.');
  }
})();
