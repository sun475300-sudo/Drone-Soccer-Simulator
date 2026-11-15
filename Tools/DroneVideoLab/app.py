"""Drone Video Lab: local-only, human-reviewed drone soccer video analysis."""
from __future__ import annotations
import argparse
import copy
import json
import os
from pathlib import Path
import queue
import sys
import threading
import time
import traceback
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import cv2
import numpy as np
from PIL import Image, ImageTk
import core


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title(f"드론축구 영상분석실 · {core.VERSION}")
        self.geometry(f"{min(1450,self.winfo_screenwidth()-40)}x{min(900,self.winfo_screenheight()-80)}")
        self.minsize(1050,700)
        self.configure(bg="#101720")
        self.cap = None
        self.project = None
        self.source = None
        self.project_path = None
        self.raw = None
        self.index = 0
        self.time_s = 0.
        self.time_source = "fps_estimate"
        self.playing = False
        self.play_epoch = 0
        self.dirty = False
        self.engines = {}
        self.segments = {}
        self.segment = 0
        self.active_id = None
        self.undo_state = None
        self.playlist = []
        self.slider_guard = False
        self.drag_start = None
        self.drag_id = None
        self.scale = 1.
        self.offset = (0.,0.)
        self.worker = None
        self.messages = queue.Queue()
        self.cancel_export = threading.Event()
        self.loop_a = None
        self.loop_b = None
        self.team = tk.StringVar(value="Red")
        self.label = tk.StringVar(value="기체 1")
        self.auto = tk.BooleanVar(value=True)
        self.rate = tk.StringVar(value="1.0")
        self.threshold = tk.DoubleVar(value=.68)
        self.loop = tk.BooleanVar(value=False)
        self.event_type = tk.StringVar(value="공격시도")
        self.note = tk.StringVar()
        self.status = tk.StringVar(value="영상 열기 → 일시정지 → 기체 주변 드래그 → 한 프레임씩 검토")
        self.details = tk.StringVar(value="로컬 전용 · 영상 업로드 없음 · 소리 없이 재생")
        self.stats = tk.StringVar(value="기록 없음")
        self._build()
        self.protocol("WM_DELETE_WINDOW", self.close)
        self.report_callback_exception = self._tk_error
        self.bind("<space>", lambda e:self.key(e,self.toggle))
        self.bind("<Left>", lambda e:self.key(e,lambda:self.step(-1)))
        self.bind("<Right>", lambda e:self.key(e,lambda:self.step(1)))
        self.bind("<Control-s>", lambda e:self.guard(self.save))
        self.bind("<Control-z>", lambda e:self.guard(self.undo))
        self.bind("<F1>", lambda e:self.help())
        self.after(150,self.poll)

    def _build(self):
        style = ttk.Style(self)
        style.theme_use("clam")
        style.configure(".", background="#172230", foreground="#e9edf4", fieldbackground="#223144", font=("Malgun Gothic",10))
        style.configure("TButton", padding=(9,6))
        style.map("TButton", background=[("active","#324962")])
        style.configure("TCombobox", fieldbackground="#f0f3f8", foreground="#172230")
        style.configure("TEntry", fieldbackground="#f0f3f8", foreground="#172230")
        header=ttk.Frame(self,padding=(14,10)); header.pack(fill="x")
        ttk.Label(header,text="DRONE VIDEO LAB",font=("Segoe UI",20,"bold")).pack(side="left")
        ttk.Label(header,text="  영상 복기 · 실험용 추적 보조",font=("Malgun Gothic",11)).pack(side="left",padx=12)
        self.button(header,"도움말 F1",self.help).pack(side="right")
        toolbar=ttk.Frame(self,padding=(10,4)); toolbar.pack(fill="x")
        for text,fn in [("영상 열기",self.open_dialog),("프로젝트 열기",self.load),
                        ("프로젝트 저장",self.save),("CSV 저장",self.csv),
                        ("궤적 MP4",self.export),("현재 PNG",self.snapshot),
                        ("위치 분포 PNG",self.density)]:
            self.button(toolbar,text,fn).pack(side="left",padx=3)
        main=ttk.Frame(self,padding=10); main.pack(fill="both",expand=True)
        left=ttk.Frame(main); left.pack(side="left",fill="both",expand=True)
        # Scrollable controls preserve access on small screens and high-DPI Windows desktops.
        side=ttk.Frame(main,width=330); side.pack(side="right",fill="y"); side.pack_propagate(False)
        sidebar=tk.Canvas(side,width=310,highlightthickness=0,bg="#172230")
        scroll=ttk.Scrollbar(side,orient="vertical",command=sidebar.yview)
        scroll.pack(side="right",fill="y"); sidebar.pack(side="left",fill="both",expand=True)
        sidebar.configure(yscrollcommand=scroll.set)
        right=ttk.Frame(sidebar,padding=(12,0,5,5))
        window=sidebar.create_window((0,0),window=right,anchor="nw")
        right.bind("<Configure>",lambda e:sidebar.configure(scrollregion=sidebar.bbox("all")))
        sidebar.bind("<Configure>",lambda e:sidebar.itemconfigure(window,width=e.width))
        self.canvas=tk.Canvas(left,bg="#080d14",highlightthickness=0)
        self.canvas.pack(fill="both",expand=True)
        self.canvas.bind("<Configure>",lambda e:self.render())
        self.canvas.bind("<ButtonPress-1>",self.drag_begin)
        self.canvas.bind("<B1-Motion>",self.drag_move)
        self.canvas.bind("<ButtonRelease-1>",lambda e:self.guard(lambda:self.drag_end(e)))
        self.timeline=ttk.Scale(left,from_=0,to=1,command=self.slider_changed)
        self.timeline.pack(fill="x",pady=8)
        controls=ttk.Frame(left); controls.pack(fill="x")
        for text,fn in [("◀ 1",lambda:self.step(-1)),("재생 / 정지",self.toggle),("1 ▶",lambda:self.step(1)),
                        ("A 지정",lambda:self.set_loop("a")),("B 지정",lambda:self.set_loop("b"))]:
            self.button(controls,text,fn).pack(side="left",padx=2)
        ttk.Checkbutton(controls,text="A–B 반복",variable=self.loop).pack(side="left",padx=5)
        ttk.Combobox(controls,textvariable=self.rate,values=("0.25","0.5","1.0","2.0"),state="readonly",width=5).pack(side="right")
        ttk.Label(controls,text="배속 ").pack(side="right")
        ttk.Label(left,textvariable=self.details,wraplength=850).pack(anchor="w",pady=(8,3))
        ttk.Label(left,text="화면 2D 좌표입니다. 실제 거리·고도·m/s·자동 득점 판정이 아닙니다.",foreground="#f8c56c").pack(anchor="w")
        ttk.Label(right,text="01  영상 목록",font=("Malgun Gothic",11,"bold")).pack(anchor="w")
        self.files=tk.Listbox(right,height=2,bg="#111c2a",fg="#e9edf4",exportselection=False,selectbackground="#375575")
        self.files.pack(fill="x",pady=(5,10)); self.files.bind("<Double-Button-1>",lambda e:self.guard(self.switch_video))
        ttk.Label(right,text="02  기체 지정 / 수정",font=("Malgun Gothic",11,"bold")).pack(anchor="w")
        row=ttk.Frame(right); row.pack(fill="x",pady=5)
        ttk.Entry(row,textvariable=self.label,width=18).pack(side="left",fill="x",expand=True)
        ttk.Combobox(row,textvariable=self.team,values=core.TEAMS,state="readonly",width=8).pack(side="right",padx=(5,0))
        row=ttk.Frame(right); row.pack(fill="x")
        self.button(row,"새 기체",self.new_track).pack(side="left")
        self.button(row,"선택 삭제",self.delete_track).pack(side="left",padx=4)
        self.button(row,"되돌리기",self.undo).pack(side="left")
        ttk.Label(right,text="일시정지 후 기체를 사각형으로 드래그하세요.",wraplength=285).pack(anchor="w",pady=5)
        ttk.Checkbutton(right,text="템플릿 추적 후보 생성 (실험용)",variable=self.auto,command=self.auto_changed).pack(anchor="w")
        row=ttk.Frame(right);row.pack(fill="x",pady=3)
        ttk.Label(row,text="유사도 기준").pack(side="left")
        ttk.Spinbox(row,from_=.4,to=.95,increment=.01,textvariable=self.threshold,width=6).pack(side="left",padx=7)
        self.tracks=tk.Listbox(right,height=4,bg="#111c2a",fg="#e9edf4",exportselection=False,selectbackground="#375575")
        self.tracks.pack(fill="x",pady=5); self.tracks.bind("<<ListboxSelect>>",self.select_track)
        ttk.Label(right,textvariable=self.stats,wraplength=280).pack(anchor="w")
        ttk.Separator(right).pack(fill="x",pady=10)
        ttk.Label(right,text="03  사건 기록 (수동)",font=("Malgun Gothic",11,"bold")).pack(anchor="w")
        ttk.Combobox(right,textvariable=self.event_type,state="readonly",values=("공격시도","득점 후보","득점 확인(수동)","수비","충돌 후보","추적 유실","메모")).pack(fill="x",pady=5)
        ttk.Entry(right,textvariable=self.note).pack(fill="x")
        row=ttk.Frame(right);row.pack(fill="x",pady=5)
        self.button(row,"현재 시점 기록",self.add_event).pack(side="left")
        self.button(row,"사건 삭제",self.delete_event).pack(side="left",padx=4)
        self.events=tk.Listbox(right,height=3,bg="#111c2a",fg="#e9edf4",exportselection=False,selectbackground="#375575")
        self.events.pack(fill="both",expand=True)
        self.events.bind("<Double-Button-1>",lambda e:self.guard(self.goto_event))
        ttk.Label(right,text="사건을 두 번 누르면 해당 프레임으로 이동합니다.",wraplength=280).pack(anchor="w",pady=5)
        self.button(right,"MP4 내보내기 취소",self.cancel_export.set).pack(fill="x",pady=5)
        ttk.Label(self,textvariable=self.status,padding=(12,9),wraplength=1300).pack(fill="x")

    def button(self,parent,text,fn):
        return ttk.Button(parent,text=text,width=0,command=lambda:self.guard(fn))

    def guard(self,fn):
        try:return fn()
        except Exception as exc:
            self.playing=False
            self.status.set(str(exc)); messagebox.showerror("작업을 완료하지 못했습니다",str(exc),parent=self)
            return False

    def _tk_error(self,kind,value,tb):
        self.playing=False
        messagebox.showerror("오류",f"{value}\n\n현재 프로젝트를 별도 파일로 저장한 뒤 다시 열어 주세요.",parent=self)

    def key(self,event,fn):
        if isinstance(self.focus_get(),(tk.Entry,ttk.Entry,ttk.Combobox,ttk.Spinbox)):
            return
        self.guard(fn);return "break"

    def ensure(self):
        if self.project is None or self.raw is None:raise ValueError("먼저 영상을 여세요.")

    def check_dirty(self):
        if not self.dirty:return True
        answer=messagebox.askyesnocancel("기록 저장","변경한 복기 기록을 저장할까요?",parent=self)
        if answer is None:return False
        return bool(self.save()) if answer else True

    def open_dialog(self):
        names=filedialog.askopenfilenames(parent=self,title="복기할 영상 선택",filetypes=[("Videos","*.mp4 *.avi *.mov *.mkv *.m4v"),("All files","*.*")])
        if not names:return
        if not self.check_dirty():return
        self.open_video(names[0])
        self.playlist=list(names); self.files.delete(0,"end")
        for name in names:self.files.insert("end",Path(name).name)
        self.files.selection_set(0)

    def switch_video(self):
        chosen=self.files.curselection()
        if chosen and self.check_dirty():self.open_video(self.playlist[chosen[0]])

    def open_video(self,path,project=None):
        self.playing=False
        info=core.video_info(path)
        if project and project["video"]["sha256"] != info["sha256"]:
            raise ValueError("프로젝트와 영상의 SHA-256이 다릅니다. 원본 영상을 선택하세요.")
        if project and any(project["video"][k]!=info[k] for k in ("width","height","frame_count")):
            raise ValueError("저장한 영상 크기/프레임 수가 현재 디코더 결과와 다릅니다.")
        cap=cv2.VideoCapture(str(path))
        try:frame,stamp,source=core.read_frame(cap,0,info["fps"])
        except Exception:cap.release();raise
        if self.cap:self.cap.release()
        self.cap=cap;self.source=Path(path).resolve();self.project=project or core.new_project(info)
        self.project_path=None;self.raw=frame;self.index=0;self.time_s=stamp;self.time_source=source
        self.engines.clear();self.segments.clear();self.active_id=None;self.undo_state=None;self.dirty=False
        self.segment=max((p["segment"] for t in self.project["tracks"].values() for p in t["samples"]),default=0)
        self.loop_a=self.loop_b=None;self.loop.set(False)
        self.timeline.configure(to=max(1,info["frame_count"]-1));self.refresh_lists();self.render();self.update_time()
        self.status.set("영상을 열었습니다. 기체를 드래그해 지정하세요. 영상은 서버로 전송되지 않습니다.")

    def update_time(self):
        if not self.project:return
        v=self.project["video"]
        self.slider_guard=True;self.timeline.set(self.index);self.slider_guard=False
        self.details.set(f"{v['name']}  |  {self.index+1:,}/{v['frame_count']:,} frames  |  {self.time_s:.3f}s  |  {v['width']}×{v['height']}  |  평균 {v['fps']:.3f}fps  |  {self.time_source}")

    def slider_changed(self,value):
        if self.slider_guard or not self.project:return
        index=int(float(value))
        if index!=self.index:self.guard(lambda:self.seek(index))

    def seek(self,index):
        self.ensure();self.playing=False;self.engines.clear()
        self._read(max(0,min(self.project["video"]["frame_count"]-1,int(index))),True)
        self.status.set("이동 후 자동 추적은 해제됩니다. 계속 추적할 기체를 다시 드래그하세요.")

    def _read(self,index,seek):
        self.raw,self.time_s,self.time_source=core.read_frame(self.cap,index,self.project["video"]["fps"],seek)
        self.index=index
        self.render();self.update_time()

    def toggle(self):
        self.ensure();self.playing=not self.playing
        if self.playing:
            self.play_epoch+=1;self.tick(self.play_epoch)

    def tick(self,epoch):
        if not self.playing or epoch!=self.play_epoch:return
        start=time.monotonic()
        try:self.advance()
        except Exception as exc:self.playing=False;self.status.set(str(exc));return
        if self.playing:
            delay=max(1,round(1000/(self.project["video"]["fps"]*float(self.rate.get()))-(time.monotonic()-start)*1000))
            self.after(delay,lambda:self.tick(epoch))

    def step(self,direction):
        self.ensure();self.playing=False
        if direction==1:self.advance()
        else:self.seek(self.index-1)

    def advance(self):
        v=self.project["video"]
        if self.loop.get() and self.loop_a is not None and self.loop_b is not None and self.index>=self.loop_b:
            self.engines.clear();self._read(self.loop_a,True);return
        if self.index+1>=v["frame_count"]:self.playing=False;return
        target=self.index+1
        self.raw,self.time_s,self.time_source=core.read_frame(self.cap,target,v["fps"],False)
        self.index=target
        proposals={}
        if self.auto.get():
            for tid,engine in self.engines.items():
                result=engine.update(self.raw)
                if result:proposals[tid]=result
            ids=list(proposals)
            ambiguous=set()
            for i,tid in enumerate(ids):
                for other in ids[i+1:]:
                    if core.overlap(proposals[tid][0],proposals[other][0])>.55:ambiguous.update((tid,other))
            for tid in ambiguous:
                self.engines[tid].active=False;self.engines[tid].reason="다른 트랙과 겹침";del proposals[tid]
            for tid,(box,score) in proposals.items():
                core.upsert(self.project["tracks"][tid],core.sample(target,self.time_s,self.time_source,box,score,"candidate",self.segments[tid]))
                self.dirty=True
            lost=[tid for tid,engine in self.engines.items() if not engine.active]
            if lost:
                for tid in lost:self.engines.pop(tid)
                self.playing=False
                self.status.set(f"추적 중단: {', '.join(lost)}. 가림/오인 가능성입니다. 기체를 다시 지정하세요.")
        self.render();self.update_time();self.refresh_stats()

    def auto_changed(self):
        self.engines.clear()
        self.status.set("추적 설정이 바뀌었습니다. 적용할 기체를 다시 지정하세요.")

    def render(self):
        if self.raw is None:return
        width,height=max(1,self.canvas.winfo_width()),max(1,self.canvas.winfo_height())
        h,w=self.raw.shape[:2];self.scale=min(width/w,height/h)
        dw,dh=max(1,round(w*self.scale)),max(1,round(h*self.scale))
        self.offset=((width-dw)/2,(height-dh)/2)
        annotated=core.annotate(self.raw,self.project,self.index)
        image=cv2.resize(annotated,(dw,dh),interpolation=cv2.INTER_AREA)
        self.photo=ImageTk.PhotoImage(Image.fromarray(cv2.cvtColor(image,cv2.COLOR_BGR2RGB)))
        self.canvas.delete("frame")
        self.canvas.create_image(self.offset[0],self.offset[1],image=self.photo,anchor="nw",tags="frame")
        self.canvas.tag_lower("frame")

    def image_point(self,x,y):
        if self.raw is None:return None
        ix=(x-self.offset[0])/self.scale;iy=(y-self.offset[1])/self.scale
        h,w=self.raw.shape[:2]
        return (int(ix),int(iy)) if 0<=ix<w and 0<=iy<h else None

    def drag_begin(self,event):
        self.playing=False
        if self.image_point(event.x,event.y) is None:return
        self.drag_start=(event.x,event.y)
        self.drag_id=self.canvas.create_rectangle(event.x,event.y,event.x,event.y,outline="#ffffff",width=2,dash=(4,3))

    def drag_move(self,event):
        if self.drag_id:self.canvas.coords(self.drag_id,*self.drag_start,event.x,event.y)

    def drag_end(self,event):
        if not self.drag_start:return
        start=self.image_point(*self.drag_start);end=self.image_point(event.x,event.y)
        self.canvas.delete(self.drag_id);self.drag_start=None;self.drag_id=None
        if not start or not end:return
        x,y=min(start[0],end[0]),min(start[1],end[1]);w,h=abs(end[0]-start[0]),abs(end[1]-start[1])
        self.seed((x,y,w,h))

    def seed(self,box):
        self.ensure()
        x,y,w,h=map(int,box)
        height,width=self.raw.shape[:2]
        if min(w,h)<8 or x<0 or y<0 or x+w>width or y+h>height:raise ValueError("영상 안에서 8×8픽셀 이상 지정하세요.")
        name=self.label.get().strip()[:120]
        if not name:raise ValueError("기체 이름을 입력하세요.")
        threshold=float(self.threshold.get())
        if not .4<=threshold<=.95:raise ValueError("유사도 기준은 0.40~0.95 사이로 입력하세요.")
        engine=None;warning=""
        if self.auto.get():
            try:engine=core.TemplateTracker(self.raw,box,threshold)
            except ValueError as exc:warning=str(exc)
        self.undo_state=copy.deepcopy(self.project)
        tid=self.active_id
        if tid is None:
            tid="T"+str(max((int(t[1:]) for t in self.project["tracks"]),default=0)+1)
            if len(self.project["tracks"])>=100:raise ValueError("기체 트랙은 최대 100개입니다.")
            self.project["tracks"][tid]=dict(label=name,team=self.team.get(),samples=[])
        self.active_id=tid
        track=self.project["tracks"][tid];track["label"]=name;track["team"]=self.team.get()
        self.segment+=1;self.segments[tid]=self.segment
        core.upsert(track,core.sample(self.index,self.time_s,self.time_source,box,1.,"manual",self.segment))
        self.engines.pop(tid,None)
        if engine:self.engines[tid]=engine
        self.dirty=True;self.refresh_lists();self.render()
        self.status.set(f"{tid} 수동 위치 저장. "+(warning if warning else "오른쪽 화살표로 다음 프레임을 검토하세요. 같은 프레임의 수정 추적은 기존 위치를 대체합니다."))

    def new_track(self):
        self.active_id=None;self.tracks.selection_clear(0,"end");self.label.set("새 기체")
        self.status.set("새 기체를 영상에서 드래그해 지정하세요.")

    def select_track(self,event=None):
        indices=self.tracks.curselection()
        if not indices or not self.project:return
        tid=list(self.project["tracks"])[indices[0]];self.active_id=tid
        track=self.project["tracks"][tid];self.label.set(track["label"]);self.team.set(track["team"])

    def delete_track(self):
        self.ensure()
        if self.active_id and messagebox.askyesno("트랙 삭제","선택 트랙의 모든 위치 기록을 삭제할까요? 되돌리기가 가능합니다.",parent=self):
            self.undo_state=copy.deepcopy(self.project)
            self.project["tracks"].pop(self.active_id);self.engines.pop(self.active_id,None)
            self.active_id=None;self.dirty=True;self.refresh_lists();self.render()

    def undo(self):
        if self.undo_state is None:return
        self.project,self.undo_state=self.undo_state,None
        self.engines.clear();self.active_id=None;self.playing=False;self.dirty=True
        self.refresh_lists();self.render();self.status.set("마지막 수동 편집 이전으로 복원했습니다. 이후 자동 추적 기록도 함께 복원됩니다.")

    def refresh_lists(self):
        self.tracks.delete(0,"end");self.events.delete(0,"end")
        if not self.project:return
        for tid,t in self.project["tracks"].items():self.tracks.insert("end",f"[{tid}] {t['team']} · {t['label']}")
        if self.active_id in self.project["tracks"]:self.tracks.selection_set(list(self.project["tracks"]).index(self.active_id))
        for e in self.project["events"]:self.events.insert("end",f"{e['time_s']:.2f}s · {e['team']} · {e['type']} {e['note']}")
        self.refresh_stats()

    def refresh_stats(self):
        if not self.project:return
        samples=[p for t in self.project["tracks"].values() for p in t["samples"]]
        manual=sum(p["status"]=="manual" for p in samples)
        self.stats.set(f"수동 위치 {manual:,} · 미확정 추적 후보 {len(samples)-manual:,}\n활성 추적 {len(self.engines)} · 유사도는 정확도 확률이 아닙니다.")

    def add_event(self):
        self.ensure();self.playing=False;self.undo_state=copy.deepcopy(self.project)
        self.project["events"].append(dict(frame=self.index,time_s=self.time_s,type=self.event_type.get(),team=self.team.get(),note=self.note.get()[:1000]))
        self.dirty=True;self.refresh_lists();self.note.set("")

    def delete_event(self):
        self.ensure();chosen=self.events.curselection()
        if chosen:
            self.undo_state=copy.deepcopy(self.project);self.project["events"].pop(chosen[0]);self.dirty=True;self.refresh_lists()

    def goto_event(self):
        self.ensure();chosen=self.events.curselection()
        if chosen:self.seek(self.project["events"][chosen[0]]["frame"])

    def set_loop(self,which):
        self.ensure()
        if which=="a":self.loop_a=self.index
        else:self.loop_b=self.index
        if self.loop_a is not None and self.loop_b is not None and self.loop_b<=self.loop_a:
            self.loop_b=None;raise ValueError("B는 A보다 뒤에 지정하세요.")
        self.status.set(f"반복 구간: A={self.loop_a}, B={self.loop_b}. 반복 시 추적은 초기화됩니다.")

    def save(self):
        self.ensure();self.playing=False
        path=filedialog.asksaveasfilename(parent=self,title="프로젝트 저장 (영상은 포함되지 않음)",defaultextension=".json",initialfile=(self.project_path.name if self.project_path else self.source.stem+".review.json"),filetypes=[("Review project","*.json")])
        if not path:return False
        try:self.project["video_ref"]=os.path.relpath(self.source,Path(path).parent)
        except ValueError:self.project["video_ref"]=str(self.source)
        core.save_project(path,self.project);self.project_path=Path(path);self.dirty=False
        self.status.set(f"프로젝트 저장: {Path(path).name}. 원본 영상은 별도 보관하세요.");return True

    def load(self):
        if not self.check_dirty():return
        path=filedialog.askopenfilename(parent=self,filetypes=[("Review project","*.json")])
        if not path:return
        project=core.load_project(path)
        reference=project.get("video_ref",project["video"]["name"])
        unsafe=not isinstance(reference,str) or "://" in reference or reference.startswith(("\\\\","//"))
        candidate=None if unsafe else Path(path).parent/reference
        if candidate is None or not candidate.is_file():
            selected=filedialog.askopenfilename(parent=self,title="프로젝트의 원본 영상 선택",filetypes=[("Videos","*.mp4 *.avi *.mov *.mkv *.m4v")])
            if not selected:return
            candidate=Path(selected)
        self.open_video(candidate,project);self.project_path=Path(path)
        self.playlist=[str(candidate)];self.files.delete(0,"end");self.files.insert("end",candidate.name)

    def csv(self):
        self.ensure();folder=filedialog.askdirectory(parent=self,title="tracks.csv / events.csv 저장 폴더")
        if not folder:return
        if any((Path(folder)/name).exists() for name in ("tracks.csv","events.csv")):
            if not messagebox.askyesno("덮어쓰기","기존 tracks.csv 또는 events.csv를 덮어쓸까요?",parent=self):return
        core.export_csv(folder,self.project);self.status.set("CSV 저장 완료. 속도 단위는 화면 픽셀/초이며 실제 기체 속도가 아닙니다.")

    def snapshot(self):
        self.ensure();path=filedialog.asksaveasfilename(parent=self,defaultextension=".png",initialfile=f"frame_{self.index}.png",filetypes=[("PNG","*.png")])
        if path:Image.fromarray(cv2.cvtColor(core.annotate(self.raw,self.project,self.index),cv2.COLOR_BGR2RGB)).save(path)

    def density(self):
        self.ensure();image=core.heatmap(self.project,self.raw)
        path=filedialog.asksaveasfilename(parent=self,defaultextension=".png",initialfile="observation_counts.png",filetypes=[("PNG","*.png")])
        if path:Image.fromarray(cv2.cvtColor(image,cv2.COLOR_BGR2RGB)).save(path)
        self.status.set("위치 분포는 관측 프레임 수입니다. 체류 시간·실제 경기장 좌표가 아니며 카메라 이동을 보정하지 않았습니다.")

    def export(self):
        self.ensure();self.playing=False
        if self.worker and self.worker.is_alive():raise ValueError("현재 내보내기가 진행 중입니다.")
        if not messagebox.askokcancel("MP4 출력 안내","음성이 없는 고정 프레임률 MP4로 출력합니다.\n가변 프레임률 원본과 시간 길이가 조금 달라질 수 있습니다.\n추적 후보는 확정값이 아니며 원본을 보존합니다.",parent=self):return
        path=filedialog.asksaveasfilename(parent=self,defaultextension=".mp4",initialfile=self.source.stem+"_review.mp4",filetypes=[("MP4","*.mp4")])
        if not path:return
        data=copy.deepcopy(self.project);source=str(self.source)
        self.cancel_export.clear()
        def work():
            try:
                core.export_video(source,path,data,self.cancel_export,lambda a,b:self.messages.put(("progress",f"MP4 출력 {a:,}/{b:,} 프레임")))
                self.messages.put(("done",f"MP4 출력 및 프레임 수 검증 완료: {Path(path).name}"))
            except Exception as exc:self.messages.put(("done",str(exc)))
        self.worker=threading.Thread(target=work,daemon=True);self.worker.start()
        self.status.set("MP4 출력 시작. 현재 기록의 복사본을 사용합니다. 원본 영상은 변경하지 않습니다.")

    def poll(self):
        try:
            while True:
                kind,text=self.messages.get_nowait();self.status.set(text)
        except queue.Empty:pass
        self.after(150,self.poll)

    def help(self):
        messagebox.showinfo("사용 안내", "1. 영상 열기 → 일시정지 → 기체 영역을 드래그합니다.\n2. 새 기체 버튼으로 추가하거나 목록에서 선택해 위치를 수정합니다.\n3. 오른쪽 화살표/재생으로 추적 후보를 확인합니다. 불확실하면 정지합니다.\n4. 사건을 수동 기록하고 JSON 프로젝트 / CSV / MP4로 저장합니다.\n\nSpace 재생·정지 / ← → 한 프레임 / Ctrl+S 저장 / Ctrl+Z 마지막 수동 편집 되돌리기\n\n추적은 고정 템플릿의 영상 유사도 방식입니다. 학습된 드론 검출기가 아닙니다.\n가림·기체 겹침·카메라 이동·크기 변화에서 오인/유실될 수 있습니다.\n시점 이동 후에는 재지정이 필요합니다. 실측 3D·m/s·자동심판 기능은 없습니다.\n재생·출력은 무음입니다. 인터넷 접속·자동 업로드는 하지 않습니다.",parent=self)

    def close(self):
        self.playing=False
        if self.worker and self.worker.is_alive():
            self.cancel_export.set();self.status.set("내보내기를 취소 중입니다. 종료 후 창을 닫아 주세요.");return
        if not self.check_dirty():return
        if self.cap:self.cap.release()
        self.destroy()


