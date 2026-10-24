#!/usr/bin/env python3
"""Exact additive main/include admission; never rewrites the old runtime seal."""
import hashlib,json,pathlib
P=pathlib.Path(__file__).resolve().parent
def expected(before):
 s=before.decode()
 a='int main (int argc, char **argv)';assert s.count(a)==1
 s=s.replace(a,'\n#include "test-painter-perspective-smoothing-events.inc"\n'+a)
 a='  gint result; g_test_init (&argc, &argv, NULL);';assert s.count(a)==1
 s=s.replace(a,'  gint result;\n  g_setenv ("GIMP_NO_PAINT_THREAD", "1", TRUE);\n  g_test_init (&argc, &argv, NULL);')
 a='  g_application_run (gimp->app, 0, NULL); result';assert s.count(a)==1
 s=s.replace(a,'  g_test_add_func ("/perspective-correction/paintbrush-smoothing-order", smoothing_real_paintbrush);\n'+a)
 return s.encode()
def check(before,current,p=P):
 j=json.loads((p/'lifecycle-successor.json').read_text())
 assert j['before_sha256']==hashlib.sha256(before).hexdigest()
 assert j['after_sha256']==hashlib.sha256(current).hexdigest()
 assert current==expected(before)
 return True
