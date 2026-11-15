'use strict';
const $=id=>document.getElementById(id), C=ReviewCore;
const video=$('video'), overlay=$('overlay'), ctx=overlay.getContext('2d');
const frameCanvas=document.createElement('canvas'), frameCtx=frameCanvas.getContext('2d',{willReadFrequently:true});
const state={clips:[],current:null,selected:null,mode:null,drag:null,running:false,token:0,inTime:0,outTime:null,loop:false,lastDraw:-1};
const colors={A:'#6aaaff',B:'#ff867a',U:'#f5d97a'};
const roles={striker:'골잡이',defender:'수비',guide:'가이드',unknown:'미확인'};
const uid=()=>crypto.randomUUID?crypto.randomUUID():Math.random().toString(36).slice(2)+Date.now();
function time(t){t=Math.max(0,Number(t)||0);const m=Math.floor(t/60),s=Math.floor(t%60),ms=Math.floor(t%1*1000);return `${String(m).padStart(2,'0')}:${String(s).padStart(2,'0')}.${String(ms).padStart(3,'0')}`;}
function status(msg){$('status').textContent=msg;}
function project(){return state.current?.project;}
function dirty(){if(state.current)state.current.dirty=true;}
function requireVideo(){if(!state.current||!Number.isFinite(video.duration)||video.readyState<2){status('먼저 재생 가능한 영상을 여세요.');return false;}return true;}
function selected(){return project()?.tracks.find(t=>t.id===state.selected);}
function saveBlob(name,blob){const url=URL.createObjectURL(blob),a=document.createElement('a');a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),2000);}
function saveText(name,text,type='text/plain;charset=utf-8'){saveBlob(name,new Blob([text],{type}));}
function stem(){return (state.current?.file.name||'review').replace(/\.[^.]+$/,'');}
function resize(){
 if(!video.videoWidth)return;
 overlay.width=video.videoWidth;overlay.height=video.videoHeight;
 const r=Math.min($('stage').clientWidth/video.videoWidth,$('stage').clientHeight/video.videoHeight);
 overlay.style.width=video.videoWidth*r+'px';overlay.style.height=video.videoHeight*r+'px';draw();
}
new ResizeObserver(resize).observe($('stage'));
function frame(){
 const scale=Math.min(1,960/Math.max(video.videoWidth,video.videoHeight));
 const w=Math.round(video.videoWidth*scale),h=Math.round(video.videoHeight*scale);
 if(frameCanvas.width!==w||frameCanvas.height!==h){frameCanvas.width=w;frameCanvas.height=h;}
 frameCtx.drawImage(video,0,0,w,h);return frameCtx.getImageData(0,0,w,h);
}
function latest(points,t){let lo=0,hi=points.length-1,found=null;while(lo<=hi){const m=(lo+hi)>>1;if(points[m].t<=t+.0001){found=points[m];lo=m+1;}else hi=m-1;}return found;}
function draw(target=ctx){
 const p=project(),w=overlay.width,h=overlay.height,t=video.currentTime||0;
 target.clearRect(0,0,w,h);if(!p)return;
 if($('heatmap').checked){
  const gw=48,gh=32,bins=new Float32Array(gw*gh);let peak=0;
  for(const tr of p.tracks)for(const pt of tr.points){if(pt.t>t)break;const n=Math.min(gh-1,Math.floor(pt.y*gh))*gw+Math.min(gw-1,Math.floor(pt.x*gw));peak=Math.max(peak,++bins[n]);}
  for(let i=0;i<bins.length;i++)if(bins[i]){target.fillStyle=`rgba(255,186,70,${.45*Math.sqrt(bins[i]/peak)})`;target.fillRect((i%gw)*w/gw,Math.floor(i/gw)*h/gh,w/gw+1,h/gh+1);}
 }
 const tail=Number($('trail').value),line=Math.max(2,w/600),font=Math.max(14,w/85);
 for(const tr of p.tracks){
  target.strokeStyle=colors[tr.team];target.lineWidth=line;target.beginPath();let prev=null;
  for(const pt of tr.points){if(pt.t>t+.0001)break;if(pt.t<t-tail)continue;
   if(!prev||pt.segment!==prev.segment||pt.t-prev.t>.30)target.moveTo(pt.x*w,pt.y*h);else target.lineTo(pt.x*w,pt.y*h);prev=pt;
  }target.stroke();const pt=latest(tr.points,t);
  if(pt&&t-pt.t<.16){
   target.strokeRect((pt.x-pt.w/2)*w,(pt.y-pt.h/2)*h,pt.w*w,pt.h*h);
   target.beginPath();target.arc(pt.x*w,pt.y*h,line*1.8,0,Math.PI*2);target.fillStyle=colors[tr.team];target.fill();
   const label=`${tr.name} · ${pt.source==='manual'?'수동':Math.round(pt.confidence*100)+' 유사도'}`;
   target.font=`600 ${font}px sans-serif`;const tw=target.measureText(label).width+12;
   const lx=C.clamp((pt.x-pt.w/2)*w,0,Math.max(0,w-tw)),ly=Math.max(font+8,(pt.y-pt.h/2)*h-6);
   target.fillStyle='rgba(3,8,15,.85)';target.fillRect(lx,ly-font-5,tw,font+8);target.fillStyle=colors[tr.team];target.fillText(label,lx+6,ly-2);
  }
 }
 if(state.drag){const b=dragBox();target.setLineDash([8,5]);target.strokeStyle='#ffffff';target.lineWidth=line;target.strokeRect(b.x*w,b.y*h,b.w*w,b.h*h);target.setLineDash([]);}
}
function stats(){
 const p=project();$('trackCount').textContent=p?.tracks.length||0;$('pointCount').textContent=p?.tracks.reduce((n,t)=>n+t.points.length,0)||0;$('eventCount').textContent=p?.events.length||0;
 const tr=selected(),m=tr?C.metric(tr.points,video.videoWidth,video.videoHeight):null;
 $('meanSpeed').textContent=m?.meanSpeed!==null&&m?.meanSpeed!==undefined?m.meanSpeed.toFixed(1)+' px/s':'— px/s';
}
function renderTracks(){
 const root=$('tracks');root.replaceChildren();
 for(const tr of project()?.tracks||[]){const item=document.createElement('div');item.className='track'+(tr.id===state.selected?' selected':'');item.tabIndex=0;item.setAttribute('role','button');
  const top=document.createElement('div'),name=document.createElement('strong'),badge=document.createElement('small');name.textContent=tr.name;name.style.color=colors[tr.team];badge.textContent=tr._lost?'재지정 필요':`${tr.points.length}점`;top.append(name,badge);
  const detail=document.createElement('small');detail.textContent=`${tr.team==='U'?'미확인':tr.team+'팀'} · ${roles[tr.role]} · ID ${tr.id.slice(0,6)}`;item.append(top,detail);
  const choose=()=>{state.selected=tr.id;renderTracks();stats();draw()};item.onclick=choose;item.onkeydown=e=>{if(e.key==='Enter')choose()};root.append(item);
 }stats();
}
function renderEvents(){
 const root=$('events');root.replaceChildren();const es=project()?.events||[];
 if(!es.length){const p=document.createElement('p');p.className='muted';p.textContent='아직 기록한 이벤트가 없습니다.';root.append(p);}
 es.forEach((e,i)=>{const row=document.createElement('div');row.className='event-row';const jump=document.createElement('button');jump.textContent=time(e.t);jump.onclick=()=>{if(!state.running){video.pause();seekTo(e.t).catch(x=>status(x.message))}};
  const desc=document.createElement('span');desc.textContent=`${e.team==='U'?'미확인':e.team+'팀'} · ${e.type}${e.note?' — '+e.note:''}`;
  const del=document.createElement('button');del.textContent='삭제';del.className='subtle';del.onclick=()=>{es.splice(i,1);dirty();renderEvents();stats()};row.append(jump,desc,del);root.append(row);
 });stats();
}
function playlist(){const root=$('playlist');root.replaceChildren();state.clips.forEach(clip=>{const b=document.createElement('button');b.textContent=clip.file.name;b.className=state.current===clip?'selected':'';b.onclick=()=>activate(clip);root.append(b)});}
function activate(clip){
 if(state.running){status('추적을 중지한 뒤 영상을 바꾸세요.');return;}if(state.current===clip)return;
 video.pause();if(state.current?.url)URL.revokeObjectURL(state.current.url);
 state.current=clip;state.selected=clip.project.tracks[0]?.id||null;state.inTime=0;state.outTime=null;state.loop=false;state.mode=null;state.drag=null;
 clip.url=URL.createObjectURL(clip.file);video.src=clip.url;video.muted=$('mute').checked;$('currentFile').textContent=clip.file.name;$('progress').value=0;$('loop').textContent='구간 반복 꺼짐';$('rangeText').textContent='전체 영상';
 $('empty').style.display='none';$('modeBadge').textContent='영상 여는 중';status('영상의 디코딩 가능 여부를 확인하고 있습니다.');playlist();renderTracks();renderEvents();
}
$('fileInput').onchange=e=>{
 if(state.running)return;let first=null;
 for(const file of e.target.files){if(!file.size)continue;let clip=state.clips.find(c=>c.file.name===file.name&&c.file.size===file.size&&c.file.lastModified===file.lastModified);if(!clip){clip={file,dirty:false,project:{app:'DroneSoccerLab',schema:1,video:{name:file.name,size:file.size,lastModified:file.lastModified},rules:$('rules').value,tracks:[],events:[]}};state.clips.push(clip)}first||=clip;}
 if(first)activate(first);e.target.value='';
};
video.onloadedmetadata=()=>{
 const p=project();if(!p)return;Object.assign(p.video,{duration:video.duration,width:video.videoWidth,height:video.videoHeight});$('seek').max=video.duration;
 $('videoInfo').textContent=`${video.videoWidth} × ${video.videoHeight} · ${time(video.duration)} · ${(state.current.file.size/1048576).toFixed(1)} MB`;
 $('modeBadge').textContent='수동 복기';resize();
};
video.onloadeddata=()=>{status('기체를 선택해 영역을 그리세요. 프로젝트는 JSON으로 저장해야 보존됩니다.');resize()};
video.onerror=()=>{status('재생하지 못했습니다. HEVC/H.265 원본이면 H.264 MP4로 변환 후 여세요. 손상·미지원 코덱도 확인하세요.');$('modeBadge').textContent='코덱 확인 필요'};
video.onplay=()=>{$('play').textContent='일시정지'};video.onpause=()=>{$('play').textContent='재생'};
function seekTo(t){
 t=C.clamp(t,0,Math.max(0,video.duration-.001));
 if(Math.abs(video.currentTime-t)<.00001&&video.readyState>=2){draw();return Promise.resolve()}
 return new Promise((resolve,reject)=>{const timeout=setTimeout(()=>finish(Error('해당 시점의 프레임을 읽지 못했습니다. 다른 시점 또는 H.264 파일로 확인하세요.')),8000);
  function finish(err){clearTimeout(timeout);video.removeEventListener('seeked',ok);video.removeEventListener('error',fail);err?reject(err):resolve();}
  const ok=()=>{draw();finish()},fail=()=>finish(Error('영상 디코딩 오류'));video.addEventListener('seeked',ok,{once:true});video.addEventListener('error',fail,{once:true});video.currentTime=t;
 });
}
async function step(dir){if(!requireVideo()||state.running)return;video.pause();await seekTo(video.currentTime+dir/C.clamp(Number($('fps').value)||30,1,240));}
$('backFrame').onclick=()=>step(-1).catch(e=>status(e.message));$('nextFrame').onclick=()=>step(1).catch(e=>status(e.message));
$('play').onclick=()=>{if(!requireVideo()||state.running)return;if(video.paused)video.play().catch(e=>status('재생 오류: '+e.message));else video.pause()};
$('speed').onchange=()=>{video.playbackRate=Number($('speed').value)};$('mute').onchange=()=>{video.muted=$('mute').checked};
$('seek').oninput=()=>{if(state.running)return;video.pause();video.currentTime=Number($('seek').value);};
function clockLoop(){
 if(video.readyState>=2){const t=video.currentTime;
  if(state.loop&&!state.running&&!video.paused&&state.outTime!==null&&t>=state.outTime)video.currentTime=state.inTime;
  $('clock').textContent=`${time(t)} / ${time(video.duration)}`;$('seek').value=t;
  if(Math.abs(t-state.lastDraw)>.0001){draw();state.lastDraw=t;}
 }requestAnimationFrame(clockLoop);
}requestAnimationFrame(clockLoop);
function pickMode(mode){if(!requireVideo()||state.running)return;if(mode==='correct'&&!selected()){status('수정할 기체를 먼저 선택하세요.');return;}if(mode==='add'&&project().tracks.length>=10){status('시험판은 영상당 기체 10대까지 기록합니다.');return;}video.pause();state.mode=mode;$('modeBadge').textContent=mode==='add'?'기체 영역 선택':'위치 재지정';status('기체를 둘러싸는 사각형을 드래그하세요. 그물이나 배경이 너무 많이 들어가지 않게 선택합니다.');}
$('addTrack').onclick=()=>pickMode('add');$('correctTrack').onclick=()=>pickMode('correct');
function coords(e){const r=overlay.getBoundingClientRect();return{x:C.clamp((e.clientX-r.left)/r.width,0,1),y:C.clamp((e.clientY-r.top)/r.height,0,1)}}
function dragBox(){const {a,b}=state.drag;return{x:Math.min(a.x,b.x),y:Math.min(a.y,b.y),w:Math.abs(a.x-b.x),h:Math.abs(a.y-b.y)}}
overlay.onpointerdown=e=>{if(!state.mode||state.running||!requireVideo())return;const p=coords(e);state.drag={a:p,b:p};overlay.setPointerCapture(e.pointerId);draw()};
overlay.onpointermove=e=>{if(state.drag){state.drag.b=coords(e);draw()}};
overlay.onpointercancel=()=>{state.drag=null;draw()};
overlay.onpointerup=e=>{
 if(!state.drag)return;state.drag.b=coords(e);const b=dragBox();state.drag=null;
 if(b.w*video.videoWidth<6||b.h*video.videoHeight<6){status('영역이 너무 작습니다. 기체 전체를 포함하도록 다시 그리세요.');draw();return;}
 let tr=selected();
 if(state.mode==='add'){tr={id:uid(),name:$('trackName').value.trim()||`${$('team').value==='U'?'미확인':$('team').value} 기체 ${project().tracks.length+1}`,team:$('team').value,role:$('role').value,points:[]};project().tracks.push(tr);state.selected=tr.id;}
 else if(!tr)return;
 if(tr.points.some(p=>p.t>video.currentTime+.001)&&!confirm('이 기체의 현재 시점 이후 궤적은 삭제하고 다시 기록합니다. 계속할까요?')){state.mode=null;draw();return;}
 tr.points=tr.points.filter(p=>p.t<video.currentTime-.001);tr.points.push({t:video.currentTime,x:b.x+b.w/2,y:b.y+b.h/2,w:b.w,h:b.h,confidence:1,source:'manual',segment:uid()});
 tr._lost=false;state.mode=null;$('modeBadge').textContent='수동 복기';$('trackName').value='';dirty();renderTracks();draw();status('수동 기준점을 기록했습니다. 짧은 종료점을 지정하고 추적을 시작하세요.');
};
$('removeTrack').onclick=()=>{const tr=selected();if(!tr||state.running)return;if(confirm(`${tr.name}의 궤적 전체를 삭제할까요?`)){project().tracks=project().tracks.filter(t=>t.id!==tr.id);state.selected=project().tracks[0]?.id||null;dirty();renderTracks();draw()}};
function busy(on){state.running=on;for(const id of ['fileInput','loadProject','play','backFrame','nextFrame','seek','addTrack','correctTrack','removeTrack','analyze','markIn','markOut','loop','sampleRate','radius','threshold'])$(id).disabled=on;$('modeBadge').textContent=on?'추적 처리 중':'수동 복기';}
$('stop').onclick=()=>{state.token++;status('중지 요청됨. 현재 프레임 처리가 끝나면 멈춥니다.');};
$('analyze').onclick=async()=>{
 if(!requireVideo()||state.running)return;video.pause();state.mode=null;const p=project(),start=video.currentTime,end=state.outTime??video.duration;
 if(end<=start+.005){status('종료점이 현재 위치보다 뒤에 있어야 합니다.');return;}
 const fps=Number($('sampleRate').value),threshold=Number($('threshold').value),radius=C.clamp(Number($('radius').value)||36,8,100),f=frame();
 let active=[];
 for(const tr of p.tracks){const pt=latest(tr.points,start);if(!pt||Math.abs(pt.t-start)>.10)continue;const b={x:(pt.x-pt.w/2)*f.width,y:(pt.y-pt.h/2)*f.height,w:pt.w*f.width,h:pt.h*f.height};active.push({tr,box:b,template:C.patch(f,b),segment:pt.segment})}
 if(!active.length){status('현재 장면에 기준점이 없습니다. 기체를 추가하거나 위치를 수정한 뒤 시작하세요.');return;}
 if(active.some(a=>a.tr.points.some(pt=>pt.t>start+.001))&&!confirm('현재 시점 이후 해당 기체의 기존 궤적을 지우고 다시 추적할까요?'))return;
 for(const a of active){a.tr.points=a.tr.points.filter(pt=>pt.t<=start+.001);a.tr._lost=false;}
 const token=++state.token;busy(true);dirty();let processed=0,lost=0;
 try{
  for(let step=1;start+step/fps<=end+.00001;step++){
   if(token!==state.token)break;const t=Math.min(start+step/fps,video.duration-.001);await seekTo(t);if(token!==state.token)break;
   const im=frame();const candidates=active.map(a=>({...a,found:C.match(im,a.template,a.box,radius),collision:false}));
   for(let i=0;i<candidates.length;i++)for(let j=i+1;j<candidates.length;j++){const a=candidates[i].found,b=candidates[j].found;if(Math.hypot(a.x+a.w/2-b.x-b.w/2,a.y+a.h/2-b.y-b.h/2)<Math.min(a.w,a.h,b.w,b.h)*.45){candidates[i].collision=candidates[j].collision=true}}
   active=[];
   for(const a of candidates){const b=a.found;if(b.score<threshold||a.collision){a.tr._lost=true;lost++;continue;}
    a.tr.points.push({t:video.currentTime,x:(b.x+b.w/2)/im.width,y:(b.y+b.h/2)/im.height,w:b.w/im.width,h:b.h/im.height,confidence:b.score,source:'template',segment:a.segment});a.box=b;active.push(a);
   }
   processed++;$('progress').value=(t-start)/(end-start);status(`${time(t)} / ${time(end)} · 추적 중 ${active.length}대 · 재지정 필요 ${lost}대`);
   if(processed%5===0)renderTracks();draw();
   if(!active.length){status('모든 기체가 유사도 기준 또는 겹침 검사에서 중단되었습니다. 추적을 확인하고 위치를 다시 지정하세요.');break;}
   await new Promise(resolve=>setTimeout(resolve,0));
  }
  if(active.length)status(`${token!==state.token?'중지':'구간 처리 완료'} · ${processed}개 표본 처리 · 미확인 기체 ${lost}대. 궤적을 검토하고 JSON으로 저장하세요.`);
 }catch(e){status('추적 중단: '+e.message)}finally{busy(false);renderTracks();draw()}
};
function range(){ $('rangeText').textContent=`${time(state.inTime)} → ${state.outTime===null?'영상 끝':time(state.outTime)}`;}
$('markIn').onclick=()=>{if(!requireVideo())return;state.inTime=video.currentTime;if(state.outTime!==null&&state.outTime<=state.inTime)state.outTime=null;range();status('시작점은 반복 재생의 기준입니다. 추적은 현재 재생 위치에서 시작합니다.')};
$('markOut').onclick=()=>{if(!requireVideo())return;if(video.currentTime<=state.inTime){status('종료점은 시작점보다 뒤에 지정하세요.');return;}state.outTime=video.currentTime;range()};
$('loop').onclick=()=>{if(!requireVideo())return;if(state.outTime===null){status('먼저 시작점과 종료점을 지정하세요.');return;}state.loop=!state.loop;$('loop').textContent='구간 반복 '+(state.loop?'켜짐':'꺼짐');if(state.loop)seekTo(state.inTime).catch(e=>status(e.message))};
$('threshold').oninput=()=>{$('thresholdValue').textContent=Number($('threshold').value).toFixed(2)};
$('trail').onchange=()=>draw();$('heatmap').onchange=()=>draw();$('rules').onchange=()=>{if(project()){project().rules=$('rules').value;dirty()}};
$('addEvent').onclick=()=>{if(!requireVideo())return;project().events.push({id:uid(),t:video.currentTime,type:$('eventType').value,team:$('eventTeam').value,note:$('eventNote').value,source:'manual'});project().events.sort((a,b)=>a.t-b.t);$('eventNote').value='';dirty();renderEvents();status('사용자 확인 이벤트를 기록했습니다. 자동 판정 결과가 아닙니다.')};
function serializable(){const p=project();if(!p)return null;return{app:p.app,schema:1,version:'0.1.0-preview',savedAt:new Date().toISOString(),video:p.video,rules:p.rules,coordinateSystem:'normalized-image-2D',cameraCompensated:false,tracks:p.tracks.map(({id,name,team,role,points})=>({id,name,team,role,points})),events:p.events,settings:{fps:Number($('fps').value),sampleRate:Number($('sampleRate').value),threshold:Number($('threshold').value)}};}
$('saveProject').onclick=()=>{if(!requireVideo())return;saveText(stem()+'.review.json',JSON.stringify(serializable(),null,2),'application/json');state.current.dirty=false;status('JSON 다운로드를 요청했습니다. 브라우저 다운로드 완료를 확인하세요. 원본 영상은 포함되지 않습니다.')};
$('loadProject').onchange=async e=>{
 const file=e.target.files[0];e.target.value='';if(!file||!requireVideo()||state.running)return;
 try{if(file.size>30*1024*1024)throw Error('프로젝트 파일은 30 MB 이하여야 합니다.');const p=C.validateProject(JSON.parse(await file.text()));
  if(p.video.name!==state.current.file.name||p.video.size!==state.current.file.size||p.video.width!==video.videoWidth||p.video.height!==video.videoHeight||Math.abs(p.video.duration-video.duration)>.15)throw Error('프로젝트에 기록된 원본 영상과 현재 파일이 다릅니다. 같은 영상 파일을 여세요.');
  if((project().tracks.length||project().events.length)&&!confirm('현재 메모리의 복기 내용을 불러온 프로젝트로 바꿀까요? 저장하지 않은 내용은 사라집니다.'))return;
  state.current.project=p;state.current.dirty=false;state.selected=p.tracks[0]?.id||null;
  if(p.settings){$('fps').value=C.clamp(Number(p.settings.fps)||30,1,240);if(['10','15','30'].includes(String(p.settings.sampleRate)))$('sampleRate').value=p.settings.sampleRate;$('threshold').value=C.clamp(Number(p.settings.threshold)||.72,.4,.98);$('threshold').oninput();}
  if([...$('rules').options].some(o=>o.value===p.rules))$('rules').value=p.rules;renderTracks();renderEvents();draw();status('프로젝트를 복원했습니다. 이어서 추적할 장면에서 기체 위치를 확인하세요.');
 }catch(err){status('프로젝트 불러오기 실패: '+err.message)}
};
$('exportTracks').onclick=()=>{if(!requireVideo())return;const rows=[['video','track_id','name','team','role','time_seconds','x_normalized','y_normalized','x_pixels','y_pixels','width_normalized','height_normalized','similarity','source','segment','coordinates']];for(const tr of project().tracks)for(const p of tr.points)rows.push([project().video.name,tr.id,tr.name,tr.team,tr.role,p.t,p.x,p.y,p.x*video.videoWidth,p.y*video.videoHeight,p.w,p.h,p.confidence,p.source,p.segment,'image_2D_not_world']);saveText(stem()+'.tracks.csv',C.csv(rows),'text/csv;charset=utf-8')};
$('exportEvents').onclick=()=>{if(!requireVideo())return;const rows=[['video','time_seconds','team','type','note','source']];for(const e of project().events)rows.push([project().video.name,e.t,e.team,e.type,e.note,'manual']);saveText(stem()+'.events.csv',C.csv(rows),'text/csv;charset=utf-8')};
$('snapshot').onclick=()=>{if(!requireVideo())return;const c=document.createElement('canvas');c.width=video.videoWidth;c.height=video.videoHeight;const x=c.getContext('2d');x.drawImage(video,0,0);x.drawImage(overlay,0,0);c.toBlob(b=>{if(b)saveBlob(stem()+'_'+video.currentTime.toFixed(3)+'.png',b)})};
const esc=x=>String(x??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
$('report').onclick=()=>{if(!requireVideo())return;const p=project();let table='';for(const tr of p.tracks){const m=C.metric(tr.points,video.videoWidth,video.videoHeight);table+=`<tr><td>${esc(tr.name)}</td><td>${esc(tr.team)}</td><td>${esc(roles[tr.role])}</td><td>${tr.points.length}</td><td>${m.meanSpeed===null?'미산출':m.meanSpeed.toFixed(1)}</td><td>${m.distance.toFixed(1)}</td></tr>`;}
 const events=p.events.map(e=>`<tr><td>${time(e.t)}</td><td>${esc(e.team)}</td><td>${esc(e.type)}</td><td>${esc(e.note)}</td></tr>`).join('');
 const text=`<!doctype html><html lang="ko"><meta charset="utf-8"><title>드론축구 복기 보고서</title><style>body{font:15px/1.7 sans-serif;max-width:1050px;margin:45px auto;padding:20px;color:#202b38}h1{border-bottom:3px solid #245e6d;padding-bottom:15px}table{width:100%;border-collapse:collapse}th,td{border:1px solid #c4cbd4;padding:9px;text-align:left;overflow-wrap:anywhere}th{background:#edf2f5}.warning{padding:15px;background:#fff3d9}@media print{body{margin:0}}</style><h1>드론축구 영상 복기 보고서</h1><p>파일: ${esc(p.video.name)}<br>길이: ${time(p.video.duration)} / 크기: ${p.video.width} × ${p.video.height}<br>규정 메모: ${esc(p.rules)}<br>작성: ${esc(new Date().toLocaleString('ko-KR'))}</p><p class="warning">시험판 · 자동 판정 아님. 위치·거리·속도는 카메라 영상 좌표이며 실제 3D 좌표나 기체 속도가 아닙니다. 카메라 이동 보정 없음. 속도는 같은 궤적 구간에서 0.30초 이하 간격의 유효 점 쌍으로 계산했습니다. 이벤트는 사용자 수동 기록입니다.</p><h2>기체 기록</h2><table><tr><th>기체</th><th>팀</th><th>역할</th><th>포인트</th><th>평균 화면속도 px/s</th><th>화면상 이동량 px</th></tr>${table}</table><h2>이벤트 타임라인</h2><table><tr><th>시각</th><th>팀</th><th>종류</th><th>관찰 메모</th></tr>${events}</table><p>Drone Soccer Lab 0.1.0-preview · 원본 영상과 JSON 프로젝트를 함께 보관하세요.</p></html>`;
 saveText(stem()+'.report.html',text,'text/html;charset=utf-8');
};
$('helpButton').onclick=()=>{$('help').showModal()};$('closeHelp').onclick=()=>{$('help').close()};
$('quitButton').onclick=async()=>{if(state.clips.some(c=>c.dirty)&&!confirm('저장하지 않은 복기 내용이 있습니다. JSON을 저장하지 않고 종료할까요?'))return;state.token++;try{const r=await fetch('quit',{method:'POST',headers:{'X-Lab-Request':'shutdown'}});if(!r.ok)throw Error('종료 응답 오류');state.clips.forEach(c=>c.dirty=false);video.pause();status('프로그램을 종료했습니다. 이 브라우저 탭을 닫으세요.');$('modeBadge').textContent='종료됨'}catch(e){status('종료 요청 실패. 실행 창을 닫거나 작업 관리자에서 DroneSoccerLab을 종료하세요.')}};
window.addEventListener('beforeunload',e=>{if(state.clips.some(c=>c.dirty)){e.preventDefault();e.returnValue=''}});
document.addEventListener('keydown',e=>{if((e.ctrlKey||e.metaKey)&&e.key.toLowerCase()==='s'){e.preventDefault();$('saveProject').click();return;}if(['INPUT','SELECT','TEXTAREA'].includes(document.activeElement.tagName)||$('help').open)return;if(e.code==='Space'){e.preventDefault();$('play').click()}if(e.key==='ArrowLeft'){e.preventDefault();step(-1).catch(x=>status(x.message))}if(e.key==='ArrowRight'){e.preventDefault();step(1).catch(x=>status(x.message))}if(e.key==='Escape'){state.mode=null;state.drag=null;draw()}});
