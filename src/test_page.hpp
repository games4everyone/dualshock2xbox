#pragma once

namespace ds2x {

// the test-mode GUI as one self-contained page: no external assets, so it
// renders even in sandboxed previews. two SVG pads (playstation in, xbox out),
// a keyboard->ps->xbox mapping table, and a little JS that reports key events
// to /key and polls /last to light up the xbox side.
inline const char* kTestPage = R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>dualshock2xbox test mode</title>
<style>
body{background:#14161c;color:#e8e8e8;font-family:system-ui,sans-serif;margin:24px}
h1{font-size:20px} .sub{color:#9aa0aa;font-size:13px;margin-bottom:16px}
.pads{display:flex;gap:40px;flex-wrap:wrap}
.pad{background:#1d2027;border-radius:12px;padding:16px}
.pad h2{font-size:14px;margin:0 0 8px;color:#9aa0aa;text-transform:uppercase;letter-spacing:1px}
svg text{fill:#cfd3da;font-size:11px;font-family:monospace}
.btn{fill:#2a2e38;stroke:#3a3f4b;stroke-width:1.5}
.btn.on{fill:#3fae5a;stroke:#6fdc8b}
.axis{fill:#2a2e38;stroke:#3a3f4b}
.axis.on{fill:#3f7fae;stroke:#6fb6dc}
table{border-collapse:collapse;margin-top:20px;font-size:13px}
td,th{border:1px solid #333;padding:4px 10px;text-align:left}
th{color:#9aa0aa} td.k{font-family:monospace;color:#ffd479}
#log{margin-top:16px;font-family:monospace;font-size:12px;color:#9aa0aa;white-space:pre}
</style></head><body>
<h1>dualshock2xbox ; test mode</h1>
<div class="sub">press keys on your keyboard ; left pad is the fake DualShock you are playing, right pad is the Xbox the game sees</div>
<div class="pads">
  <div class="pad"><h2>DualShock (from keyboard)</h2><svg id="ps" width="300" height="190" viewBox="0 0 300 190"></svg></div>
  <div class="pad"><h2>Xbox 360 (what games see)</h2><svg id="xb" width="300" height="190" viewBox="0 0 300 190"></svg></div>
</div>
<table id="map"><tr><th>key</th><th>dualshock</th><th>xbox</th></tr></table>
<div id="log"></div>
<script>
const PS = {
 square:{x:210,y:45,r:11,n:"square"}, cross:{x:210,y:75,r:11,n:"cross"},
 circle:{x:240,y:60,r:11,n:"circle"}, triangle:{x:225,y:30,r:11,n:"triangle"},
 l1:{x:40,y:18,r:13,n:"L1"}, r1:{x:260,y:18,r:13,n:"R1"},
 l2:{x:40,y:2,rect:[28,2,24,10],n:"L2"}, r2:{x:260,y:2,rect:[248,2,24,10],n:"R2"},
 create:{x:120,y:60,r:8,n:"create"}, options:{x:180,y:60,r:8,n:"options"},
 ps:{x:150,y:80,r:9,n:"PS"}, touch:{x:150,y:35,r:14,n:"touch"},
 dpad:{x:60,y:95,arm:14},
 ls:{x:105,y:140,r:16,n:"LS"}, rs:{x:195,y:140,r:16,n:"RS"}
};
const XB = {
 x:{x:210,y:45,r:11,n:"X"}, a:{x:210,y:75,r:11,n:"A"}, b:{x:240,y:60,r:11,n:"B"}, y:{x:225,y:30,r:11,n:"Y"},
 lb:{x:40,y:18,r:13,n:"LB"}, rb:{x:260,y:18,r:13,n:"RB"},
 back:{x:120,y:60,r:8,n:"back"}, start:{x:180,y:60,r:8,n:"start"},
 guide:{x:150,y:80,r:9,n:"guide"},
 dpad:{x:60,y:95,arm:14}, ls:{x:105,y:140,r:16,n:"LS"}, rs:{x:195,y:140,r:16,n:"RS"}
};
const XB_BIT = {dpad_up:1,dpad_down:2,dpad_left:4,dpad_right:8,start:0x10,back:0x20,ls:0x40,rs:0x80,lb:0x100,rb:0x200,guide:0x400,a:0x1000,b:0x2000,x:0x4000,y:0x8000};
const PS_KEY = {}; // filled from /map
const state = {};
function drawPad(svgId, defs, lit) {
  const svg = document.getElementById(svgId); svg.innerHTML = "";
  for (const k in defs) {
    const d = defs[k]; let el;
    if (d.rect) { el = document.createElementNS("http://www.w3.org/2000/svg","rect");
      el.setAttribute("x",d.rect[0]); el.setAttribute("y",d.rect[1]); el.setAttribute("width",d.rect[2]); el.setAttribute("height",d.rect[3]); }
    else if (d.arm) { // dpad cross
      el = document.createElementNS("http://www.w3.org/2000/svg","path");
      const c=d, p=`M${c.x-4},${c.y-c.arm} h8 v${c.arm-4} h${c.arm-4} v8 h-${c.arm-4} v${c.arm-4} h-8 v-${c.arm-4} h-${c.arm-4} v-8 h${c.arm-4} z`;
      el.setAttribute("d",p);
    } else { el = document.createElementNS("http://www.w3.org/2000/svg","circle");
      el.setAttribute("cx",d.x); el.setAttribute("cy",d.y); el.setAttribute("r",d.r); }
    el.setAttribute("class", lit(k) ? (d.arm?"axis on":"btn on") : (d.arm?"axis":"btn"));
    el.dataset.k = k; svg.appendChild(el);
    const t = document.createElementNS("http://www.w3.org/2000/svg","text");
    t.setAttribute("x", d.arm ? d.x-8 : d.x); t.setAttribute("y", d.arm ? d.y+4 : d.y + (d.rect?18:3));
    t.setAttribute("text-anchor","middle"); t.textContent = d.n; svg.appendChild(t);
  }
}
// the left pad is drawn from the keys you hold. PS_KEY maps a pad element
// ("cross") to the keyboard keys that drive it, filled from /map ; PS_ALIAS
// bridges the drawing names to the mapping names.
const PS_ALIAS = {touch:"touchpad", ls:"l3", rs:"r3"};
const STICK_KEYS = {ls:["lstick_up","lstick_down","lstick_left","lstick_right"],
                    rs:["rstick_up","rstick_down","rstick_left","rstick_right"]};
function held(effect){ return (PS_KEY[effect]||[]).some(key=>state[key]); }
function psLit(k){
  if (k==="dpad") return ["dpad_up","dpad_down","dpad_left","dpad_right"].some(held);
  const e = PS_ALIAS[k]||k;
  if (STICK_KEYS[k] && STICK_KEYS[k].some(held)) return true;  // stick pushed
  return held(e);
}
function xbLit(k){
  if (k==="dpad") return !!(lastXb.buttons & 0x000f);
  if (k==="ls" && (Math.abs(lastXb.lx)>0.05||Math.abs(lastXb.ly)>0.05)) return true;
  if (k==="rs" && (Math.abs(lastXb.rx)>0.05||Math.abs(lastXb.ry)>0.05)) return true;
  if (k==="lt") return lastXb.lt>0.05;
  if (k==="rt") return lastXb.rt>0.05;
  return !!(lastXb.buttons & (XB_BIT[k]||0));
}
let lastXb = {buttons:0};
fetch("/map").then(r=>r.json()).then(m=>{
  const tbl = document.getElementById("map");
  for (const k in m){ const tr=document.createElement("tr");
    (PS_KEY[m[k].ps] = PS_KEY[m[k].ps] || []).push(k);
    // textContent, not innerHTML: key names are data, never markup
    for (const v of [k===" "?"space":k, m[k].ps, m[k].xb]) { const td=document.createElement("td"); td.textContent=v; tr.appendChild(td); }
    tr.firstChild.className="k"; tbl.appendChild(tr); }
  drawPad("ps",PS,psLit);
});
// shift turns "w" into "W", and the keyup would then release a different key
// than the keydown pressed (a stuck stick) ; fold letters to lower case
const norm = k => (k.length===1 ? k.toLowerCase() : k);
window.addEventListener("keydown", e=>{ if(e.repeat) return; if(e.key===" ") e.preventDefault(); send(norm(e.key),true); });
window.addEventListener("keyup", e=>send(norm(e.key),false));
// switching tabs while holding a key never delivers the keyup ; let go of everything
window.addEventListener("blur", ()=>{ for (const k in state) if (state[k]) send(k,false); });
function send(k,down){ state[k]=down; fetch(`/key?k=${encodeURIComponent(k)}&down=${down?1:0}`,{method:"POST"}).catch(()=>{});
  drawPad("ps",PS,psLit); }
setInterval(()=>{ fetch("/last").then(r=>r.json()).then(j=>{ lastXb=j; drawPad("xb",XB,xbLit);
  document.getElementById("log").textContent = "xbox buttons=0x"+j.buttons.toString(16).padStart(4,"0")+" lt="+j.lt.toFixed(2)+" rt="+j.rt.toFixed(2)+" l=("+j.lx.toFixed(2)+","+j.ly.toFixed(2)+") r=("+j.rx.toFixed(2)+","+j.ry.toFixed(2)+")";
}); }, 100);
drawPad("ps",PS,psLit); drawPad("xb",XB,xbLit);
</script></body></html>)HTML";

} // namespace ds2x
