#ifndef PICKLESCORE_OTA_WEB_H
#define PICKLESCORE_OTA_WEB_H

// Spec 04 Section 4: no framework, CDN, external request, cookie or persistent token.
static const char OTA_HTML[] = R"html(<!doctype html>
<html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>PickleScore maintenance</title><style>
body{font:16px system-ui,sans-serif;max-width:38rem;margin:2rem auto;padding:0 1rem;color:#17202a;background:#f8fafc}
h1{font-size:1.6rem}label{display:block;margin:1rem 0 .35rem}input,button{font:inherit;padding:.65rem;box-sizing:border-box;max-width:100%}
input[type=password],progress{width:100%}button{margin:.7rem .4rem .5rem 0}pre{white-space:pre-wrap}#message{min-height:3em}small{display:block}
</style><h1>PickleScore maintenance</h1><pre id="info">Loading local device status…</pre>
<form id="login"><label for="admin">Maintenance key</label><input id="admin" type="password" autocomplete="off" minlength="64" maxlength="64" required>
<button>Sign in</button></form><section id="maintenance" hidden>
<label for="firmware">Signed firmware package (.pksota)</label><input id="firmware" type="file" accept=".pksota">
<div><button id="upload" type="button">Upload and verify</button><button id="install" type="button" disabled>Install verified update</button>
<button id="close" type="button">Exit maintenance</button></div><progress id="progress" max="100" value="0"></progress>
<small>Upload does not install. Installation reboots the scoreboard and resets the in-memory score to 0–0–2.</small></section>
<p id="message" role="status" aria-live="polite">Use only your own local maintenance network. A connected scoring controller prevents maintenance.</p>
<script src="/app.js" defer></script></html>)html";

static const char OTA_JS[] = R"js('use strict';
const $=id=>document.getElementById(id);let token='',nonce='',busy=false;
function say(text){$('message').textContent=text;}
async function request(path,method='GET',body=null,headers={}){
 const h={...headers};if(token)h.Authorization='Bearer '+token;if(method==='POST'&&token)h['X-CSRF-Token']=nonce;
 const response=await fetch(path,{method,headers:h,body,cache:'no-store',credentials:'omit'});
 let data;try{data=await response.json();}catch{throw new Error('Device returned an invalid response');}
 if(!response.ok)throw new Error(data.error||('HTTP '+response.status));return data;
}
async function status(){const data=await request('/api/status');nonce=data.nonce;
 $('install').disabled=busy||data.phase!=='staged';return data;}
function controls(value){busy=value;$('upload').disabled=value;$('close').disabled=value;$('firmware').disabled=value;if(value)$('install').disabled=true;}
request('/api/info').then(x=>{$('info').textContent=`${x.device}\nBuild: ${x.build}\nSlot: ${x.slot} · Maximum image: ${x.limit} bytes\nFree heap: ${x.heap} bytes`;}).catch(e=>say(e.message));
$('login').addEventListener('submit',async e=>{e.preventDefault();const secret=$('admin').value;$('admin').value='';
 try{const data=await request('/api/session','POST',null,{'X-Admin-Key':secret});token=data.token;nonce=data.nonce;
 $('login').hidden=true;$('maintenance').hidden=false;say('Maintenance active for at most 10 minutes. BLE advertising is paused; last score is preserved.');}
 catch(error){say(error.message+' If the AP closes, stop the scoring controller and power-cycle for a new maintenance window.');}});
$('upload').onclick=async()=>{
 const file=$('firmware').files[0];if(!file){say('Select a signed .pksota package first.');return;}
 controls(true);$('progress').value=0;say('Uploading signed package…');
 try{await status();await new Promise((resolve,reject)=>{const xhr=new XMLHttpRequest();xhr.open('POST','/api/upload');xhr.timeout=245000;
 xhr.setRequestHeader('Authorization','Bearer '+token);xhr.setRequestHeader('X-CSRF-Token',nonce);xhr.setRequestHeader('Content-Type','application/octet-stream');
 xhr.upload.onprogress=e=>{if(e.lengthComputable)$('progress').value=e.loaded/e.total*100;};
 xhr.upload.onload=()=>say('Transfer sent; validating signature, image and flash readback…');
 xhr.onerror=()=>reject(new Error('Connection interrupted. No incomplete image is installed.'));
 xhr.ontimeout=()=>reject(new Error('Transfer timed out. Running firmware remains selected.'));
 xhr.onload=()=>{let data;try{data=JSON.parse(xhr.responseText);}catch{return reject(new Error('Invalid device response'));}
 if(xhr.status!==200)return reject(new Error(data.error||('HTTP '+xhr.status)));resolve(data);};xhr.send(file);});
 say('Firmware verified and staged. Review the build below, then explicitly install.');}
 catch(error){say(error.message);}finally{controls(false);try{const s=await status();if(s.phase==='staged')say(`Verified ${s.stagedBuild}. Install only when the game may be reset.`);}catch{say('Maintenance unavailable or expired. Running firmware is preserved; reconnect after a safe power-cycle.');}}
};
$('install').onclick=async()=>{if(!window.confirm('Install the verified firmware now? The scoreboard will reboot and the current in-memory score will reset to 0–0–2.'))return;
 controls(true);try{await status();await request('/api/install','POST');say('Installation accepted. Device is rebooting. Reconnect to its AP and refresh to verify the new build.');}
 catch(error){say(error.message);controls(false);try{await status();}catch{}}};
$('close').onclick=async()=>{try{await status();await request('/api/close','POST');say('Maintenance closed. Last score preserved; normal BLE control resumes.');controls(true);}
 catch(error){say(error.message);}};
)js";
#endif
