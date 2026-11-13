"""Reject a conducted image built under a production environment."""
Import("env")

flags = env.ParseFlags(env.get("BUILD_FLAGS", [])).get("CPPDEFINES", [])
names = {flag[0] if isinstance(flag, (list, tuple)) else flag for flag in flags}
bench = {"CONDUCTED_BENCH", "CONDUCTED_BENCH_ENV"}
expected = bench if env["PIOENV"] == "CONDUCTED_BENCH" else set()
if names & bench != expected:
    raise RuntimeError("Conducted bench flags must only be set in the CONDUCTED_BENCH environment")
