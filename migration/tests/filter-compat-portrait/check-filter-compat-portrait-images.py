from pathlib import Path
from PIL import Image
import json,hashlib,argparse
ap=argparse.ArgumentParser();ap.add_argument('directory',type=Path);ap.add_argument('--report',type=Path,required=True);a=ap.parse_args();rows={}
expected={'normalize-before':[(120,80,40),(40,120,180)],'normalize-after':[(145,72,0),(0,145,255)],'exchange-before':[(120,80,40),(40,120,180)],'exchange-after':[(240,30,100),(40,120,180)]}
for name,colors in expected.items():
 p=a.directory/(name+'-oncanvas-portrait.png');im=Image.open(p).convert('RGB');assert im.size==(1080,1920);hist=im.getcolors(im.width*im.height);counts=[]
 for color in colors:
  n=sum(count for count,pixel in hist if all(abs(pixel[c]-color[c])<=3 for c in range(3)));assert n>=1000,(name,color,n);counts.append({'expected_display_rgb':color,'channel_tolerance':3,'pixels':n})
 rows[name]={'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'dimensions':im.size,'independent_patch_colors':counts}
assert rows['normalize-before']['sha256']!=rows['normalize-after']['sha256'];assert rows['exchange-before']['sha256']!=rows['exchange-after']['sha256'];a.report.write_text(json.dumps(rows,indent=2)+'\n');print('PASS4 real portrait PNGs,8 independently expected displayed patch colors,all >=1000 pixels')
