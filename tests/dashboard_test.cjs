const fs = require('node:fs');
const assert = require('node:assert/strict');
const {JSDOM, VirtualConsole} = require('jsdom');
const html = fs.readFileSync(__dirname + '/../web/index.html', 'utf8');
const flush = () => new Promise(resolve => setImmediate(resolve));

function boot(source) {
  const state = {emptySlots:new Set(),frame:0, offline:false, calls:[], timeouts:[], scheduled:[], sockets:[], diagnostics:{hid_available:true,hid_ready:true,storage_ok:true,ws_available:true,mqtt_available:true,macro_armed:false,macro_running:false,macro_runs:0,lab_active:false,lab_sent:0}, link:{active:false,role:'initiator',session:1,planned:100,attempted:0,unique:0,duplicates:0,missing_observed:0,reordered:0,tx_errors:0,invalid:0,pdr_percent:null,per_percent:null,rtt_mean_us:null,storage_ok:true}};
  const errors=[];
  const virtualConsole=new VirtualConsole();
  virtualConsole.on('jsdomError',e=>errors.push(e.message));
  const dom = new JSDOM(source, {url:'http://oni.test/',runScripts:'dangerously',virtualConsole,beforeParse(w){
    w.HTMLCanvasElement.prototype.getContext=()=>new Proxy({}, {get:()=>()=>{}});
    w.setTimeout=(fn,ms)=>{state.scheduled.push({fn,ms});return state.scheduled.length;};
    w.AbortSignal.timeout=ms=>{
      const controller=new w.AbortController();
      state.timeouts.push({ms,expire:()=>controller.abort(new w.DOMException('Timed out','TimeoutError'))});
      return controller.signal;
    };
    w.WebSocket=class {constructor(){state.sockets.push(this)}};
    w.alert=message=>errors.push('alert: '+message);
    w.fetch=async(path,options={})=>{
      state.calls.push({path,options});
      if(state.offline && path==='/data') return new Promise((resolve,reject)=>{
        options.signal.addEventListener('abort',()=>reject(options.signal.reason),{once:true});
      });
      let body;
      if(path.startsWith('/macro/read?id=') && state.emptySlots.has(path.split('=')[1]))return {ok:false,status:404,text:async()=>''};
      if(path==='/data')body={values:Array(126).fill(0),radio_ok:true,register_ok:true,sweep_ms:1700,frame:++state.frame,uptime_ms:state.frame*1700,heap_bytes:200000,networks:[],advanced:{diagnostics:{...state.diagnostics}},link_test:{...state.link}};
      else if(path==='/events')body={events:[],overwritten:state.frame};
      else if(path==='/intelligence')body={frames:[],ble:[],wifi_probes:[],traffic:[],field:[]};
      else if(path==='/control-token')body='test-token';
      else if(path==='/diagnostics/arm'){state.diagnostics.macro_armed=true;body={...state.diagnostics};}
      else if(path.startsWith('/macro/read?id='))body='TEXT saved slot '+path.split('=')[1];
      else if(path==='/macro/save')body={...state.diagnostics};
      else if(path==='/lab/start'){state.diagnostics.lab_active=true;body={...state.diagnostics};}
      else if(path==='/sessions')body=[1];
      else if(path==='/report?id=1')body={reason:'COMPLETE'};
      else body={};
      return {ok:true,json:async()=>body,text:async()=>typeof body==='string'?body:JSON.stringify(body)};
    };
  }});
  const w=dom.window,doc=w.document;
  const tab=name=>[...doc.querySelectorAll('nav button')].find(b=>b.textContent===name).click();
  const button=name=>[...doc.querySelectorAll('#content button')].find(b=>b.textContent===name);
  const poll=()=>w.eval('poll()');
  const input=(element,value)=>{element.value=value;element.dispatchEvent(new w.Event('input'));element.dispatchEvent(new w.Event('change'));};
  return {state,dom,w,doc,errors,tab,button,poll,input};
}

