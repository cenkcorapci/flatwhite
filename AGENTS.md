# localagent

Local-first agent CLI. Prefer deterministic harness controls over prompt-only safety.

## Priorities

1. correctness
2. recoverability
3. reliability
4. latency
5. resource efficiency

## Notes

- Never trust model output for permissions or destructive actions.
- Prefer FakeModelBackend / mock Ollama in CI; do not require downloaded LLMs for unit tests.
- Keep core interfaces backend-independent (`ModelBackend`, `Tool`, process runtime, permissions).
