#!/usr/bin/env python3
"""Semantic controls; no native execution or source mutation."""
import copy,json,pathlib,tarfile,unittest
import verify
class Controls(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with tarfile.open(pathlib.Path(__file__).with_name('evidence.tar.gz')) as t:
            prefix='exact-'+verify.COMMIT+'/'
            cls.data=[json.loads(t.extractfile(prefix+n).read()) for n in ('results.json','aggregate-summary.json','extra-ui-results.json')]
    def test_positive(self):verify.validate(*copy.deepcopy(self.data))
    def test_semantic_controls(self):
        mutations=[
          lambda r,s,e:r.__setitem__(0,r[1]),
          lambda r,s,e:next(x for x in r if x['result']!='OK').__setitem__('result','OK'),
          lambda r,s,e:next(x for x in r if x['result']!='OK').__setitem__('name','unrelated'),
          lambda r,s,e:next(x for x in r if x['result']!='OK').update(stdout='',stderr=''),
          lambda r,s,e:s.__setitem__('whole_aggregate_pass',True),
          lambda r,s,e:s.__setitem__('completed_ok_tap_cases',448),
          lambda r,s,e:e[0].__setitem__('returncode',1),
          lambda r,s,e:next(x for x in e if x['name']=='red-eye-xcf').__setitem__('stdout',''),
          lambda r,s,e:s.__setitem__('extra_graph_registered_cases',2),
        ]
        for i,mutate in enumerate(mutations):
            with self.subTest(control=i):
                data=copy.deepcopy(self.data);mutate(*data)
                with self.assertRaises(AssertionError):verify.validate(*data)
if __name__=='__main__':unittest.main()
