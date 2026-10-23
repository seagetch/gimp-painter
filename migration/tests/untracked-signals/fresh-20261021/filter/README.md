# Filter dependency signal lifetime acceptance, 2026-10-21

All four exact dependency paths passed both the ordinary registered harness and focused ASan/UBSan harness. `verification.json` compares actual TAP paths, selected paths and source hashes. Normal source and sanitizer source hashes agree. No source changed during instrumentation.

Ordinary runs retain their registered 600-second watchdog (fixture setup takes 65–68 seconds per case). The focused four-case sanitizer run uses the existing helper's 180-second watchdog. Shared build lock was released after the normal phase; the sanitizer phase reacquired it separately. Authenticated Xvfb 1080x1920x24 throughout.

Scope: 20 instrumented sources plus 50 RTTI-compatibility-only bridge sources; not a full application/dependency sanitizer run, nor a whole FilterLayer suite pass. Leak detection disabled due container ptrace restrictions. All diagnostics preserved, including skipped `.dummy` plug-ins. No sanitizer error occurred. Inherited environments were reduced to the existing repository sanitizer's explicit reproducibility allowlist. Earlier historical failed controls in the parent directory are untouched.
