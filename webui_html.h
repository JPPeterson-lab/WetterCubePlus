#pragma once
// HTML-Strings in .h ausgelagert, da der Arduino-Präprozessor
// Raw-String-Literals in .ino falsch parst (beendet String bei " inmitten von R"...").

const char PORTAL_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html lang="de"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WetterCubePlus – Einrichtung</title>
<style>
  body{font-family:Arial,sans-serif;background:#0d1117;color:#e6edf3;margin:0;padding:20px}
  h1{color:#58a6ff;font-size:1.4em}
  .card{background:#161b22;border:1px solid #30363d;border-radius:8px;padding:20px;max-width:420px;margin:auto}
  label{display:block;margin-top:12px;color:#8b949e;font-size:.9em}
  input,select{width:100%;padding:8px;border-radius:4px;border:1px solid #30363d;
    background:#21262d;color:#e6edf3;box-sizing:border-box;margin-top:4px}
  button{margin-top:20px;width:100%;padding:10px;background:#238636;color:#fff;
    border:none;border-radius:4px;font-size:1em;cursor:pointer}
  button:hover{background:#2ea043}
  .hint{font-size:.8em;color:#8b949e;margin-top:4px}
</style></head><body>
<div class="card">
  <h1>WetterCubePlus Einrichtung</h1>
  <form action="/speichern" method="post">
    <label>WLAN-Netzwerk</label>
    <select name="ssid">%NETZWERKE%</select>
    <label>WLAN-Passwort</label>
    <input type="password" name="pass" placeholder="Passwort eingeben">
    <label>Standort (Stadt)</label>
    <input type="text" name="location" placeholder="z.B. Berlin" value="%LOCATION%">
    <p class="hint">Keine Umlaute: Munchen statt Muenchen, Koeln statt Koln</p>
    <label>DWD-Pollenregion</label>
    <select name="dwd_region">%DWD_REGIONEN%</select>
    <button type="submit">Speichern &amp; Neustart</button>
  </form>
</div></body></html>
)rawhtml";

const char PORTAL_OK_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html lang="de"><head>
<meta charset="UTF-8"><title>Gespeichert</title>
<meta http-equiv="refresh" content="5;url=/">
<style>
  body{font-family:Arial;background:#0d1117;color:#e6edf3;text-align:center;padding-top:60px}
  .ok{color:#2ea043;font-size:2em}
</style></head><body>
<div class="ok">Gespeichert</div>
<p>Geraet wird neu gestartet...</p>
</body></html>
)rawhtml";

const char WEBUI_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html lang="de"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WetterCubePlus</title>
<style>
  *{box-sizing:border-box}
  body{font-family:Arial,sans-serif;background:#0d1117;color:#e6edf3;margin:0;padding:16px}
  h1{color:#58a6ff;margin-bottom:4px}
  .ver{color:#8b949e;font-size:.85em;margin-bottom:20px}
  .card{background:#161b22;border:1px solid #30363d;border-radius:8px;padding:16px;margin-bottom:16px}
  h2{color:#e6edf3;font-size:1em;margin:0 0 12px}
  label{display:block;color:#8b949e;font-size:.85em;margin-top:10px}
  input[type=text],input[type=number],select{width:100%;padding:7px;border-radius:4px;
    border:1px solid #30363d;background:#21262d;color:#e6edf3;margin-top:3px}
  input[type=range]{width:100%;margin-top:6px}
  .row{display:flex;align-items:center;justify-content:space-between;margin-top:10px}
  .toggle{position:relative;width:44px;height:24px;flex-shrink:0}
  .toggle input{opacity:0;width:0;height:0}
  .slider{position:absolute;inset:0;background:#30363d;border-radius:12px;cursor:pointer;transition:.3s}
  .slider::before{content:'';position:absolute;width:18px;height:18px;left:3px;bottom:3px;
    background:#fff;border-radius:50%;transition:.3s}
  input:checked+.slider{background:#238636}
  input:checked+.slider::before{transform:translateX(20px)}
  .btn{display:inline-block;padding:9px 16px;border:none;border-radius:4px;cursor:pointer;font-size:.9em;margin-top:6px}
  .btn-gruen{background:#238636;color:#fff}.btn-gruen:hover{background:#2ea043}
  .btn-blau{background:#1f6feb;color:#fff}.btn-blau:hover{background:#388bfd}
  .btn-rot{background:#b91c1c;color:#fff}.btn-rot:hover{background:#dc2626}
  .status{font-size:.85em;color:#8b949e;margin-top:6px}
  .hint{font-size:.78em;color:#8b949e;margin-top:3px}
</style></head><body>
<h1>WetterCubePlus</h1>
<div class="ver">Firmware: %VERSION% | IP: %IP% | wettercubeplus.local</div>

<form action="/speichern" method="post">

<div class="card">
  <h2>Standort &amp; Region</h2>
  <label>Stadt (ohne Umlaute: Munchen, Koeln)</label>
  <input type="text" name="location" value="%LOCATION%">
  <label>DWD Pollenregion</label>
  <select name="dwd_region">%DWD_REGIONEN%</select>
</div>

<div class="card">
  <h2>Warnungen</h2>
  <div class="row"><span>Regenwarnung</span>
    <label class="toggle"><input type="checkbox" name="regen_warn" %RW%><span class="slider"></span></label></div>
  <div class="row"><span>Pollenwarnung</span>
    <label class="toggle"><input type="checkbox" name="pollen_warn" %PW%><span class="slider"></span></label></div>
  <div class="row"><span>DWD-Wetterwarnungen</span>
    <label class="toggle"><input type="checkbox" name="warn_region" %WR%><span class="slider"></span></label></div>
  <label>Pollen-Warnschwelle</label>
  <select name="pol_schw">
    <option value="0" %PS0%>Gering</option>
    <option value="1" %PS1%>Mittel</option>
    <option value="2" %PS2%>Hoch (Standard)</option>
    <option value="3" %PS3%>Stark</option>
  </select>
</div>

<div class="card">
  <h2>Display</h2>
  <label>Helligkeit: <span id="bv">%BRIGHT%</span>%</label>
  <input type="range" name="brightness" min="10" max="100" value="%BRIGHT%"
    oninput="document.getElementById('bv').textContent=this.value">
  <label>Dimm-Timeout</label>
  <select name="dim_time">
    <option value="0" %DT0%>Aus</option>
    <option value="1" %DT1%>1 Minute</option>
    <option value="3" %DT3%>3 Minuten (Standard)</option>
    <option value="5" %DT5%>5 Minuten</option>
    <option value="10" %DT10%>10 Minuten</option>
  </select>
</div>

<button type="submit" class="btn btn-gruen">Einstellungen speichern</button>
</form>

<div class="card">
  <h2>WLAN-Zugangsdaten aendern</h2>
  <p class="hint">SSID/Passwort aendern ohne neu zu flashen.</p>
  <a href="/wlan" class="btn btn-blau">WLAN aendern</a>
</div>

<div class="card">
  <h2>Firmware-Update</h2>
  <div class="status">Aktuelle Version: %VERSION%</div>
  <button type="button" class="btn btn-blau" onclick="checkOta()">Auf Updates pruefen</button>
  <div id="ota_status" class="status"></div>
</div>

<script>
function checkOta(){
  document.getElementById('ota_status').textContent='Pruefe...';
  fetch('/ota_check').then(function(r){return r.json();}).then(function(d){
    if(d.update_available){
      document.getElementById('ota_status').innerHTML=
        'Neue Version: '+d.latest+' &nbsp;'+
        '<button class="btn btn-rot" onclick="doUpdate()">Jetzt installieren</button>';
    } else {
      document.getElementById('ota_status').textContent='Bereits aktuell ('+d.latest+')';
    }
  }).catch(function(){document.getElementById('ota_status').textContent='Fehler beim Pruefen';});
}
function doUpdate(){
  document.getElementById('ota_status').textContent='Update laeuft... bitte warten (ca. 30 Sek.)';
  fetch('/ota_update',{method:'POST'}).then(function(r){return r.text();}).then(function(t){
    document.getElementById('ota_status').textContent=t;
  });
}
</script>
</body></html>
)rawhtml";

const char WLAN_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html lang="de"><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>WLAN aendern</title>
<style>
  body{font-family:Arial;background:#0d1117;color:#e6edf3;padding:20px}
  .card{background:#161b22;border:1px solid #30363d;border-radius:8px;padding:20px;max-width:420px;margin:auto}
  h1{color:#58a6ff;font-size:1.3em}
  label{display:block;color:#8b949e;font-size:.85em;margin-top:10px}
  input,select{width:100%;padding:8px;border-radius:4px;border:1px solid #30363d;
    background:#21262d;color:#e6edf3;box-sizing:border-box;margin-top:3px}
  .btn{width:100%;padding:10px;background:#238636;color:#fff;border:none;border-radius:4px;font-size:1em;cursor:pointer;margin-top:16px}
  .btn:hover{background:#2ea043}
  a{color:#58a6ff}
</style></head><body>
<div class="card">
  <h1>WLAN-Zugangsdaten aendern</h1>
  <form action="/wlan_speichern" method="post">
    <label>Netzwerk</label>
    <select name="ssid">%NETZWERKE%</select>
    <label>Passwort</label>
    <input type="password" name="pass">
    <button type="submit" class="btn">Speichern &amp; Neustart</button>
  </form>
  <br><a href="/">Zurueck</a>
</div></body></html>
)rawhtml";
