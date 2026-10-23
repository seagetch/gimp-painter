# Application lifecycle owner signal checks

Actual native startup7/7, HTTP18/18, bounded ASan/UBSan/vptr HTTP18/18 and GTK
HTTP6/6 passed. The UI sanitizer ran on authenticated portrait1080x1920 Xvfb.
See report.json for exact source/executable hashes, instrumentation scope and
limitations. These tests cover owner disposal and queued callback cancellation;
they do not replace separate real-app startup/accepted-exit or screenshot evidence.

The core test initializer runs initialize/restore but not app.c's external-input
ready flag assignment. An initial incorrect TRUE assertion failed; the corrected
FALSE assertion documents that boundary. The original failure is retained in the
outside-repository application-lifetime-evidence directory. No production code
was changed for these assertions. Raw Meson inherited environments are excluded;
only selected test results and actual test stdout are retained here.
