// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Jan Althaus (JPPeterson-lab)
#pragma once
// Gemeinsames Design der Web-Seiten: Hell/Dunkel über CSS-Variablen; der Umschalter merkt sich die Wahl im Browser ("wcp-theme").
// Ohne gemerkte Wahl gilt die Einstellung des Browsers. Als Makros, damit alle Seiten dasselbe Aussehen teilen.

// im <head> ganz vorn: setzt das Design vor dem ersten Zeichnen (kein Aufblitzen)
#define WEBUI_THEME_HEAD R"rawjs(<script>
(function(){var t=null;try{t=localStorage.getItem('wcp-theme');}catch(e){}
if(t!=='light'&&t!=='dark')t=(window.matchMedia&&window.matchMedia('(prefers-color-scheme: light)').matches)?'light':'dark';
document.documentElement.setAttribute('data-theme',t);})();
function wcpThemeLabel(){var b=document.getElementById('wcpThemeBtn');if(!b)return;
b.textContent=document.documentElement.getAttribute('data-theme')==='light'?'☾ Dunkel':'☀ Hell';}
function wcpTheme(){var t=document.documentElement.getAttribute('data-theme')==='light'?'dark':'light';
document.documentElement.setAttribute('data-theme',t);try{localStorage.setItem('wcp-theme',t);}catch(e){}wcpThemeLabel();}
</script>)rawjs"

// Farben: dunkel (Standard) und hell
#define WEBUI_THEME_CSS R"rawcss(<style>
:root,[data-theme=dark]{--bg:#0d1117;--card:#161b22;--bd:#30363d;--txt:#e6edf3;--mut:#8b949e;--in:#21262d;--acc:#58a6ff;--slider:#30363d;--tab:#161b22;color-scheme:dark}
[data-theme=light]{--bg:#f3f5f8;--card:#ffffff;--bd:#d0d7de;--txt:#1f2328;--mut:#59636e;--in:#f6f8fa;--acc:#0969da;--slider:#b8c0c8;--tab:#e6eaef;color-scheme:light}
.themebtn{background:var(--tab);color:var(--txt);border:1px solid var(--bd);border-radius:8px;padding:8px 14px;font-size:.92em;cursor:pointer;margin:0;width:auto}
</style>)rawcss"
