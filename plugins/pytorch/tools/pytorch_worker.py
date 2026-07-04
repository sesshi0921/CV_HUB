#!/usr/bin/env python3
"""PyTorch plugin worker — communicates with C++ host via stdin/stdout binary protocol.

Protocol:
  C++ → Python (stdin):
    [uint32 LE: header_len][JSON bytes][raw image bytes]
    JSON: {"fn":"<name>","params":{...},"w":int,"h":int,"c":int,"fmt":"RGB8"|"BGR8"|"Gray8"}

  Python → C++ (stdout):
    [uint32 LE: header_len][JSON bytes][raw image bytes if ok]
    JSON success: {"ok":true,"w":int,"h":int,"c":int,"fmt":"..."}
    JSON error:   {"ok":false,"error":"..."}
"""

import json
import struct
import sys
import traceback

# Redirect text stdout to stderr BEFORE importing any library.
# This prevents torchvision/numpy/PIL from polluting the binary protocol channel.
_proto_out = sys.stdout.buffer
sys.stdout = sys.stderr

import numpy as np  # noqa: E402  # pylint: disable=wrong-import-position
import torchvision.transforms.functional as F  # noqa: E402  # pylint: disable=wrong-import-position
from PIL import Image  # noqa: E402  # pylint: disable=wrong-import-position


def _read_message():
    header_len_raw = sys.stdin.buffer.read(4)
    if len(header_len_raw) < 4:
        return None, None
    (header_len,) = struct.unpack("<I", header_len_raw)
    header = json.loads(sys.stdin.buffer.read(header_len).decode("utf-8"))
    w, h, c = header["w"], header["h"], header["c"]
    image_bytes = sys.stdin.buffer.read(w * h * c)
    return header, image_bytes


def _write_response(header: dict, image_bytes: bytes):
    header_json = json.dumps(header).encode("utf-8")
    _proto_out.write(struct.pack("<I", len(header_json)))
    _proto_out.write(header_json)
    _proto_out.write(image_bytes)
    _proto_out.flush()


def _write_error(msg: str):
    _write_response({"ok": False, "error": msg}, b"")


def _bytes_to_pil(header: dict, image_bytes: bytes):
    w, h, c = header["w"], header["h"], header["c"]
    fmt = header["fmt"]
    arr = np.frombuffer(image_bytes, dtype=np.uint8)
    if c == 1:
        arr = arr.reshape(h, w)
        return Image.fromarray(arr, mode="L"), "Gray8"
    arr = arr.reshape(h, w, c)
    if fmt == "BGR8":
        arr = arr[:, :, ::-1].copy()
        return Image.fromarray(arr, mode="RGB"), "BGR8"
    return Image.fromarray(arr, mode="RGB"), "RGB8"


def _pil_to_bytes(img: Image.Image, original_fmt: str):
    mode = img.mode
    arr = np.array(img)
    if mode == "L":
        h, w = arr.shape
        c = 1
        out_fmt = "Gray8"
    else:
        h, w, c = arr.shape
        if original_fmt == "BGR8":
            arr = arr[:, :, ::-1].copy()
            out_fmt = "BGR8"
        else:
            out_fmt = "RGB8"
    return arr.tobytes(), w, h, c, out_fmt


def _dispatch(fn_name: str, params: dict, pil_img: Image.Image) -> Image.Image:
    fn = getattr(F, fn_name, None)
    if fn is None:
        raise ValueError(f"torchvision.transforms.functional has no function: {fn_name!r}")
    return fn(pil_img, **params)


def _main():
    while True:
        header, image_bytes = _read_message()
        if header is None:
            break
        try:
            fn_name = header["fn"]
            params = header.get("params", {})
            pil_img, original_fmt = _bytes_to_pil(header, image_bytes)
            result_img = _dispatch(fn_name, params, pil_img)
            if not isinstance(result_img, Image.Image):
                # Some functions return Tensor — convert back
                import torch  # noqa: PLC0415

                if isinstance(result_img, torch.Tensor):
                    result_img = F.to_pil_image(result_img)
                else:
                    raise TypeError(f"Unexpected return type: {type(result_img)}")
            out_bytes, ow, oh, oc, out_fmt = _pil_to_bytes(result_img, original_fmt)
            _write_response({"ok": True, "w": ow, "h": oh, "c": oc, "fmt": out_fmt}, out_bytes)
        except Exception:  # pylint: disable=broad-exception-caught
            _write_error(traceback.format_exc())


if __name__ == "__main__":
    _main()