def synthetic_video(path: Path):
    writer=cv2.VideoWriter(str(path),cv2.VideoWriter_fourcc(*"mp4v"),30,(320,240))
    if not writer.isOpened():raise RuntimeError("Synthetic MP4 writer unavailable")
    rng=np.random.default_rng(42);patch=rng.integers(0,255,(24,24,3),dtype=np.uint8)
    for i in range(45):
        image=np.zeros((240,320,3),np.uint8);image[:]=(20,30,40)
        image[70:94,35+i*2:59+i*2]=patch
        writer.write(image)
    writer.release()


def smoke_test(folder: str):
    """Real Tk window, decode, tracking, save/load, export and relocated executable smoke test."""
    root=Path(folder).resolve();root.mkdir(parents=True,exist_ok=True)
    app=None
    try:
        source=root/"synthetic.mp4";synthetic_video(source)
        app=App();app.update();app.open_video(source);app.update()
        app.label.set("합성 검증 기체");app.seed((35,70,24,24))
        for _ in range(15):app.advance()
        app.update()
        samples=app.project["tracks"]["T1"]["samples"]
        assert len(samples)==16, f"Expected 16 points, got {len(samples)}"
        assert abs(samples[-1]["x"]-77)<3, samples[-1]
        app.project["video_ref"]=source.name
        core.save_project(root/"smoke.review.json",app.project)
        loaded=core.load_project(root/"smoke.review.json");assert loaded==app.project
        core.export_csv(root,loaded)
        core.export_video(source,root/"annotated.mp4",loaded)
        Image.fromarray(cv2.cvtColor(core.annotate(app.raw,loaded,app.index),cv2.COLOR_BGR2RGB)).save(root/"overlay.png")
        from PIL import ImageGrab
        app.update()
        ImageGrab.grab(bbox=(app.winfo_rootx(),app.winfo_rooty(),app.winfo_rootx()+app.winfo_width(),app.winfo_rooty()+app.winfo_height())).save(root/"window.png")
        report=dict(passed=True,app_version=core.VERSION,points=len(samples),synthetic_only=True,
                    checks=["Tk window created","MP4 decode","template candidate tracking","JSON roundtrip","CSV output","MP4 export and all-frame decode","window screenshot"])
        (root/"result.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
    except Exception:
        (root/"error.txt").write_text(traceback.format_exc(),encoding="utf-8")
        raise
    finally:
        if app:
            if app.cap:app.cap.release()
            app.destroy()


if __name__=="__main__":
    parser=argparse.ArgumentParser(description="Offline drone soccer video review")
    parser.add_argument("--smoke-test",metavar="OUTPUT_DIR")
    args=parser.parse_args()
    if args.smoke_test:smoke_test(args.smoke_test)
    else:App().mainloop()
