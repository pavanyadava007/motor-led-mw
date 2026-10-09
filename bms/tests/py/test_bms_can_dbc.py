"""Cross-checks of the C CAN packers against the DBC (cantools) and of the C CRC against
an independent Python CRC-8 SAE J1850, through a host shared library built with gcc."""

from __future__ import annotations

import ctypes as C
import subprocess
from pathlib import Path

import cantools
import pytest

ROOT = Path(__file__).resolve().parents[3]
BMS = ROOT / "bms"


class Frame(C.Structure):
    _fields_ = [("id", C.c_uint32), ("data", C.c_uint8 * 8)]


class Status(C.Structure):
    _fields_ = [
        ("state", C.c_uint8),
        ("pack_mv", C.c_uint32),
        ("current_ma", C.c_int32),
        ("soc_permille", C.c_uint16),
        ("main_pos", C.c_bool),
        ("main_neg", C.c_bool),
        ("precharge", C.c_bool),
        ("balancing", C.c_bool),
        ("fault", C.c_bool),
        ("comm_loss", C.c_bool),
    ]


class Faults(C.Structure):
    _fields_ = [("faults", C.c_uint32), ("t_max_dc", C.c_int16), ("t_min_dc", C.c_int16)]


class Cmd(C.Structure):
    _fields_ = [("close_req", C.c_bool), ("reset_req", C.c_bool)]


@pytest.fixture(scope="module")
def lib(tmp_path_factory):
    so = tmp_path_factory.mktemp("bms") / "libbms.so"
    subprocess.run(
        [
            "gcc",
            "-std=c99",
            "-O2",
            "-shared",
            "-fPIC",
            "-ffp-contract=off",
            f"-I{BMS / 'include'}",
            *map(str, sorted((BMS / "src").glob("*.c"))),
            "-o",
            str(so),
        ],
        check=True,
    )
    return C.CDLL(str(so))


@pytest.fixture(scope="module")
def db():
    return cantools.database.load_file(str(BMS / "dbc" / "bms.dbc"))


def crc8_j1850(data: bytes) -> int:
    c = 0xFF
    for b in data:
        c ^= b
        for _ in range(8):
            c = ((c << 1) ^ 0x1D) & 0xFF if c & 0x80 else (c << 1) & 0xFF
    return c ^ 0xFF


# verifies: BMS-REQ-019
def test_crc_matches_python_reference(lib):
    lib.bms_crc8_j1850.restype = C.c_uint8
    for msg in (b"123456789", bytes(range(255)), b"\x00" * 7, b"\xff" * 3):
        buf = (C.c_uint8 * len(msg))(*msg)
        assert lib.bms_crc8_j1850(C.c_uint8(0xFF), buf, C.c_uint8(len(msg))) ^ 0xFF == crc8_j1850(msg)
    assert crc8_j1850(b"123456789") == 0x4B


@pytest.mark.parametrize(
    "state,mv,ma,soc,bits",
    [
        (3, 44123, -12345, 873, (1, 1, 0, 1, 0, 1)),
        (4, 0, 0, 0, (0, 0, 0, 0, 1, 0)),
        (2, 650000, 250000, 1000, (0, 1, 1, 0, 0, 0)),
        (1, 51234, -327680, 505, (0, 0, 0, 0, 0, 1)),
    ],
)
# verifies: BMS-REQ-018, BMS-REQ-019
def test_status_frame_decodes_with_dbc(lib, db, state, mv, ma, soc, bits):
    s = Status(state, mv, ma, soc, *[bool(b) for b in bits])
    f, cnt = Frame(), C.c_uint8(5)
    lib.bms_can_pack_status(C.byref(f), C.byref(s), C.byref(cnt))
    assert f.id == db.get_message_by_name("BMS_Status").frame_id
    d = db.decode_message(f.id, bytes(f.data), decode_choices=False)
    assert d["BMS_State"] == state and d["Status_Alive"] == 5
    assert d["PackVoltage"] == pytest.approx(min(mv // 100, 65535) * 0.1)
    assert d["PackCurrent"] == pytest.approx(max(min(int(ma / 100), 32767), -32768) * 0.1)
    assert d["SoC"] == pytest.approx(min(soc // 5, 200) * 0.5)
    names = ("MainPositive", "MainNegative", "Precharge", "Balancing", "FaultActive", "CommLoss")
    assert tuple(d[n] for n in names) == bits
    assert d["Status_CRC"] == crc8_j1850(bytes([0x10]) + bytes(f.data[1:]))


# verifies: BMS-REQ-018
def test_faults_frame_decodes_with_dbc(lib, db):
    m, f, cnt = Faults(0x00000281, 655, -125), Frame(), C.c_uint8(0)
    lib.bms_can_pack_faults(C.byref(f), C.byref(m), C.byref(cnt))
    d = db.decode_message(f.id, bytes(f.data))
    assert d["FaultBits"] == 0x281 and d["TempMax"] == 65 and d["TempMin"] == -12
    assert d["Faults_CRC"] == crc8_j1850(bytes([0x11]) + bytes(f.data[1:]))


# verifies: BMS-REQ-018
def test_cell_voltage_mux_decodes_with_dbc(lib, db):
    cells = (C.c_uint16 * 16)(*[3000 + 7 * k for k in range(16)])
    for mux in range(4):
        f, cnt = Frame(), C.c_uint8(mux)
        lib.bms_can_pack_cellv(C.byref(f), C.c_uint8(mux), cells, C.c_uint8(12), C.byref(cnt))
        d = db.decode_message(f.id, bytes(f.data))
        assert d["CellV_Mux"] == mux
        for j in range(3):
            k = 3 * mux + j
            assert d[f"Cell{k + 1:02d}"] == pytest.approx(cells[k] / 1000)
        assert d["CellV_CRC"] == crc8_j1850(bytes([0x20 + mux]) + bytes(f.data[1:]))


# verifies: BMS-REQ-018, BMS-REQ-020
def test_vcu_cmd_encoded_by_dbc_is_accepted(lib, db):
    """A frame built from the DBC by another node passes the C E2E check."""
    msg = db.get_message_by_name("VCU_Cmd")
    raw = bytearray(msg.encode({"Cmd_CRC": 0, "Cmd_Alive": 9, "CloseRequest": 1, "FaultResetRequest": 0}))
    raw[0] = crc8_j1850(bytes([0x30]) + bytes(raw[1:]))
    lib.bms_e2e_crc.restype = C.c_uint8
    buf = (C.c_uint8 * 8)(*raw)
    assert lib.bms_e2e_crc(C.c_uint8(0x30), buf) == raw[0]
    c = Cmd()
    lib.bms_can_unpack_vcu_cmd(buf, C.byref(c))
    assert c.close_req and not c.reset_req
