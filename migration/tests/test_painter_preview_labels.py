"""Run original and replacement ImageMagick labeling only on temporary copies."""
import importlib.util
import hashlib
import json
import re
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('labels', ROOT / 'tools/label_painter_brush_previews.py')
labels = importlib.util.module_from_spec(spec)
spec.loader.exec_module(labels)
SOURCE = ROOT / 'data/painter-mypaint-brushes/classic/pencil_prev.png'
OLD_BLOB = 'b640164fae398977f576b30550492dfb4e2e8e65'


def pixels(path):
    return subprocess.check_output(['convert', str(path), '-depth', '8', 'RGBA:-'], stderr=subprocess.PIPE)


class PreviewLabelTests(unittest.TestCase):
    def test_full_manifest_names_and_nonpreview_bytes(self):
        lock_bytes = (ROOT / 'migration/fixtures/legacy-mypaint/assets.json').read_bytes()
        self.assertEqual(hashlib.sha256(lock_bytes).hexdigest(),
                         '018f3db6888b948c23dc1ca383e2cdb57f0df0e4a8d3b202215d29d4fe88f8e7')
        lock = json.loads(lock_bytes)
        names = {r['path'] for r in lock['assets']}
        previews = {n for n in names if n.endswith('.png')}
        self.assertEqual(len(previews), 178)
        # The unchanged old script is unsafe for arbitrary shell words. Its
        # pinned corpus is explicitly checked before executing it on copies.
        self.assertTrue(all(re.fullmatch(r'[A-Za-z0-9_+.%()/-]+', n) for n in previews))
        script = subprocess.check_output(['git', '-C', str(ROOT), 'show', OLD_BLOB])
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); old, new = root / 'old', root / 'new'
            for target in [old, new]:
                for name in names:
                    p = target / name; p.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copyfile(ROOT / 'data/painter-mypaint-brushes' / name, p)
            count = 0
            for parent in sorted({str(Path(n).parent) for n in previews}):
                subprocess.run(['bash'], input=script, cwd=old / parent, check=True,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                count += labels.label(new / parent)
            self.assertEqual(count, 178)
            self.assertEqual({p.relative_to(new).as_posix() for p in new.rglob('*') if p.is_file()}, names)
            for name in names:
                if name in previews:
                    self.assertEqual(pixels(old / name), pixels(new / name), name)
                    self.assertEqual(len(pixels(new / name)), 128 * 128 * 4, name)
                else:
                    self.assertEqual((new / name).read_bytes(),
                                     (ROOT / 'data/painter-mypaint-brushes' / name).read_bytes(), name)

    def test_exact_original_pixels_for_safe_names(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            old, new = root / 'old', root / 'new'
            old.mkdir(); new.mkdir()
            names = ['ink_name_prev.png', 'other.png', 'odd_prev.png_tail.png']
            for n in names:
                shutil.copyfile(SOURCE, old / n); shutil.copyfile(SOURCE, new / n)
            script = subprocess.check_output(['git', '-C', str(ROOT), 'show', OLD_BLOB])
            subprocess.run(['bash'], input=script, cwd=old, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self.assertEqual(labels.label(new), 3)
            for n in names:
                self.assertEqual(pixels(old / n), pixels(new / n))
                self.assertEqual(len(pixels(new / n)), 128 * 128 * 4)

    def test_space_unicode_dash_filename_pixels(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp)
            # Independent literal old pipeline, with only shell tokenization
            # corrected. No invented font or rendering operation is substituted.
            for filename, caption in [('筆 name_prev.png', '筆 name'), ('-ink_name_prev.png', '-ink-name')]:
                source = p / filename
                reference = p / 'expected.bin'
                shutil.copyfile(SOURCE, source)
                subprocess.run(['convert', str(source), '-fill', 'black', '-font', 'fixed',
                    '-gravity', 'South', '-background', '#FFF8', '-size', '124x26',
                    'caption:' + caption, '-composite', '-bordercolor', '#CCC9', '-border', '2',
                    '-bordercolor', '#9999', '-border', '1', '-resize', '128x128',
                    '-unsharp', '0x.5', '-flatten', '+matte', 'PNG:' + str(reference)],
                    check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                labels.label(p)
                self.assertEqual(pixels(source), pixels(reference))
                source.unlink(); reference.unlink()

    def test_only_immediate_visible_png_and_mode_preserved(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp); (p / 'child').mkdir()
            for name in ['preview.png', '.hidden.png', 'child/nested.png', 'brush.myb']:
                shutil.copyfile(SOURCE, p / name)
            (p / 'preview.png').chmod(0o640)
            original = SOURCE.read_bytes()
            self.assertEqual(labels.label(p), 1)
            self.assertEqual((p / 'preview.png').stat().st_mode & 0o777, 0o640)
            for name in ['.hidden.png', 'child/nested.png', 'brush.myb']:
                self.assertEqual((p / name).read_bytes(), original)

    def test_failed_conversion_preserves_original_and_cleans_temp(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp); shutil.copyfile(SOURCE, p / 'preview.png')
            original = (p / 'preview.png').read_bytes()
            with self.assertRaises(subprocess.CalledProcessError): labels.label(p, 'false')
            self.assertEqual((p / 'preview.png').read_bytes(), original)
            self.assertEqual([f.name for f in p.iterdir()], ['preview.png'])
            with self.assertRaises(RuntimeError): labels.label(p, 'true')
            self.assertEqual((p / 'preview.png').read_bytes(), original)
            self.assertEqual([f.name for f in p.iterdir()], ['preview.png'])

    def test_symlink_rejected_before_any_overwrite(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp); shutil.copyfile(SOURCE, p / 'a.png')
            (p / 'z.png').symlink_to(p / 'a.png')
            with self.assertRaises(ValueError): labels.label(p)
            self.assertEqual((p / 'a.png').read_bytes(), SOURCE.read_bytes())

    def test_empty_directory_and_exact_caption_substitution(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.assertEqual(labels.label(tmp, 'not-invoked'), 0)
        self.assertEqual(labels.caption('a_prev.png_b_prev.png'), 'a-b-prev.png')
        self.assertEqual(labels.caption('other_file.png'), 'other-file.png')


if __name__ == '__main__':
    unittest.main()
