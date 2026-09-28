#include "web_page.h"

namespace web_page {

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>步进电机控制</title>
  <style>
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      display: grid;
      place-items: center;
      padding: 20px;
      color: #1f2937;
      font-family: Arial, "Microsoft YaHei", sans-serif;
      background: #f3f4f6;
    }
    main {
      width: min(420px, 100%);
      padding: 28px;
      border: 1px solid #d1d5db;
      border-radius: 12px;
      background: #ffffff;
      box-shadow: 0 4px 14px rgba(0, 0, 0, .08);
    }
    h1 { margin: 0 0 22px; text-align: center; font-size: 24px; }
    .status {
      display: flex;
      align-items: center;
      gap: 10px;
      padding: 14px;
      border-radius: 8px;
      background: #f3f4f6;
    }
    .dot { width: 10px; height: 10px; border-radius: 50%; background: #16a34a; }
    .offline .dot { background: #dc2626; }
    #state { font-weight: bold; }
    .info {
      margin: 18px 0;
      padding: 0;
      list-style: none;
      line-height: 2;
      color: #4b5563;
    }
    .info b { color: #111827; }
    .speed-settings { margin-bottom: 20px; }
    .speed-settings label { display: block; margin-bottom: 8px; }
    .speed-row { display: grid; grid-template-columns: 1fr 110px; gap: 10px; }
    .speed-row input {
      width: 100%;
      min-width: 0;
      padding: 10px;
      border: 1px solid #9ca3af;
      border-radius: 7px;
      font: inherit;
    }
    .hint { margin: 8px 0 0; color: #6b7280; font-size: 13px; line-height: 1.5; }
    .controls { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
    button {
      min-height: 46px;
      border: 0;
      border-radius: 7px;
      color: white;
      font-size: 15px;
      cursor: pointer;
      background: #2563eb;
    }
    button:active { opacity: .8; }
    button:disabled { opacity: .5; cursor: wait; }
    .stop { grid-column: 1 / -1; background: #dc2626; }
    #message { min-height: 22px; margin: 14px 0 0; text-align: center; color: #dc2626; }
    @media (max-width: 380px) {
      main { padding: 20px; }
      .controls { grid-template-columns: 1fr; }
      .stop { grid-column: auto; }
    }
  </style>
</head>
<body>
  <main>
    <h1>步进电机控制</h1>

    <div id="connection" class="status">
      <span class="dot"></span>
      <span id="state">正在连接设备……</span>
    </div>

    <ul class="info">
      <li>Wi-Fi：<b id="ssid">--</b></li>
      <li>IP 地址：<b id="ip">--</b></li>
      <li>信号强度：<b><span id="rssi">--</span> dBm</b></li>
      <li>当前设定转速：<b><span id="current-rpm">--</span> RPM</b></li>
    </ul>

    <form id="speed-form" class="speed-settings">
      <label for="rpm">转速（RPM，转/分钟）</label>
      <div class="speed-row">
        <input id="rpm" name="rpm" type="number" min="1" max="15" step="0.1"
               required inputmode="decimal" aria-describedby="speed-hint">
        <button id="set-speed" type="submit">设置转速</button>
      </div>
      <p id="speed-hint" class="hint">范围 1～15，支持一位小数。设置后立即生效；待机时不会启动电机。</p>
    </form>

    <div class="controls">
      <button type="button" onclick="controlMotor(-4096)">反转一圈</button>
      <button type="button" onclick="controlMotor(4096)">正转一圈</button>
      <button type="button" class="stop" onclick="stopMotor()">停止</button>
    </div>

    <p id="message" aria-live="polite"></p>
  </main>

  <script>
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
  </script>
</body>
</html>
)HTML";

}  // namespace web_page
