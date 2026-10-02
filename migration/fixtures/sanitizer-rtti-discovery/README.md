# Historical RTTI discovery helper

`initial-cpp-only.py.gz` preserves the exact helper used by the initial native
MyPaint/Save/Fill focused runtime checkpoints. SHA256 of the decompressed source
is a050cd5fea3fb0f15e9b4a46531a213bc64b1bcc0344b091212571a6b5d09402.
It discovers `.cpp` sources. The current helper extends discovery to other C++
suffixes and `arguments` compile records, validated by three metadata tests.
Historical runtime lists are not relabeled as testing the expanded closure.
