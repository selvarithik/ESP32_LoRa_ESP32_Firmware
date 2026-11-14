#pragma once
const char nodeChatPage[] PROGMEM=R"CHAT(<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>SELVARITHIK'S A&amp;D Gateway</title><link rel="icon" href="/favicon.ico"><link rel="apple-touch-icon" sizes="180x180" href="/favicon-180.png"><style>
:root{color-scheme:light dark;font:16px system-ui}body{margin:0 auto;max-width:42rem;padding:1rem}header,form{display:flex;gap:.6rem;align-items:center;flex-wrap:wrap}h1{margin:.4rem 0}input,button{font:inherit;padding:.65rem;border-radius:.5rem}input{min-width:8rem;flex:1}button{cursor:pointer}#messages{display:grid;gap:.6rem;margin:1rem 0;min-height:14rem;align-content:start}article{padding:.7rem 1rem;border:1px solid GrayText;border-radius:1rem;max-width:80%;overflow-wrap:anywhere}article.out{justify-self:end;background:#164e63;color:#fff}small{display:block;margin-top:.3rem}#status{font-size:.85rem}section[hidden]{display:none}
</style><header><h1>Node chat</h1><a href="/">Device console</a></header><p>Local network only</p>
<form id="login"><label for="pin">Chat PIN</label><input id="pin" type="password" maxlength="24" required><button>Sign in</button></form>
<section id="chat" hidden><p id="link"></p><div id="messages" aria-live="polite"></div><form id="send"><input id="text" maxlength="24" placeholder="Message" aria-label="Message" required><output id="count">0 / 24</output><button>Send</button></form></section><p id="status" role="status"></p>
<script>
const by=id=>document.getElementById(id);
let token=sessionStorage.getItem("nodeChat")||"", cursor=0, busy=false;
const bubbles=new Map();
async function api(path,body){
 const options={headers:{Authorization:"Bearer "+token}};
 if(body!==undefined){options.method="POST";options.headers["Content-Type"]="application/json";options.body=JSON.stringify(body);}
 const response=await fetch(path,options);
 const data=await response.json();
 if(!response.ok){
  if(response.status===401){token="";sessionStorage.removeItem("nodeChat");show();}
  throw new Error(data.error||"Request failed");
 }
 return data;
}
function show(){by("chat").hidden=!token;by("login").hidden=!!token;}
function update(entry){
 const key=String(entry.outgoing)+":"+entry.peer+":"+entry.id;
 let card=bubbles.get(key);
 if(!card){card=document.createElement("article");card.className=entry.outgoing?"out":"";by("messages").append(card);bubbles.set(key,card);}
 const text=document.createElement("span");text.textContent=entry.text;
 const state=document.createElement("small");state.textContent=({queued:"…",sent:"✓",delivered:"✓✓",failed:"!"})[entry.state]+" "+entry.state;
 card.replaceChildren(text,state);
 if(bubbles.size>24){const oldest=bubbles.keys().next().value;bubbles.get(oldest).remove();bubbles.delete(oldest);}
}
async function poll(){
 if(!token||document.hidden||busy)return;
 busy=true;
 try{
  const data=await api("/api/node-chat/messages?since="+cursor);
  data.messages.forEach(update);cursor=data.since;
  const r=data.signal,a=data.airtime;
  const rssi=r.rssi_valid?r.rssi_dbm+" dBm":"n/a";
  const level=r.rssi_valid?(r.rssi_dbm>=-70?4:r.rssi_dbm>=-90?3:r.rssi_dbm>=-110?2:1):0;
  by("link").textContent="RSSI "+rssi+" · Link "+level+"/4 · "+(a.remaining_us/1000000).toFixed(1)+" s remaining · TX gap "+Math.ceil(a.next_tx_gap_ms/1000)+" s";
 }catch(error){by("status").textContent=error.message;}
 finally{busy=false;}
}
by("login").onsubmit=async event=>{
 event.preventDefault();
 try{const data=await api("/api/node-chat/login",{pin:by("pin").value});token=data.token;sessionStorage.setItem("nodeChat",token);by("status").textContent="";show();poll();}
 catch(error){by("status").textContent=error.message;}finally{by("pin").value="";}
};
by("text").oninput=()=>{by("count").textContent=new TextEncoder().encode(by("text").value).length+" / 24";};
by("send").onsubmit=async event=>{
 event.preventDefault();const button=event.currentTarget.querySelector("button");button.disabled=true;
 try{await api("/api/node-chat/messages",{text:by("text").value});by("text").value="";by("text").oninput();by("status").textContent="";poll();}
 catch(error){by("status").textContent=error.message;}finally{button.disabled=false;}
};
document.addEventListener("visibilitychange",poll);show();poll();setInterval(poll,2000);
</script>)CHAT";
