const state = document.querySelector('#state');
const message = document.querySelector('#message');
const connection = document.querySelector('#connection');
const rpmInput = document.querySelector('#rpm');
const speedButton = document.querySelector('#set-speed');
let speedEdited = false;
rpmInput.addEventListener('input', () => { speedEdited = true; });
document.querySelector('#speed-form').addEventListener('submit', setSpeed);

function render(data) {
  state.textContent = data.motor.busy ? '电机运行中' : '电机待机';
  document.querySelector('#ssid').textContent = data.ssid;
  document.querySelector('#ip').textContent = data.ip;
  document.querySelector('#rssi').textContent = data.rssi;
  document.querySelector('#current-rpm').textContent = data.motor.rpm.toFixed(1);
  if (!speedEdited) rpmInput.value = data.motor.rpm.toFixed(1);
  connection.classList.remove('offline');
}

async function setSpeed(event) {
  event.preventDefault();
  if (speedButton.disabled || !rpmInput.reportValidity()) return;
  const submittedValue = rpmInput.value;
  const rpm = Number(submittedValue);
  if (!Number.isFinite(rpm) || rpm < 1 || rpm > 15) {
    message.textContent = '请输入 1～15 之间的转速';
    return;
  }
  speedButton.disabled = true;
  message.textContent = '正在设置转速……';
  try {
    const response = await fetch('/api/motor/speed?rpm=' + encodeURIComponent(rpm),
                                 {method:'POST'});
    if (response.status === 400) {
      message.textContent = '转速无效，请输入 1～15 之间的数值';
      return;
    }
    if (!response.ok) throw new Error();
    const data = await response.json();
    if (rpmInput.value === submittedValue) speedEdited = false;
    render(data);
    message.textContent = '转速已设置为 ' + data.motor.rpm.toFixed(1) + ' RPM';
  } catch (error) { showOffline('转速设置失败，请检查 Wi-Fi 连接后重试'); }
  finally { speedButton.disabled = false; }
}

function showOffline(text) {
  state.textContent = '设备连接失败';
  connection.classList.add('offline');
  message.textContent = text;
}

async function refresh() {
  try {
    const response = await fetch('/api/status', {cache:'no-store'});
    if (!response.ok) throw new Error();
    render(await response.json());
  } catch (error) { showOffline('请检查 Wi-Fi 连接'); }
}

async function controlMotor(steps) {
  message.textContent = '正在发送指令……';
  try {
    const response = await fetch('/api/motor?steps=' + steps, {method:'POST'});
    if (!response.ok) throw new Error();
    render(await response.json());
    message.textContent = '指令已发送';
  } catch (error) { showOffline('指令发送失败'); }
}

async function stopMotor() {
  try {
    const response = await fetch('/api/motor/stop', {method:'POST'});
    if (!response.ok) throw new Error();
    render(await response.json());
    message.textContent = '电机已停止';
  } catch (error) { showOffline('停止指令发送失败'); }
}

refresh();
setInterval(refresh, 3000);
