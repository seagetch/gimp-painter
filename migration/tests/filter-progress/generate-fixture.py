from pathlib import Path
from PIL import Image, ImageDraw

# Exact deterministic image recipe used for the desktop validation.
# Only writes progress-fixture.png next to this script.
run = Path(__file__).resolve().parent
w = h = 3072
im = Image.new('RGB', (w, h))
px = im.load()
for y in range(h):
    for x in range(w):
        checker = 28 if ((x // 192 + y // 192) % 2) else 0
        px[x, y] = (
            int(20 + 200 * x / (w - 1)),
            int(25 + 180 * y / (h - 1)),
            int(35 + 150 * (x + y) / (w + h - 2)) + checker,
        )
d = ImageDraw.Draw(im)
for k, c in enumerate([(245, 78, 45), (30, 185, 155), (40, 85, 225)]):
    x = 450 + k * 850
    d.ellipse((x, 1000, x + 550, 1550), fill=c, outline=(255, 245, 220), width=24)
d.rectangle((280, 2300, 2800, 2420), fill=(245, 230, 200))
im.save(run / 'progress-fixture.png')
