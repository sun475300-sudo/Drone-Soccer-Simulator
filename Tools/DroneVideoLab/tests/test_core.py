import copy
import csv
import json
from pathlib import Path
import tempfile
import threading
import unittest
import cv2
import numpy as np
import core


class CoreTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
        self.rng=np.random.default_rng(91)
        self.patch=self.rng.integers(0,255,(20,20,3),dtype=np.uint8)
        self.first=np.zeros((150,220,3),np.uint8)
        self.first[40:60,40:60]=self.patch
        self.second=np.zeros_like(self.first);self.second[45:65,48:68]=self.patch
        self.info=dict(name='test.mp4',sha256='0'*64,width=220,height=150,fps=30.,frame_count=100,duration_estimate_s=100/30)
        self.project=core.new_project(self.info)
        self.project['tracks']['T1']=dict(label='test',team='Red',samples=[core.sample(0,0.,'container_pts',(40,40,20,20),1.,'manual',1)])

    def tearDown(self):self.tmp.cleanup()
    def test_tracking_translation(self):
        tracker=core.TemplateTracker(self.first,(40,40,20,20))
        box,score=tracker.update(self.second)
        self.assertEqual(box,(48,45,20,20));self.assertGreater(score,.99)
    def test_tracking_loss_stops(self):
        tracker=core.TemplateTracker(self.first,(40,40,20,20))
        self.assertIsNone(tracker.update(np.zeros_like(self.first)));self.assertFalse(tracker.active)
        self.assertIsNone(tracker.update(self.first))
    def test_blank_seed_rejected(self):
        with self.assertRaises(ValueError):core.TemplateTracker(self.first,(0,0,20,20))
    def test_seed_outside_rejected(self):
        with self.assertRaises(ValueError):core.TemplateTracker(self.first,(-1,40,20,20))
    def test_ambiguous_targets_rejected(self):
        second=self.first.copy();second[40:60,70:90]=self.patch
        self.assertIsNone(core.TemplateTracker(self.first,(40,40,20,20)).update(second))
    def test_schema_rejected(self):
        project=copy.deepcopy(self.project);project['schema_version']=99
        with self.assertRaises(ValueError):core.validate_project(project)
    def test_invalid_coordinate_rejected(self):
        self.project['tracks']['T1']['samples'][0]['x']=float('nan')
        with self.assertRaises(ValueError):core.validate_project(self.project)
    def test_duplicate_frame_rejected(self):
        p=self.project['tracks']['T1']['samples'];p.append(copy.deepcopy(p[0]))
        with self.assertRaises(ValueError):core.validate_project(self.project)
    def test_roundtrip(self):
        path=self.root/'review.json';core.save_project(path,self.project)
        self.assertEqual(core.load_project(path),self.project);self.assertFalse((self.root/'review.json.tmp').exists())
    def test_replace_same_frame(self):
        track=self.project['tracks']['T1'];p=copy.deepcopy(track['samples'][0]);p['x']=100
        core.upsert(track,p);self.assertEqual(len(track['samples']),1);self.assertEqual(track['samples'][0]['x'],100)
    def test_speed_contiguous(self):
        a=core.sample(0,0,'container_pts',(10,10,20,20),1,'manual',1)
        b=core.sample(1,.1,'container_pts',(13,14,20,20),.9,'candidate',1)
        self.assertAlmostEqual(core.speed(a,b),50)
    def test_no_speed_across_gap(self):
        a=core.sample(0,0,'container_pts',(10,10,20,20),1,'manual',1)
        for frame,stamp,segment in [(2,.1,1),(1,.1,2),(1,0,1),(1,1,1)]:
            b=core.sample(frame,stamp,'container_pts',(13,14,20,20),.9,'candidate',segment)
            self.assertIsNone(core.speed(a,b))
    def test_csv_formula_guard(self):
        self.project['tracks']['T1']['label']='=1+1';core.export_csv(self.root,self.project)
        with (self.root/'tracks.csv').open(encoding='utf-8-sig') as f:rows=list(csv.reader(f))
        self.assertEqual(rows[1][1],"'=1+1")
    def test_empty_heatmap_rejected(self):
        with self.assertRaises(ValueError):core.heatmap(core.new_project(self.info),self.first)
    def test_heatmap_dimensions(self):self.assertEqual(core.heatmap(self.project,self.first).shape,self.first.shape)
    def test_no_phantom_box_after_loss(self):
        plain=np.zeros((150,220,3),np.uint8)
        annotated=core.annotate(plain,self.project,80)
        self.assertFalse(annotated[40:61,40:61].any())
    def test_overlap(self):
        self.assertEqual(core.overlap((0,0,20,20),(0,0,20,20)),1)
        self.assertEqual(core.overlap((0,0,20,20),(100,100,20,20)),0)
    def test_hash(self):
        p=self.root/'a';p.write_bytes(b'abc')
        self.assertEqual(core.sha256_file(p),'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad')
    def test_export_original_protected(self):
        with self.assertRaises(ValueError):core.export_video(self.root/'a.mp4',self.root/'a.mp4',self.project)
    def test_invalid_event(self):
        self.project['events'].append(dict(frame=-1,time_s=0.,type='goal',team='Red',note='test'))
        with self.assertRaises(ValueError):core.validate_project(self.project)


if __name__=='__main__':unittest.main()
