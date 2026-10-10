"""Exercise the real terminal program without touching a user's save.dat."""
import pathlib
import subprocess
import sys
import tempfile

executable = str(pathlib.Path(sys.argv[1]).resolve())
for job in (1, 2, 3):
    with tempfile.TemporaryDirectory(prefix="aincrad-smoke-") as workspace:
        def run(commands):
            result = subprocess.run(
                [executable], input=commands, cwd=workspace,
                encoding="utf-8", capture_output=True, timeout=5, check=True
            )
            assert not result.stderr, result.stderr
            return result.stdout

        first = run(f"Smoke{job}\n{job}\n7\n2\n5\n6\n4\n2\n")
        assert "最高到达楼层: 1" in first
        save = pathlib.Path(workspace, "save.dat")
        saved = save.read_text(encoding="utf-8")
        assert "VERSION 2\n" in saved and "HIGHEST_FLOOR 1\n" in saved
        loaded = run("1\n6\n4\n2\n")
        assert f"Smoke{job}" in loaded and "最高到达楼层: 1" in loaded
        assert save.read_text(encoding="utf-8") == saved
        # EOF in a menu must terminate, never loop printing forever or rewrite a save.
        eof = run("1\n")
        assert "输入结束" in eof
        assert save.read_text(encoding="utf-8") == saved

print("CLI smoke tests passed: three jobs, save/load, clean exit and EOF.")
