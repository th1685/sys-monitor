const bar_ratio = 7.5;
BW = document.getElementById('cpu-bar').offsetWidth / bar_ratio;
const W = {cpu:65, mem:70, load:2}, C = {cpu:85, mem:85, load:4};
const G = '#00ff41', A = '#ffaa00', R = '#ff3535', E = '#2a2a2a';
const filepath = '/resources/monitor.json'; //wherever output saved

window.onresize = function() {
    BW = document.getElementById('cpu-bar').offsetWidth / bar_ratio;
}

async function load_data (filepath) {
    const response = await fetch(filepath);
    const monitordata = await response.json();

    return monitordata;
}

function co(v, w, c){ return v>=c ? R : (v>=w? A : G) }

function bar(pct, w, c){
    const n = Math.max(0, Math.min(BW, Math.round(pct/100*BW)));
    return `<span style="color:${co(pct,w,c)}; font-family: mono;">${'#'.repeat(n)}</span><span style="color:${E};font-family:mono;">${'0'.repeat(BW-n)}</span>`;
}

function fmtMem(b){ return ((b >= 1e6) ? (b/1e6).toFixed(2) + ' GB' : (b/1e3).toFixed(0) + ' MB'); }

function htm(id, h){ document.getElementById(id).innerHTML = h; }

function txt(id, t){ document.getElementById(id).textContent = t; }

let s = { cpu:0.0, mp:0.0, mu:0, mt:0, l1:0.0, l5:0.0, l15:0.0, uptime:"0" };

let tick = 0;
async function upd(){
    //read from JSON
    try {
        const monitordata = await load_data(filepath);
        s.cpu = monitordata.cpu_pct;
        s.l1  = monitordata.load_1m;
        s.l5  = monitordata.load_5m;
        s.l15 = monitordata.load_15m;
        s.mp  = monitordata.mem_used_pct;
        s.mu  = monitordata.mem_used_kb;
        s.mt  = monitordata.mem_total_kb;
        s.uptime = monitordata.uptime;
    } catch (err) {
        console.error(err);
    }

    //push to page by element ID
    const now = new Date();
    txt('ts',now.toLocaleTimeString());
    htm('cpu-pct',`<span style="color:${co(s.cpu,W.cpu,C.cpu)}">${s.cpu.toFixed(1)}%</span>`);
    htm('cpu-bar',bar(s.cpu, W.cpu, C.cpu));
    htm('l1',`<span style="color:${co(s.l1,W.load,C.load)}">${s.l1.toFixed(2)}</span>`);
    htm('l5',`<span style="color:${co(s.l5,W.load,C.load)}">${s.l5.toFixed(2)}</span>`);
    htm('l15',`<span style="color:${co(s.l15,W.load,C.load)}">${s.l15.toFixed(2)}</span>`);
    htm('mem-pct',`<span style="color:${co(s.mp,W.mem,C.mem)}">${s.mp.toFixed(1)}%</span>`);
    htm('mem-bar',bar(s.mp,W.mem,C.mem));
    txt('mem-detail',`${fmtMem(s.mu)} / ${fmtMem(s.mt)}`);
    tick++;
    txt('uinfo',`uptime ${s.uptime}`);
}

upd();
setInterval(upd,1000);