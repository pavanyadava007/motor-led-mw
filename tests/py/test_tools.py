"""Host-side checks of the Python tooling (no QEMU, no model needed)."""

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "agent"))

import mutate  # noqa: E402

from agent import extract_c, remove_tests, split, split_tests  # noqa: E402

SRC = """#include <zephyr/ztest.h>
ZTEST(s, test_a)
{
	if (1) { zassert_true(1); }
}

ZTEST(s, test_b)
{
	zassert_equal(2, 2);
}
ZTEST_SUITE(s, NULL, NULL, NULL, NULL, NULL);
"""


def test_split_tests_handles_nested_braces():
    names = [n for n, _, _ in split_tests(SRC)]
    assert names == ["test_a", "test_b"]


def test_remove_tests_keeps_the_rest():
    out = remove_tests(SRC, {"test_a"})
    assert "test_a" not in out and "test_b" in out and "ZTEST_SUITE" in out


def test_extract_c_takes_code_block():
    assert extract_c("text\n```c\nint x;\n```\nmore").strip() == "int x;"


def test_mutation_candidates_cover_operators():
    src = "int f(int a)\n{\n\tif (a < 3 && a != 0) {\n\t\treturn a + 1;\n\t}\n\tg = 0;\n\treturn 0;\n}\n"
    ops = {d for _, d, _, _ in mutate.candidates(src)}
    assert "'<' -> '<='" in ops and "'&&' -> '||'" in ops and "'+' -> '-'" in ops
    assert "delete statement" in ops


def test_mutation_skips_comments():
    src = "/* a < b */\n// x + y\nint z;\n"
    assert mutate.candidates(src) == []


def test_split_is_deterministic_and_disjoint():
    ms = [{"id": f"m-{k:03d}"} for k in range(100)]
    a1, b1 = split(ms)
    a2, b2 = split(ms)
    assert a1 == a2 and b1 == b2
    assert not ({m["id"] for m in a1} & {m["id"] for m in b1})
    assert len(a1) + len(b1) == 100
