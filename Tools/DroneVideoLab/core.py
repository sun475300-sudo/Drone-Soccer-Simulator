"""Offline image-plane review tools. Tracking outputs are candidates, not ground truth."""
from __future__ import annotations
import csv
import hashlib
import json
import math
import os
from pathlib import Path
from typing import Any
import cv2
import numpy as np

VERSION = "0.1.0-preview"
SCHEMA = 1
TEAMS = ("Red", "Blue", "Unknown")
COLORS = {"Red": (80, 90, 245), "Blue": (245, 195, 60), "Unknown": (70, 220, 210)}


def sha256_file(path: str | Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def video_info(path: str | Path) -> dict:
    cap = cv2.VideoCapture(str(path))
    try:
        if not cap.isOpened():
            raise ValueError("영상을 열 수 없습니다. 파일 손상 또는 지원하지 않는 코덱을 확인하세요.")
        fps = float(cap.get(cv2.CAP_PROP_FPS))
        frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
        width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        if not math.isfinite(fps) or fps <= 0 or frames <= 0 or min(width, height) <= 0:
            raise ValueError("영상 시간/크기 정보를 읽을 수 없습니다.")
        return dict(name=Path(path).name, sha256=sha256_file(path), width=width,
                    height=height, fps=fps, frame_count=frames, duration_estimate_s=frames/fps)
    finally:
        cap.release()


def read_frame(cap, index: int, fps: float, seek: bool = True):
    if seek:
        cap.set(cv2.CAP_PROP_POS_FRAMES, index)
    ok, image = cap.read()
    if not ok:
        raise ValueError(f"{index}번 프레임을 읽지 못했습니다.")
    raw = float(cap.get(cv2.CAP_PROP_POS_MSEC)) / 1000
    valid = math.isfinite(raw) and (raw > 0 or index == 0)
    return image, raw if valid else index/fps, "container_pts" if valid else "fps_estimate"


def new_project(info: dict) -> dict:
    return dict(schema_version=SCHEMA, app_version=VERSION, video=info, video_ref=info["name"],
                coordinate_system="image_pixels_not_world", tracks={}, events=[])


def sample(frame, time_s, time_source, box, confidence, status, segment):
    x, y, w, h = map(float, box)
    return dict(frame=int(frame), time_s=float(time_s), time_source=time_source,
                x=x+w/2, y=y+h/2, w=w, h=h, confidence=float(confidence),
                status=status, segment=int(segment))


def upsert(track: dict, point: dict):
    # One observation per track per frame; a manual correction replaces that frame only.
    for i, old in enumerate(track["samples"]):
        if old["frame"] == point["frame"]:
            track["samples"][i] = point
            return
    track["samples"].append(point)


def validate_project(data: Any) -> dict:
    if not isinstance(data, dict) or data.get("schema_version") != SCHEMA:
        raise ValueError("지원하지 않는 프로젝트 버전입니다.")
    if data.get("coordinate_system") != "image_pixels_not_world":
        raise ValueError("좌표계가 올바르지 않습니다.")
    v = data["video"]
    for key in ("width", "height", "frame_count"):
        if not isinstance(v[key], int) or v[key] <= 0:
            raise ValueError("잘못된 영상 정보입니다.")
    if not math.isfinite(v["fps"]) or v["fps"] <= 0 or len(v["sha256"]) != 64:
        raise ValueError("잘못된 영상 식별 정보입니다.")
    tracks = data["tracks"]
    if not isinstance(tracks, dict) or len(tracks) > 100:
        raise ValueError("트랙은 100개 이하만 지원합니다.")
    for tid, track in tracks.items():
        if not isinstance(tid, str) or not tid.startswith("T") or not tid[1:].isdigit():
            raise ValueError("잘못된 트랙 ID입니다.")
        if track["team"] not in TEAMS or not isinstance(track["label"], str) or len(track["label"]) > 120:
            raise ValueError("잘못된 트랙 정보입니다.")
        points = track["samples"]
        if not isinstance(points, list) or len(points) > 2_000_000:
            raise ValueError("관측 데이터가 너무 큽니다.")
        frames = set()
        for p in points:
            if not isinstance(p["frame"], int) or not 0 <= p["frame"] < v["frame_count"] or p["frame"] in frames:
                raise ValueError("중복되거나 범위를 벗어난 프레임입니다.")
            frames.add(p["frame"])
            for key in ("time_s", "x", "y", "w", "h", "confidence"):
                if not isinstance(p[key], (int, float)) or not math.isfinite(p[key]):
                    raise ValueError("유효하지 않은 관측 수치입니다.")
            if not (0 <= p["x"] <= v["width"] and 0 <= p["y"] <= v["height"] and
                    0 < p["w"] <= v["width"] and 0 < p["h"] <= v["height"] and
                    0 <= p["confidence"] <= 1 and p["time_s"] >= 0):
                raise ValueError("영상 범위를 벗어난 관측입니다.")
            if p["status"] not in ("manual", "candidate") or p["time_source"] not in ("container_pts", "fps_estimate"):
                raise ValueError("관측 출처가 올바르지 않습니다.")
            if not isinstance(p["segment"], int) or p["segment"] < 0:
                raise ValueError("잘못된 연속 구간입니다.")
    if not isinstance(data["events"], list) or len(data["events"]) > 100_000:
        raise ValueError("잘못된 사건 목록입니다.")
    for e in data["events"]:
        if (not isinstance(e["frame"], int) or not 0 <= e["frame"] < v["frame_count"] or
                not isinstance(e["time_s"], (float, int)) or not math.isfinite(e["time_s"]) or e["time_s"] < 0 or
                any(not isinstance(e.get(k), str) for k in ("type", "team", "note"))):
            raise ValueError("잘못된 사건 항목입니다.")
    return data


def save_project(path: str | Path, data: dict):
    validate_project(data)
    path = Path(path)
    temp = path.with_name(path.name + ".tmp")
    try:
        temp.write_text(json.dumps(data, ensure_ascii=False, indent=2, allow_nan=False), encoding="utf-8")
        os.replace(temp, path)
    finally:
        temp.unlink(missing_ok=True)


def load_project(path: str | Path) -> dict:
    if Path(path).stat().st_size > 80_000_000:
        raise ValueError("프로젝트 파일은 80MB 이하만 지원합니다.")
    return validate_project(json.loads(Path(path).read_text(encoding="utf-8")))


def csv_safe(value):
    text = str(value)
    return "'"+text if text.lstrip().startswith(("=", "+", "-", "@", "\t", "\r")) else text


def speed(previous, point):
    # No bridging lost intervals, seeks or corrected tracking segments.
    dt = point["time_s"] - previous["time_s"]
    if (point["segment"] != previous["segment"] or point["frame"] != previous["frame"]+1 or not 0 < dt <= .5):
        return None
    return math.hypot(point["x"]-previous["x"], point["y"]-previous["y"])/dt


def export_csv(folder: str | Path, project: dict):
    folder = Path(folder)
    folder.mkdir(parents=True, exist_ok=True)
    with (folder/"tracks.csv").open("w", newline="", encoding="utf-8-sig") as handle:
        out = csv.writer(handle)
        out.writerow(["track_id", "label", "team", "frame", "time_s", "time_source", "x_px", "y_px",
                      "x_normalized", "y_normalized", "width_px", "height_px", "similarity_not_probability",
                      "status", "segment", "image_speed_px_s"])
        v = project["video"]
        for tid, track in project["tracks"].items():
            previous = None
            for p in sorted(track["samples"], key=lambda q: q["frame"]):
                velocity = speed(previous, p) if previous else None
                out.writerow([tid, csv_safe(track["label"]), track["team"], p["frame"], p["time_s"], p["time_source"],
                              p["x"], p["y"], p["x"]/v["width"], p["y"]/v["height"], p["w"], p["h"],
                              p["confidence"], p["status"], p["segment"], "" if velocity is None else velocity])
                previous = p
    with (folder/"events.csv").open("w", newline="", encoding="utf-8-sig") as handle:
        out = csv.writer(handle)
        out.writerow(["frame", "time_s", "type", "team", "note", "source"])
        for event in project["events"]:
            out.writerow([event["frame"], event["time_s"], csv_safe(event["type"]),
                          csv_safe(event["team"]), csv_safe(event["note"]), "manual_not_referee_decision"])


class TemplateTracker:
    """Short-range fixed-template NCC baseline; it does not detect or re-identify drones."""
    def __init__(self, frame: np.ndarray, box, threshold: float = .68, margin: float = .04):
        x, y, w, h = map(int, box)
        height, width = frame.shape[:2]
        if min(w, h) < 8 or x < 0 or y < 0 or x+w > width or y+h > height:
            raise ValueError("기체 주변을 영상 안에서 8×8픽셀 이상으로 지정하세요.")
        self.template = cv2.cvtColor(frame[y:y+h, x:x+w], cv2.COLOR_BGR2GRAY)
        if float(self.template.std()) < 5:
            raise ValueError("선택 영역의 명암 차이가 너무 작아 추적할 수 없습니다.")
        self.box = (x, y, w, h)
        self.threshold, self.margin = threshold, margin
        self.active = True
        self.reason = ""

    def update(self, frame: np.ndarray):
        if not self.active:
            return None
        x, y, w, h = self.box
        radius = max(32, int(max(w, h)*1.5))
        left, top = max(0, x-radius), max(0, y-radius)
        right, bottom = min(frame.shape[1], x+w+radius), min(frame.shape[0], y+h+radius)
        gray = cv2.cvtColor(frame[top:bottom, left:right], cv2.COLOR_BGR2GRAY)
        if gray.shape[0] < h or gray.shape[1] < w:
            self.active, self.reason = False, "영상 경계"
            return None
        scores = cv2.matchTemplate(gray, self.template, cv2.TM_CCOEFF_NORMED)
        _, best, _, location = cv2.minMaxLoc(scores)
        second = scores.copy()
        px, py = location
        rx, ry = max(3, w//2), max(3, h//2)
        second[max(0, py-ry):py+ry+1, max(0, px-rx):px+rx+1] = -1
        gap = best-float(second.max())
        if not math.isfinite(best) or best < self.threshold or gap < self.margin:
            self.active, self.reason = False, "유사도 부족 또는 위치 모호"
            return None
        self.box = (left+px, top+py, w, h)
        return self.box, float(min(1, max(0, best)))


def overlap(a, b):
    x1, y1, w1, h1 = a
    x2, y2, w2, h2 = b
    area = max(0, min(x1+w1, x2+w2)-max(x1, x2))*max(0, min(y1+h1, y2+h2)-max(y1, y2))
    return area/max(1, min(w1*h1, w2*h2))


def annotate(frame: np.ndarray, project: dict, index: int, history: float = 2.):
    out = frame.copy()
    fps = project["video"]["fps"]
    for tid, track in project["tracks"].items():
        color = COLORS[track["team"]]
        points = sorted((p for p in track["samples"] if index-int(history*fps) <= p["frame"] <= index), key=lambda p:p["frame"])
        for a, b in zip(points, points[1:]):
            if a["segment"] == b["segment"] and b["frame"] == a["frame"]+1:
                cv2.line(out, (round(a["x"]), round(a["y"])), (round(b["x"]), round(b["y"])), color, 2, cv2.LINE_AA)
        now = next((p for p in points if p["frame"] == index), None)
        if now:
            x, y, w, h = now["x"]-now["w"]/2, now["y"]-now["h"]/2, now["w"], now["h"]
            cv2.rectangle(out, (round(x),round(y)), (round(x+w),round(y+h)), color, 2)
            label = f"{tid} {now['status']} {now['confidence']:.2f}"
            cv2.putText(out, label, (max(0,round(x)),max(15,round(y)-6)), cv2.FONT_HERSHEY_SIMPLEX,.48,color,1,cv2.LINE_AA)
    text=f"Frame {index} | 2D IMAGE SPACE | CANDIDATES NEED REVIEW"
    scale=min(.52,(out.shape[1]-24)/max(1,cv2.getTextSize(text,cv2.FONT_HERSHEY_SIMPLEX,1,1)[0][0]))
    cv2.putText(out,text,(12,26),cv2.FONT_HERSHEY_SIMPLEX,scale,(255,255,255),1,cv2.LINE_AA)
    return out


def heatmap(project: dict, background: np.ndarray):
    h, w = background.shape[:2]
    counts = np.zeros((h,w), np.float32)
    for track in project["tracks"].values():
        for p in track["samples"]:
            counts[min(h-1,max(0,round(p["y"]))), min(w-1,max(0,round(p["x"])))] += 1
    if not counts.any():
        raise ValueError("관측 위치가 없습니다.")
    smooth = cv2.GaussianBlur(counts, (0,0), max(4,min(h,w)/70))
    level = np.uint8(smooth/max(1e-9,float(smooth.max()))*255)
    colored = cv2.applyColorMap(level, cv2.COLORMAP_TURBO)
    alpha = (level.astype(np.float32)/255*.7)[...,None]
    out = np.uint8(background*(1-alpha)+colored*alpha)
    cv2.putText(out,"OBSERVATION COUNTS / IMAGE SPACE / NOT OCCUPANCY TIME",(12,28),cv2.FONT_HERSHEY_SIMPLEX,.48,(255,255,255),1)
    return out


def export_video(source, destination, project, cancel=None, progress=None):
    source, destination = Path(source), Path(destination)
    if source.resolve() == destination.resolve():
        raise ValueError("원본 영상과 같은 경로로 내보낼 수 없습니다.")
    cap = cv2.VideoCapture(str(source))
    writer = None
    partial = destination.with_name(destination.stem+".partial.mp4")
    try:
        v = project["video"]
        writer = cv2.VideoWriter(str(partial), cv2.VideoWriter_fourcc(*"mp4v"), v["fps"], (v["width"],v["height"]))
        if not cap.isOpened() or not writer.isOpened():
            raise ValueError("MP4 디코더/인코더를 열 수 없습니다.")
        for i in range(v["frame_count"]):
            if cancel and cancel.is_set():
                raise InterruptedError("내보내기를 취소했습니다.")
            ok, frame = cap.read()
            if not ok:
                raise ValueError(f"내보내기 중 {i}번 프레임 읽기 실패")
            writer.write(annotate(frame, project, i))
            if progress and i % 30 == 0:
                progress(i+1, v["frame_count"])
        writer.release()
        writer = None
        check = cv2.VideoCapture(str(partial))
        decoded = 0
        try:
            while True:
                ok, _ = check.read()
                if not ok: break
                decoded += 1
        finally:
            check.release()
        if decoded != v["frame_count"]:
            raise ValueError(f"출력 프레임 검증 실패: {decoded}/{v['frame_count']}")
        os.replace(partial, destination)
    finally:
        cap.release()
        if writer is not None: writer.release()
        partial.unlink(missing_ok=True)
