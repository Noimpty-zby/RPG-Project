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
        assert "VERSION 4\n" in saved and "HIGHEST_FLOOR 1\n" in saved
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
        profile = pathlib.Path(workspace, "profile.dat")
        assert not profile.exists() or "RECORD" not in profile.read_text(encoding="utf-8")

        # Help, archive, travel and partial stack sales never touch the save.
        save.write_text(saved, encoding="utf-8")
        menus = run("1\n12\n1\n6\n0\n11\n1\n1\n0\n2\n3\n4\n0\n10\n1\n2\n3\n1\n2\n0\n"
                    "8\n8\n1\n0\n9\n0\n0\n4\n2\n")
        assert "基础流程" in menus and "[剧毒]" in menus and "敌人图鉴：已发现" in menus
        assert "强化已穿戴装备" in menus and " +1 | " in menus and "无需修理" in menus
        assert "成就：" in menus and "还没有完成的远征" in menus
        assert "卖出数量（1-4" in menus and "×2，获得" in menus
        assert save.read_text(encoding="utf-8") == saved

        # Batch potions show their plan before drinking toward the chosen HP.
        save.write_text(re.sub(r"^HP \d+$", "HP 20", saved, flags=re.MULTILINE), encoding="utf-8")
        potions = run("1\n2\n6\n100\n1\n0\n4\n2\n")
        assert "将使用：" in potions and "溢出" in potions and re.search(r"共恢复 [1-9]\d* 生命", potions)
        save.write_text(saved, encoding="utf-8")

        # Inspecting during a fight does not spend the turn; finishing a fight
        # records the enemy in the profile's bestiary.
        fought = run("1\n1\n5\n" + "1\n" * 12 + "4\n2\n")
        assert "—— 敌人 ——" in fought and "—— 自身 ——" in fought and "目标完成" in fought
        assert "SEEN " in profile.read_text(encoding="utf-8")

        # A death that ends a no-revive run is recorded once; reloading the
        # same save afterwards does not log the run again.
        dying = re.sub(r"^HP \d+$", "HP 1", saved, flags=re.MULTILINE).replace("CHALLENGE 0", "CHALLENGE 2")
        save.write_text(dying, encoding="utf-8")
        ended = run("1\n1\n" + "3\n" * 10)
        assert "无复苏挑战结束" in ended and "本次远征已记入档案" in ended
        assert profile.read_text(encoding="utf-8").count("RECORD ") == 1
        save.write_text(defeated.replace("CHALLENGE 0", "CHALLENGE 2"), encoding="utf-8")
        run("1\n")
        assert profile.read_text(encoding="utf-8").count("RECORD ") == 1

print("CLI smoke passed: three jobs, V4 round-trip, camp, inventory, input bounds, battle cancellation, EOF, "
      "old save preservation, help, archive, travel, inspect, stack sales, batch potions, enhance/repair, run records.")
