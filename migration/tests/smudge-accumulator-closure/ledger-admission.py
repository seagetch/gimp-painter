import pathlib,json,hashlib
P=pathlib.Path(__file__).resolve().parent
def admit(historical,current):
 b=(P/'rows.json').read_bytes()
 assert hashlib.sha256(b).hexdigest()=='21009fff98576d13936f53d418cae72d553e56405f5da8dff25cf38ca509f0b3','closure map changed'
 rows=json.loads(b)['rows'];matches=[x for x in rows if x['before']['work_id']==historical['work_id']]
 assert len(matches)==1
 row=matches[0];before={k:v for k,v in historical.items()if k not in ['payload','other_whole_hunk_owners']}
 assert before==row['before'],'historical row changed'
 assert current in [row['before'],row['after']],'unadmitted ledger change'
