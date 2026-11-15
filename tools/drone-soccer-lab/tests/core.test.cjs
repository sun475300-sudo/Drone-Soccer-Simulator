'use strict';
const test=require('node:test'), assert=require('node:assert/strict');
const C=require('../web/core.js');
function scene(dx=0,dy=0,visible=true){const w=100,h=80,data=new Uint8ClampedArray(w*h*4);for(let y=0;y<h;y++)for(let x=0;x<w;x++){let i=(y*w+x)*4;data[i]=20;data[i+1]=30;data[i+2]=35;data[i+3]=255;if(visible&&x>=25+dx&&x<45+dx&&y>=25+dy&&y<45+dy){let rx=x-25-dx,ry=y-25-dy;data[i]=80+rx*7;data[i+1]=180-ry*6;data[i+2]=80+((rx+ry)%6)*20}}return{width:w,height:h,data};}
test('template detects a translated colored region',()=>{const box={x:25,y:25,w:20,h:20};const template=C.patch(scene(),box);const hit=C.match(scene(6,-4),template,box,20);assert.ok(Math.abs(hit.x-31)<=1);assert.ok(Math.abs(hit.y-21)<=1);assert.ok(hit.score>.95);});
test('missing target has low similarity',()=>{const box={x:25,y:25,w:20,h:20};const hit=C.match(scene(0,0,false),C.patch(scene(),box),box,20);assert.ok(hit.score<.72);});
test('matching stays inside frame bounds',()=>{const b={x:0,y:0,w:12,h:12};const hit=C.match(scene(),C.patch(scene(),b),b,100);assert.ok(hit.x>=0&&hit.y>=0&&hit.x+hit.w<=100&&hit.y+hit.h<=80);});
test('speed uses valid pairs and never bridges gaps or new segments',()=>{const pts=[{t:0,x:0,y:0,segment:'a'},{t:.1,x:.1,y:0,segment:'a'},{t:2,x:.9,y:.9,segment:'a'},{t:2.1,x:1,y:1,segment:'b'}];const m=C.metric(pts,100,100);assert.equal(m.pairs,1);assert.equal(m.distance,10);assert.equal(m.meanSpeed,100);});
test('empty and singleton tracks do not imply zero velocity',()=>{assert.equal(C.metric([],100,100).meanSpeed,null);assert.equal(C.metric([{t:1,x:.5,y:.5,segment:'a'}],100,100).meanSpeed,null);});
test('CSV escapes quotes and spreadsheet formulas',()=>{assert.equal(C.csvCell('a"b'),'"a""b"');assert.equal(C.csvCell('=HYPERLINK(1)'), '"\'=HYPERLINK(1)"');assert.equal(C.csvCell(-2),'"-2"');assert.ok(C.csv([['기체',1]]).startsWith('\uFEFF'));});
const valid=()=>({app:'DroneSoccerLab',schema:1,video:{name:'a.mp4',duration:10,width:100,height:100},tracks:[{id:'a',name:'기체',team:'A',role:'striker',points:[{t:1,x:.5,y:.5,w:.1,h:.1,confidence:1,source:'manual',segment:'s'}]}],events:[]});
test('valid project round-trips',()=>{const p=valid();assert.deepEqual(C.validateProject(JSON.parse(JSON.stringify(p))),p);});
test('invalid NaN coordinate rejected',()=>{const p=valid();p.tracks[0].points[0].x=NaN;assert.throws(()=>C.validateProject(p));});
test('duplicate IDs rejected',()=>{const p=valid();p.tracks.push(p.tracks[0]);assert.throws(()=>C.validateProject(p));});
test('backward time rejected',()=>{const p=valid();p.tracks[0].points.push({...p.tracks[0].points[0],t:0});assert.throws(()=>C.validateProject(p));});
test('unknown project rejected',()=>{assert.throws(()=>C.validateProject({}));});
test('oversized team list rejected',()=>{const p=valid();p.tracks=Array(11).fill(p.tracks[0]);assert.throws(()=>C.validateProject(p));});
