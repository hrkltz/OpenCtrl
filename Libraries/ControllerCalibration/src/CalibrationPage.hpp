#pragma once

static const char CALIBRATION_PAGE[] = R"OPENCTRL(
<!doctype html>
<html lang=en>
<head>
<meta charset=utf-8><meta name=viewport content="width=device-width, initial-scale=1">
<title>Controller calibration · OpenCtrl</title>
<style>
:root{color-scheme:light;font:16px/1.5 system-ui,sans-serif;color:#17372d;background:#f4f5ee}*{box-sizing:border-box}body{margin:0}main{max-width:1080px;margin:auto;padding:32px 24px 100px}header{display:flex;justify-content:space-between;align-items:center;border-bottom:1px solid #d9dfd4;padding-bottom:18px}.brand{font-weight:750;letter-spacing:-.04em;font-size:23px}.brand span{color:#6c8274}small,.muted{color:#62756b}.tag{font-size:12px;letter-spacing:.08em;text-transform:uppercase;color:#527260}h1{font-size:clamp(30px,5vw,46px);line-height:1.1;letter-spacing:-.04em;margin:18px 0}h2{font-size:18px;margin:0 0 8px}p{max-width:680px}.intro{padding:28px 0 10px}.status{background:#e6eddf;border:1px solid #c9d7c2;border-radius:12px;padding:14px 18px;margin:22px 0;display:flex;gap:10px;align-items:center}.dot{width:8px;height:8px;border-radius:50%;background:#44764e;flex:none}.status.error{background:#fff0e9;border-color:#e7b9a3;color:#852f20}.toolbar{display:flex;flex-wrap:wrap;gap:10px;margin:22px 0}button{font:inherit;font-weight:600;padding:10px 18px;background:#fff;border:1px solid #cbd4c7;border-radius:8px;color:inherit;cursor:pointer}button.primary{background:#254e38;border-color:#254e38;color:white}button:disabled{opacity:.45;cursor:default}button:hover:enabled{filter:brightness(.95)}.cards{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:16px}.card{background:#fff;border:1px solid #dce2d7;border-radius:14px;padding:22px}.card-head{display:flex;justify-content:space-between;align-items:start}.raw{font-variant-numeric:tabular-nums;font-size:24px;font-weight:650;letter-spacing:-.04em}.track{height:9px;background:#edf0e9;border-radius:8px;margin:18px 0 8px;overflow:hidden}.fill{height:100%;background:#71926b;width:50%;transition:width .15s}.readout{display:flex;justify-content:space-between;font-size:12px;color:#6b7f70}.fields{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:12px;margin-top:20px}label{font-size:13px;color:#52685a;display:block}input[type=number]{display:block;width:100%;font:inherit;font-size:15px;font-variant-numeric:tabular-nums;color:#17372d;padding:9px 8px;border:1px solid #cbd4c7;border-radius:6px;margin-top:5px;background:#fcfdf9}.options{margin-top:16px;display:flex;gap:20px;align-items:end}.options label{flex:1}.options .check{display:flex;gap:7px;align-items:center;padding-bottom:10px}.note{font-size:13px;margin-top:25px}.footer{display:flex;justify-content:space-between;border-top:1px solid #d9dfd4;margin-top:30px;padding-top:18px;font-size:13px}.saved{color:#38663f;font-weight:650}@media(max-width:650px){main{padding:22px 16px 60px}.cards{grid-template-columns:1fr}.card{padding:18px}.toolbar button{flex:1 1 40%}header{align-items:start;gap:20px}.footer{gap:20px}.options{gap:15px}}
</style>
</head>
<body><main>
<header><div class=brand>Open<span>Ctrl</span></div><small>Device settings / <span id=controller>Connecting…</span></small></header>
<section class=intro><div class=tag>Make it feel right</div><h1>Calibrate your controls.</h1><p class=muted>Check the live readings, capture each stick’s travel, then fine-tune its center and deadzone. Saved settings stay on this controller after power-off.</p></section>
<div class=status id=banner role=status aria-live=polite><span class=dot></span><span id=message>Connecting to your controller…</span></div>
<div class=toolbar><button id=start class=primary disabled>Start calibration</button><button id=capture disabled>Capture range</button><button id=center disabled>Set centers</button><button id=finish disabled>Done / resume controls</button></div>
<div class=cards id=axes></div>
<div class=toolbar><button id=preview disabled>Preview</button><button id=save class=primary disabled>Save to controller</button><button id=reset disabled>Reset defaults</button></div>
<p class=note>HID output is paused while calibration is active. Preview changes stay in memory until you save. Done discards an unsaved preview; a disconnected session expires after 30 seconds.</p>
<div class=footer><span>Stay connected to this controller’s WiFi.</span><span id=persistence>Reading settings…</span></div>
</main>
<script>
'use strict';
let state=null, busy=false, capturing=false, draftLoaded=false, dirty=false, connected=false;
const $=id=>document.getElementById(id);
const delay=ms=>new Promise(resolve=>setTimeout(resolve,ms));
function notify(text,error=false){$('message').textContent=text;$('banner').classList.toggle('error',error);}
function controls(){
 const active=connected&&state&&state.active;
 $('start').disabled=!connected||busy||active;
 for(const id of ['capture','center','finish','preview','save','reset'])$(id).disabled=!active||busy;
 $('save').disabled=$('save').disabled||!state.storage||capturing;
 $('center').disabled=$('center').disabled||capturing||!state.axes.every(a=>a.online);
 $('capture').disabled=$('capture').disabled||!state.axes.every(a=>a.online);
 $('preview').disabled=$('preview').disabled||capturing;
 for(const el of document.querySelectorAll('#axes input'))el.disabled=!active||busy;
}
function loadDraft(){
 if(!state)return;
 $('axes').replaceChildren();
 for(const [i,axis] of state.axes.entries()){
  const card=document.createElement('section');card.className='card';
  card.innerHTML='<div class=card-head><div><h2></h2><small class=online>Live input</small></div><div class=raw id=raw-'+i+'>—</div></div><div class=track><div class=fill id=fill-'+i+'></div></div><div class=readout><span>−100%</span><span id=value-'+i+'>0%</span><span>+100%</span></div><div class=fields></div><div class=options></div>';
  card.querySelector('h2').textContent=axis.label;
  for(const [key,label] of [['minimum','Minimum'],['center','Center'],['maximum','Maximum']]){
   const field=document.createElement('label');field.textContent=label;
   const input=document.createElement('input');input.type='number';input.step='1';input.id=key+'-'+i;input.value=axis[key];input.required=true;field.append(input);card.querySelector('.fields').append(field);
  }
  const options=card.querySelector('.options');
  const deadzone=document.createElement('label');deadzone.textContent='Deadzone (%)';
  const amount=document.createElement('input');amount.type='number';amount.required=true;amount.min=0;amount.max=99;amount.step='0.1';amount.id='deadzone-'+i;amount.value=Number((axis.deadzone*100).toFixed(2));deadzone.append(amount);options.append(deadzone);
  const inverted=document.createElement('label');inverted.className='check';
  const checkbox=document.createElement('input');checkbox.type='checkbox';checkbox.id='inverted-'+i;checkbox.checked=axis.inverted;inverted.append(checkbox,document.createTextNode('Invert axis'));options.append(inverted);
  $('axes').append(card);
 }
 for(const input of document.querySelectorAll('#axes input'))input.addEventListener('input',()=>{dirty=true;notify('Unsaved edits. Preview to test the response, or Save to keep them.');});
 draftLoaded=true;dirty=false;controls();render();
}
function render(){
 if(!state)return;
 $('controller').textContent=state.title;
 $('persistence').textContent=!state.storage?'Storage unavailable':dirty?'Unsaved edits':state.saved?'Saved on this controller':'Using temporary settings';
 $('persistence').className=state.saved&&!dirty?'saved':'';
 for(const [i,axis] of state.axes.entries()){
  if(!$('raw-'+i))continue;
  $('raw-'+i).textContent=axis.online?axis.raw:'—';
  $('value-'+i).textContent=axis.online?Math.round(axis.value*100)+'%':'Input disconnected';
  $('fill-'+i).style.width=(axis.online?(axis.value+1)*50:50)+'%';
 }
 controls();
}
async function refresh(){
 const response=await fetch('/api/calibration',{cache:'no-store'});
 if(!response.ok)throw new Error('Could not read controller settings');
 const previous=state, reconnecting=!connected;state=await response.json();connected=true;capturing=state.capturing;
 $('capture').textContent=capturing?'Finish capture':'Capture range';
 if(reconnecting)notify(state.message);
 if(!draftLoaded)loadDraft();
 if(previous&&previous.active&&!state.active){capturing=false;$('capture').textContent='Capture range';loadDraft();notify(state.message);}
 render();return state;
}
function payload(){
 for(const input of document.querySelectorAll('#axes input[type=number]'))if(!input.checkValidity())throw new Error('Complete every numeric field with a valid value.');
 const axes=state.axes.map((axis,i)=>({id:axis.id,minimum:Number($('minimum-'+i).value),center:Number($('center-'+i).value),maximum:Number($('maximum-'+i).value),deadzone:Number($('deadzone-'+i).value)/100,inverted:$('inverted-'+i).checked}));
 for(const axis of axes)if(!Number.isInteger(axis.minimum)||!Number.isInteger(axis.center)||!Number.isInteger(axis.maximum)||!(axis.minimum<axis.center&&axis.center<axis.maximum)||!Number.isFinite(axis.deadzone)||axis.deadzone<0||axis.deadzone>=1)throw new Error('Each axis needs minimum < center < maximum, and a deadzone below 100%.');
 return{version:1,axes};
}
async function command(action,body={}){
 if(busy)return;
 busy=true;controls();
 try{
  const response=await fetch('/api/calibration/'+action,{method:'POST',headers:{'Content-Type':'application/json','X-OpenCtrl-Calibration':'1'},body:JSON.stringify(body)});
  if(!response.ok)throw new Error(await response.text());
  const queued=await response.json();
  for(let attempt=0;attempt<60;attempt++){
   await delay(100);await refresh();
   if(state.completedOperation===queued.operation){if(state.operationStatus>=400)throw new Error(state.message);notify(state.message);if(['start','save','reset','end'].includes(action))loadDraft();return;}
  }
  throw new Error('No confirmation from controller. Reconnect and check the saved settings.');
 }catch(error){notify(error.message,true);}finally{busy=false;render();}
}
$('start').onclick=()=>command('start');
$('finish').onclick=()=>{capturing=false;$('capture').textContent='Capture range';return command('end');};
$('preview').onclick=()=>{try{return command('preview',payload());}catch(error){notify(error.message,true);}};
$('save').onclick=()=>{try{return command('save',payload());}catch(error){notify(error.message,true);}};
$('reset').onclick=()=>{if(confirm('Replace the saved calibration with default settings?'))command('reset');};
$('center').onclick=()=>{for(const [i,axis]of state.axes.entries())$('center-'+i).value=axis.raw;dirty=true;render();notify('Centers captured. Keep moving through the full range, then Preview or Save.');};
$('capture').onclick=async()=>{
 if(!capturing){await command('capture');}
 else{
  await command('stop-capture');
  if(!state.capturing){
   let updated=0;
   for(const [i,axis]of state.axes.entries())if(axis.observed&&axis.observedMaximum-axis.observedMinimum>=2){$('minimum-'+i).value=axis.observedMinimum;$('maximum-'+i).value=axis.observedMaximum;updated++;}
   dirty=true;notify(updated?'Range captured. Release both sticks, choose Set centers, then Preview or Save.':'No travel detected. Previous limits were kept.',!updated);
  }
 }
 render();
};
async function poll(){try{await refresh();}catch(error){connected=false;notify('Connection lost. Reconnect to the controller’s WiFi.',true);controls();}setTimeout(poll,250);}
poll().then(()=>{if(connected)notify(state.message);});
</script></body></html>
)OPENCTRL";
