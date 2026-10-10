"""Exercise the real terminal in isolated directories; never touch a user's save."""
import pathlib
import re
import subprocess
import sys
import tempfile

executable = str(pathlib.Path(sys.argv[1]).resolve())
for job in (1, 2, 3):
    with tempfile.TemporaryDirectory(prefix="aincrad-smoke-") as workspace:
        def run(commands):
            result = subprocess.run([executable], input=commands, cwd=workspace,
                                    encoding="utf-8", capture_output=True, timeout=10, check=True)
            assert not result.stderr, result.stderr
            return result.stdout

        first = run(f"Smoke{job}\n{job}\n777\n0\n1\n7\n2\n5\n6\n4\n2\n")
        assert "最高层 1" in first
        save = pathlib.Path(workspace, "save.dat")
        saved = save.read_text(encoding="utf-8")
        assert "VERSION 3\n" in saved and "HIGHEST_FLOOR 1\n" in saved
        loaded = run("1\n6\n8\n0\n2\n0\n9\n4\n2\n")
        assert f"Smoke{job}" in loaded and "先完成本层目标" in loaded
        assert save.read_text(encoding="utf-8") == saved
        eof = run("1\n")
        assert "输入结束" in eof and save.read_text(encoding="utf-8") == saved
        battle = run("1\n1\n999\n2\n0\n")
        assert "无效行动" in battle and "输入结束" in battle
        assert save.read_text(encoding="utf-8") == saved
        # Overflow, trailing garbage and blank menu input are rejected and recover.
        invalid = run("1\n99999999999999999999999999999999\n7oops\n\n4\n2\n")
        assert invalid.count("请输入范围内的整数") == 3
        # Abandoning a new run must preserve the previous save.
        run(f"2\nNew{job}\n{job}\n1\n1\n3\n4\n2\n")
        assert save.read_text(encoding="utf-8") == saved
        # Loading a defeated character must enter recovery before events/menus.
        defeated = re.sub(r"^HP \d+$", "HP 0", saved, flags=re.MULTILINE)
        save.write_text(defeated, encoding="utf-8")
        recovered = run("1\n1\n4\n2\n")
        assert "角色已倒下" in recovered and "回到本层营地" in recovered
        save.write_text(defeated.replace("CHALLENGE 0", "CHALLENGE 2"), encoding="utf-8")
        iron = run("1\n")
        assert "无复苏挑战结束" in iron and "挑战本层" not in iron

print("CLI smoke passed: three jobs, V3 round-trip, camp, inventory, input bounds, battle cancellation, EOF, old save preservation.")
