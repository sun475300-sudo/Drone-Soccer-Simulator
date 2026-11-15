/* Pure local image-template tracking. Similarity is not calibrated probability. */
(function(root, factory){const api=factory();if(typeof module==='object'&&module.exports)module.exports=api;else root.ReviewCore=api;})(typeof globalThis!=='undefined'?globalThis:this, function(){
'use strict';
const clamp=(n,a,b)=>Math.min(b,Math.max(a,n));
function patch(frame,box,n=12){
 const out=new Float32Array(n*n*3);let k=0;
 for(let j=0;j<n;j++)for(let i=0;i<n;i++){
  const x=clamp(Math.round(box.x+(i+.5)*box.w/n),0,frame.width-1);
  const y=clamp(Math.round(box.y+(j+.5)*box.h/n),0,frame.height-1);
  const p=(y*frame.width+x)*4;out[k++]=frame.data[p];out[k++]=frame.data[p+1];out[k++]=frame.data[p+2];
 }return out;
}
function similarity(a,b){
 let ma=0,mb=0,ae=0;for(let i=0;i<a.length;i++){ma+=a[i];mb+=b[i];ae+=Math.abs(a[i]-b[i]);}ma/=a.length;mb/=b.length;
 let num=0,va=0,vb=0;for(let i=0;i<a.length;i++){let x=a[i]-ma,y=b[i]-mb;num+=x*y;va+=x*x;vb+=y*y;}
 const color=clamp(1-ae/(a.length*100),0,1);
 const corr=va>1&&vb>1?clamp(num/Math.sqrt(va*vb),0,1):color;
 return .72*corr+.28*color;
}
function match(frame,template,box,radius=32){
 const maxX=Math.max(0,frame.width-box.w),maxY=Math.max(0,frame.height-box.h);
 const lx=clamp(Math.round(box.x-radius),0,maxX),rx=clamp(Math.round(box.x+radius),0,maxX);
 const ly=clamp(Math.round(box.y-radius),0,maxY),ry=clamp(Math.round(box.y+radius),0,maxY);
 let best={x:clamp(Math.round(box.x),0,maxX),y:clamp(Math.round(box.y),0,maxY),w:box.w,h:box.h,score:-1};
 function check(x,y){const b={x,y,w:box.w,h:box.h};const s=similarity(template,patch(frame,b));if(s>best.score)best={...b,score:s};}
 check(best.x,best.y);
 const step=Math.max(2,Math.round(radius/10));
 for(let y=ly;y<=ry;y+=step)for(let x=lx;x<=rx;x+=step)check(x,y);
 const bx=best.x,by=best.y;
 for(let y=Math.max(ly,by-step);y<=Math.min(ry,by+step);y++)for(let x=Math.max(lx,bx-step);x<=Math.min(rx,bx+step);x++)check(x,y);
 return best;
}
function metric(points,width,height,maxGap=.30){
 let distance=0,seconds=0,peak=0,count=0;
 for(let i=1;i<points.length;i++){
  const a=points[i-1],b=points[i],dt=b.t-a.t;
  if(a.segment!==b.segment||dt<=0||dt>maxGap)continue;
  const d=Math.hypot((b.x-a.x)*width,(b.y-a.y)*height);
  distance+=d;seconds+=dt;peak=Math.max(peak,d/dt);count++;
 }return{distance,seconds,meanSpeed:seconds?distance/seconds:null,peakSpeed:seconds?peak:null,pairs:count};
}
function csvCell(v){let s=v===null||v===undefined?'':String(v);if(/^[=+@\t\r]/.test(s)||/^-[^\d.]/.test(s))s="'"+s;return '"'+s.replace(/"/g,'""')+'"';}
function csv(rows){return '\uFEFF'+rows.map(r=>r.map(csvCell).join(',')).join('\r\n');}
function validateProject(p){
 if(!p||p.schema!==1||p.app!=='DroneSoccerLab'||!Array.isArray(p.tracks)||p.tracks.length>10||!Array.isArray(p.events)||p.events.length>10000)throw Error('지원하지 않는 프로젝트 형식입니다.');
 const finite=n=>typeof n==='number'&&Number.isFinite(n);
 if(!p.video||typeof p.video.name!=='string'||!finite(p.video.duration)||p.video.duration<=0||!finite(p.video.width)||!finite(p.video.height)||p.video.width<=0||p.video.height<=0)throw Error('영상 메타데이터가 올바르지 않습니다.');
 const ids=new Set();let total=0;
 for(const tr of p.tracks){
  if(typeof tr.id!=='string'||ids.has(tr.id)||typeof tr.name!=='string'||tr.name.length>80||!['A','B','U'].includes(tr.team)||!['striker','defender','guide','unknown'].includes(tr.role)||!Array.isArray(tr.points))throw Error('기체 정보가 올바르지 않습니다.');ids.add(tr.id);
  total+=tr.points.length;if(total>500000)throw Error('프로젝트 포인트가 너무 많습니다.');
  let prev=-1;
  for(const pt of tr.points){if(![pt.t,pt.x,pt.y,pt.w,pt.h,pt.confidence].every(finite)||pt.t<prev||pt.t<0||pt.t>p.video.duration+.1||pt.x<0||pt.x>1||pt.y<0||pt.y>1||pt.w<=0||pt.w>1||pt.h<=0||pt.h>1||pt.confidence<0||pt.confidence>1||!['manual','template'].includes(pt.source)||typeof pt.segment!=='string')throw Error('궤적 데이터가 올바르지 않습니다.');prev=pt.t;}
 }
 for(const e of p.events)if(!finite(e.t)||e.t<0||e.t>p.video.duration+.1||typeof e.type!=='string'||e.type.length>80||typeof e.note!=='string'||e.note.length>2000||!['A','B','U'].includes(e.team))throw Error('이벤트 데이터가 올바르지 않습니다.');
 return p;
}
return {clamp,patch,similarity,match,metric,csv,csvCell,validateProject};
});
