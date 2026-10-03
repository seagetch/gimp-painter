import json, os, time
from pathlib import Path
from gi.repository import Gimp
EVENTS = Path('/tmp/active-quit-2-py_8zyz4/events.jsonl')
def emit(kind, **extra):
    with EVENTS.open("a") as stream:
        stream.write(json.dumps(dict(event=kind, monotonic=time.monotonic(), pid=os.getpid(), **extra)) + "\n")
        stream.flush()
def await_file(filename):
    deadline = time.monotonic() + 35
    while not Path(filename).exists():
        if time.monotonic() >= deadline:
            emit("BATCH_TIMEOUT", waiting=filename)
            raise RuntimeError("External active Quit observer did not authorize next step")
        time.sleep(0.002)
images = Gimp.get_images()
if len(images) != 2:
    raise RuntimeError("Expected 2 independent loaded images, found " + str(len(images)))
sources = []
for image in images:
    source = next((layer for layer in image.get_layers() if layer.get_name() == "quit source"), None)
    effect = next((layer for layer in image.get_layers() if layer.get_name() == "quit Blinds"), None)
    if source is None or effect is None:
        raise RuntimeError("Fixture did not restore its two named layers")
    sources.append((image, source))
emit("READY", images=[image.get_id() for image, source in sources])
await_file('/tmp/active-quit-2-py_8zyz4/start')
for image, source in sources:
    if not source.update(0, 0, image.get_width(), image.get_height()):
        raise RuntimeError("Explicit source.update failed")
    emit("INVALIDATED", image=image.get_id(), drawable=source.get_id())
emit("ALL_INVALIDATED", jobs=2)
await_file('/tmp/active-quit-2-py_8zyz4/quit')
emit("QUIT_REQUEST", jobs=2)
Gimp.quit(True)
emit("QUIT_RETURNED")