(async()=>{
  const t=boot(html);await flush();t.tab('Host Macros');await flush();
  const select=t.doc.querySelector('select'),options=[...select.options],area=t.doc.querySelector('textarea');
  t.input(select,'4');await flush();assert.equal(area.value,'TEXT saved slot 4');t.input(area,'TEXT ONI_TEST\nENTER');area.focus();area.setSelectionRange(5,8);
  for(let i=0;i<20;i++){t.state.diagnostics.macro_runs=i;await t.poll();}
  assert.equal(t.doc.querySelector('select'),select);assert.deepEqual([...select.options],options);
  assert.equal(select.value,'4');assert.equal(t.doc.querySelector('textarea'),area);
  assert.equal(area.value,'TEXT ONI_TEST\nENTER');assert.equal(area.selectionStart,5);assert.equal(area.selectionEnd,8);
  assert.equal(t.doc.querySelector('#frame').textContent,String(t.state.frame));
  assert.equal(t.doc.querySelector('tbody tr').cells[3].textContent,'19');
  // No focus dependence: mimic a native popup that leaves activeElement elsewhere.
  area.blur();await t.poll();assert.equal(t.doc.querySelector('select'),select);
  t.state.offline=true;const pending=t.poll();const timeout=t.state.timeouts.at(-1);
  assert.equal(timeout.ms,5000);timeout.expire();await pending;
  assert.equal(t.doc.querySelector('select'),select);assert.equal(select.value,'4');
  assert.equal(t.doc.querySelector('#link').textContent,'OFFLINE');assert(t.button('Save macro').disabled);
  t.state.offline=false;await t.poll();assert.equal(t.doc.querySelector('select'),select);
  await t.button('Arm for 20 seconds').onclick();assert.equal(t.doc.querySelector('select'),select);
  assert.equal(t.button('Run selected macro').disabled,false);
  await t.button('Save macro').onclick();
  const sent=t.state.calls.findLast(x=>x.path==='/macro/save').options.body;
  assert.equal(sent.get('id'),'4');assert.equal(sent.get('script'),'TEXT ONI_TEST\nENTER');
  t.state.diagnostics.macro_armed=false;await t.poll();assert(t.button('Run selected macro').disabled);
  console.log('PASS: 20 live polls, blur/native-focus case, timeout, reconnect, caret, selected options, live counters, arm expiry, and correct save payload.');

  t.tab('Lab Beacons');const rate=t.doc.querySelector('select'),duration=t.doc.querySelector('input[type=number]'),ssids=t.doc.querySelector('input');
  t.input(rate,'3');t.input(duration,'10000');t.input(ssids,'ONI-LAB-A,ONI-LAB-B');
  await t.poll();await t.button('Start bounded lab beacon test').onclick();
  assert.equal(t.doc.querySelector('select'),rate);assert.equal(duration.value,'10000');assert.equal(rate.value,'3');
  const lab=t.state.calls.findLast(x=>x.path==='/lab/start').options.body;
  assert.equal(lab.get('rate_hz'),'3');assert.equal(lab.get('duration_ms'),'10000');
  t.state.diagnostics.lab_sent=27;await t.poll();assert.equal(t.doc.querySelector('tbody tr').cells[3].textContent,'27');
  t.tab('Frames');const frameButton=t.button('Start decoded capture');
  const socket=t.state.sockets.at(-1);socket.onmessage({data:JSON.stringify({type:'frame',uptime_ms:999,length:1,crc_ok:true,hex:'41',ascii:'A'})});
  assert.equal(t.button('Start decoded capture'),frameButton);assert.equal(t.doc.querySelector('tbody tr').cells[4].textContent,'A');
  t.tab('Host Macros');await flush();const stable=t.doc.querySelector('select');socket.onmessage({data:JSON.stringify({type:'frame',uptime_ms:1000,length:1,crc_ok:true,hex:'42',ascii:'B'})});
  assert.equal(t.doc.querySelector('select'),stable);assert.equal(stable.value,'4');assert.equal(t.doc.querySelector('textarea').value,'TEXT ONI_TEST\nENTER');
  console.log('PASS: beacon inputs and live results; WebSocket Frames update; returning to Macros preserves drafts.');

  for(const name of ['Spectrum','Waterfall','Wi-Fi','Events','Diagnostics','Link Test','Sessions','BLE Telemetry','Frames','Traffic','Field Track','Anomaly Matrix','Transport','Host Macros','Lab Beacons','Emergency Reset']){
    t.tab(name);await flush();const controls=[...t.doc.querySelectorAll('#content input,#content select,#content textarea,#content button')];
    await t.poll();assert.deepEqual([...t.doc.querySelectorAll('#content input,#content select,#content textarea,#content button')],controls,name);
  }
  t.tab('Sessions');await t.button('Load saved sessions').onclick();await t.button('Review session 1').onclick();
  const report=t.doc.querySelector('#reports pre');await t.poll();assert.equal(t.doc.querySelector('#reports pre'),report);
  t.tab('Host Macros');await flush();const slot=t.doc.querySelector('select'),editor=t.doc.querySelector('textarea');
  t.input(slot,'2');await flush();assert.equal(editor.value,'TEXT saved slot 2');
  t.input(editor,'TEXT draft two');t.input(slot,'4');await flush();assert.equal(editor.value,'TEXT ONI_TEST\nENTER');
  t.input(slot,'2');await flush();assert.equal(editor.value,'TEXT draft two');
  t.state.diagnostics.hid_ready=false;await t.poll();assert(t.button('Run selected macro').disabled);
  assert.deepEqual(t.errors,[]);t.dom.window.close();
  const blank=boot(html);await flush();blank.state.emptySlots.add('0');blank.tab('Host Macros');await flush();
  const slots=blank.doc.querySelector('select'),blankEditor=blank.doc.querySelector('textarea');
  blank.input(slots,'4');await flush();blank.input(blankEditor,'TEXT private slot four');
  blank.w.eval('delete macroDrafts["0"]');blank.input(slots,'0');await flush();
  assert.equal(blankEditor.value,'TEXT flawless diagnostic test\nDELAY 250');assert.deepEqual(blank.errors,[]);blank.dom.window.close();
  console.log('PASS: all 16 tabs keep control nodes on refresh; loaded session report survives; no script errors.');
})().catch(error=>{console.error(error);process.exitCode=1});
