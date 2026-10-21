// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Jan Althaus (JPPeterson-lab)
#pragma once
#include "webui_theme.h"
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
    <p class="hint">Keine Umlaute: Munchen statt München, Koln statt Koeln</p>
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
)rawhtml" WEBUI_THEME_HEAD WEBUI_THEME_CSS R"rawhtml(
<style>
  *{box-sizing:border-box}
  body{font-family:Arial,sans-serif;background:var(--bg);color:var(--txt);margin:0;padding:16px;max-width:860px;margin:auto}
  h1{color:var(--acc);margin:0 0 4px}
  .top{display:flex;justify-content:space-between;align-items:flex-start;gap:10px;margin-bottom:14px}
  .ver{color:var(--mut);font-size:.85em}
  .tabs{display:flex;gap:6px;overflow-x:auto;margin:0 0 16px;padding-bottom:4px;-webkit-overflow-scrolling:touch}
  .tabs a{flex:0 0 auto;padding:9px 16px;border-radius:8px;background:var(--tab);color:var(--txt);text-decoration:none;font-size:.95em;border:1px solid var(--bd);white-space:nowrap}
  .tabs a.aktiv{background:#1f6feb;border-color:#1f6feb;color:#fff}
  .tab{display:none}
  .card{background:var(--card);border:1px solid var(--bd);border-radius:8px;padding:16px;margin-bottom:16px}
  h2{color:var(--txt);font-size:1em;margin:0 0 12px}
  h3.sub{margin:16px 0 4px;font-size:.95em;color:var(--txt);border-top:1px solid var(--bd);padding-top:14px}
  label{display:block;color:var(--mut);font-size:.85em;margin-top:10px}
  input[type=text],input[type=number],select{width:100%;padding:7px;border-radius:4px;
    border:1px solid var(--bd);background:var(--in);color:var(--txt);margin-top:3px}
  input[type=range]{width:100%;margin-top:6px}
  input[type=file]{width:100%;margin-top:6px;color:var(--txt)}
  .row{display:flex;align-items:center;justify-content:space-between;gap:10px;margin-top:10px}
  .row span{color:var(--txt);font-size:.95em}
  .grid2{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:8px}
  .toggle{position:relative;width:44px;height:24px;flex-shrink:0;margin:0}
  .toggle input{opacity:0;width:0;height:0}
  .slider{position:absolute;inset:0;background:var(--slider);border-radius:12px;cursor:pointer;transition:.3s}
  .slider::before{content:'';position:absolute;width:18px;height:18px;left:3px;bottom:3px;
    background:#fff;border-radius:50%;transition:.3s}
  input:checked+.slider{background:#238636}
  input:checked+.slider::before{transform:translateX(20px)}
  .btn{display:inline-block;padding:9px 16px;border:none;border-radius:4px;cursor:pointer;font-size:.9em;margin-top:6px;text-decoration:none}
  .btn-gruen{background:#238636;color:#fff}.btn-gruen:hover{background:#2ea043}
  .btn-blau{background:#1f6feb;color:#fff}.btn-blau:hover{background:#388bfd}
  .btn-rot{background:#b91c1c;color:#fff}.btn-rot:hover{background:#dc2626}
  .status{font-size:.85em;color:var(--mut);margin-top:6px}
  .hint{font-size:.78em;color:var(--mut);margin-top:3px}
  .savebar{position:sticky;bottom:0;background:var(--bg);border-top:1px solid var(--bd);padding:10px 0;margin-top:4px;display:flex;align-items:center;gap:12px;z-index:5}
  .savebar .btn{margin:0}
  #dirty{font-size:.85em;color:#d29922;display:none}
  .col{flex:1}
</style></head><body>
<div class="top">
  <div><h1>WetterCubePlus</h1>
  <div class="ver">Firmware: %VERSION% | IP: %IP% | wettercubeplus.local</div></div>
  <button id="wcpThemeBtn" class="themebtn" onclick="wcpTheme()" title="Hell oder dunkel"></button>
</div>

<div class="tabs" id="tabs">
  <a href="#anzeige" data-t="anzeige">Anzeige</a>
  <a href="#wetter" data-t="wetter">Wetter &amp; Daten</a>
  <a href="#ampel" data-t="ampel">Ampel</a>
  <a href="#system" data-t="system">System</a>
</div>

<form action="/speichern" method="post" id="einst">

<div class="tab" id="t-anzeige">
<div class="card">
  <h2>Display</h2>
  <label>Helligkeit: <span id="bv">%BRIGHT%</span>%</label>
  <input type="range" name="brightness" min="10" max="100" value="%BRIGHT%"
    oninput="document.getElementById('bv').textContent=this.value">
  <label>Dimm-Timeout</label>
  <select name="dim_time">
    <option value="0" %DT0%>Aus</option>
    <option value="-1" %DTT%>30 Sekunden (Test)</option>
    <option value="1" %DT1%>1 Minute</option>
    <option value="3" %DT3%>3 Minuten (Standard)</option>
    <option value="5" %DT5%>5 Minuten</option>
    <option value="10" %DT10%>10 Minuten</option>
  </select>
</div>

<div class="card">
  <h2>Nachtmodus</h2>
  <p class="hint">In der gew&auml;hlten Zeit leuchtet das Display mit eigener, niedriger Helligkeit.</p>
  <div class="row"><span>Nachtmodus aktiv</span>
    <label class="toggle"><input type="checkbox" name="night_mode" %NM%><span class="slider"></span></label></div>
  <div style="display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:8px">
    <div>
      <label>Von</label>
      <select name="night_from">%NIGHT_FROM_OPTIONS%</select>
    </div>
    <div>
      <label>Bis</label>
      <select name="night_to">%NIGHT_TO_OPTIONS%</select>
    </div>
  </div>
  <label style="margin-top:8px;display:block">Helligkeit nachts: <span id="nbv">%NBR%</span>%</label>
  <input type="range" name="night_bright" min="0" max="50" value="%NBR%"
    oninput="document.getElementById('nbv').textContent=this.value">
</div>

<div class="card">
  <h2>Hauptscreen</h2>
  <label>Rechtes Feld</label>
  <select name="main_box">
    <option value="0" %MB0%>Pollen (meine Allergene)</option>
    <option value="1" %MB1%>Biowetter (meine Kategorien)</option>
  </select>
  <p class="hint">Zeigt die zwei st&auml;rksten Werte: bei Pollen aus deinen Allergenen (Karte &bdquo;Meine Allergene&ldquo;), beim Biowetter aus den vier Kategorien der Wetterf&uuml;hligkeit (Karte &bdquo;Biowetter&ldquo;). Auch am Ger&auml;t im Men&uuml; umschaltbar.</p>
  <div class="row"><span>Regenbalken im Temperaturverlauf</span>
    <label class="toggle"><input type="checkbox" name="vl_regen" %VR%><span class="slider"></span></label></div>
</div>

<div class="card">
  <h2>Hinweis-Kachel (oben links)</h2>
  <p class="hint">Eine Kachel oben links zeigt Regen, Sturm, Frost und starke UV-Strahlung. Neue Hinweise blinken, bis du die Kachel antippst; danach leuchtet sie dauerhaft, solange der Hinweis gilt. Bei mehreren Hinweisen hat Regen Vorrang, dann Sturm, dann Frost, dann UV.</p>
  <div class="row"><span>Regen</span>
    <label class="toggle"><input type="checkbox" name="hw_regen" %HR%><span class="slider"></span></label></div>
  <div class="row"><span>Sturm (B&ouml;en ab 62 km/h in den n&auml;chsten 6 Stunden)</span>
    <label class="toggle"><input type="checkbox" name="hw_wind" %HW%><span class="slider"></span></label></div>
  <div class="row"><span>Frost (0 &deg;C oder darunter in den n&auml;chsten 12 Stunden)</span>
    <label class="toggle"><input type="checkbox" name="hw_frost" %HF%><span class="slider"></span></label></div>
  <div class="row"><span>UV-Index 6 oder h&ouml;her</span>
    <label class="toggle"><input type="checkbox" name="hw_uv" %HU%><span class="slider"></span></label></div>
</div>
</div>

<div class="tab" id="t-wetter">
<div class="card">
  <h2>Standort &amp; Region</h2>
  <label>Stadt (ohne Umlaute: Munchen, Koeln)</label>
  <input type="text" name="location" value="%LOCATION%">
  <label>DWD Pollenregion</label>
  <select name="dwd_region">%DWD_REGIONEN%</select>
</div>

<div class="card">
  <h2>Warnungen</h2>
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
  <h2>Meine Allergene</h2>
  <p class="hint">Die Pollenwarnung und der Gesundheitsscreen ber&uuml;cksichtigen nur die gew&auml;hlten Pollen. Hasel, Esche und Roggen kommen aus der DWD-Tagesvorhersage, die &uuml;brigen aus der Stundenvorhersage.</p>
  %POL_AUSWAHL%
</div>

<div class="card">
  <h2>Biowetter</h2>
  <label>DWD Biowetter-Zone</label>
  <select name="bio_zone">%BIO_ZONE_OPTIONS%</select>
  <h3 class="sub">Wetterf&uuml;hligkeit &ndash; angezeigte Kategorien</h3>
  <p class="hint">Wähle die 4 Kategorien für den Gesundheitsscreen.</p>
  <div style="display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:8px">
    <div><label>Zeile 1</label><select name="ws_cat0">%WS_CAT_OPTIONS0%</select></div>
    <div><label>Zeile 2</label><select name="ws_cat1">%WS_CAT_OPTIONS1%</select></div>
    <div><label>Zeile 3</label><select name="ws_cat2">%WS_CAT_OPTIONS2%</select></div>
    <div><label>Zeile 4</label><select name="ws_cat3">%WS_CAT_OPTIONS3%</select></div>
  </div>
</div>
</div>

<div class="savebar" id="savebar">
  <button type="submit" class="btn btn-gruen">Einstellungen speichern</button>
  <span id="dirty">&bull; ungespeicherte &Auml;nderungen</span>
</div>
</form>

<div class="tab" id="t-ampel">
<div class="card">
  <h2>Ampel-Konfiguration</h2>
  <p class="hint">Einstellungen fuer das Hardware-Addon (Ampel). Aktuell: <strong id="amp_d">…</strong> &nbsp;|&nbsp; Aktiv: <strong id="amp_a">…</strong></p>
  <form action="/api/ampel_save" method="post">
  <label>Was die Ampel anzeigt</label>
  <select name="amp_modus">
    <option value="0" %AMP_M0%>Temperatur</option>
    <option value="1" %AMP_M1%>Pollen (meine Allergene)</option>
    <option value="2" %AMP_M2%>Luftqualit&auml;t</option>
  </select>
  <p class="hint">Pollen: Gr&uuml;n f&uuml;r keine bis geringe, Gelb f&uuml;r mittlere bis hohe, Rot f&uuml;r starke Belastung. Luftqualit&auml;t (europ&auml;ischer AQI): Gr&uuml;n bis 40, Gelb bis 60, Rot dar&uuml;ber. Die Temperaturschwellen unten gelten nur im Modus Temperatur.</p>
  <div class="row">
    <div class="col">
      <label>Gruen min (°C)</label>
      <input type="number" name="amp_gn_min" value="%AMP_GN_MIN%" min="-30" max="60">
    </div>
    <div class="col">
      <label>Gruen max (°C)</label>
      <input type="number" name="amp_gn_max" value="%AMP_GN_MAX%" min="-30" max="60">
    </div>
  </div>
  <div class="row">
    <div class="col">
      <label>Gelb min (°C)</label>
      <input type="number" name="amp_ge_min" value="%AMP_GE_MIN%" min="-30" max="60">
    </div>
    <div class="col">
      <label>Gelb max (°C)</label>
      <input type="number" name="amp_ge_max" value="%AMP_GE_MAX%" min="-30" max="60">
    </div>
  </div>
  <div class="row">
    <div class="col">
      <label>Rot min (°C)</label>
      <input type="number" name="amp_ro_min" value="%AMP_RO_MIN%" min="-30" max="60">
    </div>
    <div class="col">
      <label>Rot max (°C)</label>
      <input type="number" name="amp_ro_max" value="%AMP_RO_MAX%" min="-30" max="60">
    </div>
  </div>
  <button type="submit" class="btn btn-gruen">Ampel-Schwellen speichern</button>
  </form>
</div>
</div>

<div class="tab" id="t-system">
<div class="card">
  <h2>Passwort f&uuml;r die Web-Oberfl&auml;che</h2>
  <p class="hint">%WPSTATUS%</p>
  <p class="hint">Mit Passwort fragt das Ger&auml;t beim &Ouml;ffnen dieser Seite nach Benutzername (<b>admin</b>) und Passwort. Die WetterAmpel liest weiter die Werte (<code>/api/ampel</code>). Das Passwort wird im Heimnetz unverschl&uuml;sselt (HTTP) &uuml;bertragen: Es sch&uuml;tzt vor Zugriffen anderer Ger&auml;te und fremder Webseiten, nicht vor jemandem, der den Netzverkehr mitliest. <b>Vergessen?</b> Auf der Anmeldeseite f&uuml;hrt &bdquo;Passwort vergessen?&ldquo; weiter: Am Display erscheint ein Code, mit dem du das Passwort l&ouml;schst.</p>
  <form action="/passwort_speichern" method="post" onsubmit="var w=this.pw_weg;return !(w&&w.checked)||confirm('Passwort entfernen? Dann kann jeder im Heimnetz die Einstellungen aendern.')">
    <label>Neues Passwort (mindestens 6 Zeichen)</label>
    <input type="password" name="pw1" autocomplete="new-password">
    <label>Wiederholen</label>
    <input type="password" name="pw2" autocomplete="new-password">
    <button type="submit" class="btn btn-blau" style="margin-top:8px">Passwort speichern</button>
    <label style="display:flex;gap:8px;align-items:center;margin-top:10px"><input type="checkbox" name="pw_weg" style="width:auto"> Passwort entfernen</label>
  </form>
</div>

<div class="card">
  <h2>Firmware-Update</h2>
  <div class="status">Aktuelle Version: %VERSION%</div>
  <button type="button" class="btn btn-blau" onclick="checkOta()">Auf Updates pruefen</button>
  <a href="/log" target="_blank" class="btn btn-blau">Log ansehen</a>
  <div id="ota_status" class="status"></div>
  <div class="status" style="margin-top:10px">Hinweis: Waehrend des Updates zeigt das Display einen Fortschrittsbalken. Das Geraet dabei <b>nicht vom Strom trennen</b> &ndash; es startet danach selbst neu (ca. 1 Minute). Das Geraet prueft ausserdem selbst regelmaessig auf Updates und zeigt oben rechts auf dem Hauptscreen ein Update-Symbol.</div>
</div>

<div class="card">
  <h2>Einstellungen sichern</h2>
  <p class="hint">Speichert alle Einstellungen (ohne WLAN-Passwort) als Datei und spielt sie bei Bedarf wieder ein, zum Beispiel nach einer Neuinstallation mit gel&ouml;schtem Speicher.</p>
  <a href="/backup" class="btn btn-blau">Einstellungen herunterladen</a>
  <label>Sicherung wiederherstellen</label>
  <input type="file" id="restore_file" accept=".json,application/json">
  <button type="button" class="btn btn-rot" onclick="restoreSettings()">Wiederherstellen</button>
  <div id="restore_status" class="status"></div>
</div>

<div class="card">
  <h2>WLAN-Zugangsdaten aendern</h2>
  <p class="hint">SSID/Passwort aendern ohne neu zu flashen.</p>
  <a href="/wlan" class="btn btn-blau">WLAN aendern</a>
</div>

<div class="card">
  <h2>Datenquellen</h2>
  <p class="hint">Wetterdaten: <a href="https://open-meteo.com" target="_blank" rel="noopener">Weather data by Open-Meteo.com</a> (<a href="https://open-meteo.com/en/licence" target="_blank" rel="noopener">CC BY 4.0</a>, die kostenlose Schnittstelle ist nur f&uuml;r nicht-kommerzielle Nutzung). Pollen und Luftqualit&auml;t: Copernicus Atmosphere Monitoring Service (CAMS) &uuml;ber Open-Meteo. Warnungen, Pollenflug, Biowetter und Radar: <a href="https://opendata.dwd.de" target="_blank" rel="noopener">Deutscher Wetterdienst</a> (Opendata).</p>
</div>
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
function restoreSettings(){
  var f=document.getElementById('restore_file').files[0];
  var st=document.getElementById('restore_status');
  if(!f){st.textContent='Bitte zuerst eine Datei waehlen.';return;}
  if(!confirm('Die Einstellungen aus der Datei jetzt einspielen? Das Geraet startet danach neu.')) return;
  var r=new FileReader();
  r.onload=function(){
    st.textContent='Wird eingespielt ...';
    fetch('/restore',{method:'POST',headers:{'Content-Type':'application/json'},body:r.result})
      .then(function(x){return x.text().then(function(tx){return {ok:x.ok,t:tx};});})
      .then(function(o){st.textContent=o.ok?'Eingespielt. Das Geraet startet neu (Seite in ca. 30 Sekunden neu laden).':'Fehler: '+o.t;})
      .catch(function(){st.textContent='Verbindung beendet. Wenn das Geraet gerade neu startet, ist das normal.';});
  };
  r.readAsText(f);
}
function doUpdate(){
  if(!confirm('Update jetzt installieren?\n\nDas Geraet NICHT vom Strom trennen. Es startet danach selbst neu (ca. 1 Minute).')) return;
  document.getElementById('ota_status').textContent='Update laeuft... Geraet NICHT ausschalten (ca. 1 Minute).';
  fetch('/ota_update',{method:'POST'}).then(function(r){return r.text();}).then(function(t){
    document.getElementById('ota_status').textContent=(t==='ok')?'Update installiert. Das Geraet startet neu ...':'Fehler: '+t;
  }).catch(function(){
    document.getElementById('ota_status').textContent='Verbindung beendet. Wenn das Geraet gerade neu startet, ist das normal.';
  });
}
function ladeAmpel(){
  fetch('/api/ampel').then(function(r){return r.json();}).then(function(d){
    document.getElementById('amp_d').textContent=d.detail||(d.temperature.toFixed(1)+' \u00B0C');
    var farben={'green':'Gruen','yellow':'Gelb','red':'Rot','none':'keine'};
    document.getElementById('amp_a').textContent=farben[d.active]||d.active;
  }).catch(function(){});
}
ladeAmpel();

var TABS=['anzeige','wetter','ampel','system'];
function zeige(t){
  if(TABS.indexOf(t)<0)t='anzeige';
  for(var i=0;i<TABS.length;i++){
    var s=document.getElementById('t-'+TABS[i]);if(s)s.style.display=(TABS[i]===t)?'block':'none';
    var a=document.querySelector('.tabs a[data-t="'+TABS[i]+'"]');if(a)a.className=(TABS[i]===t)?'aktiv':'';
  }
  var sb=document.getElementById('savebar');if(sb)sb.style.display=(t==='anzeige'||t==='wetter')?'flex':'none';
  try{localStorage.setItem('wcp-tab',t);}catch(e){}
  if(location.hash!=='#'+t&&history.replaceState)history.replaceState(null,'','#'+t);
}
function startTab(){
  var t=location.hash.replace('#','');
  if(!t){try{t=localStorage.getItem('wcp-tab')||'';}catch(e){}}
  zeige(t);
}
window.addEventListener('hashchange',function(){zeige(location.hash.replace('#',''));});
document.getElementById('tabs').addEventListener('click',function(e){var a=e.target.closest?e.target.closest('a[data-t]'):null;if(a){e.preventDefault();zeige(a.getAttribute('data-t'));}});
document.getElementById('einst').addEventListener('input',function(){var d=document.getElementById('dirty');if(d)d.style.display='inline';});
wcpThemeLabel();startTab();
</script>
</body></html>
)rawhtml";

const char WLAN_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html lang="de"><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>WLAN aendern</title>
)rawhtml" WEBUI_THEME_HEAD WEBUI_THEME_CSS R"rawhtml(
<style>
  body{font-family:Arial;background:var(--bg);color:var(--txt);padding:20px}
  .card{background:var(--card);border:1px solid var(--bd);border-radius:8px;padding:20px;max-width:420px;margin:auto}
  h1{color:var(--acc);font-size:1.3em}
  label{display:block;color:var(--mut);font-size:.85em;margin-top:10px}
  input,select{width:100%;padding:8px;border-radius:4px;border:1px solid var(--bd);
    background:var(--in);color:var(--txt);box-sizing:border-box;margin-top:3px}
  .btn{width:100%;padding:10px;background:#238636;color:#fff;border:none;border-radius:4px;font-size:1em;cursor:pointer;margin-top:16px}
  .btn:hover{background:#2ea043}
  a{color:var(--acc)}
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
  <br><a href="/#system">Zurueck</a>
</div></body></html>
)rawhtml";
