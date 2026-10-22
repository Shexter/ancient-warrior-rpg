"""Keeps a loadout of a KayKit GLB's rigid accessories and skins each to the bone it hangs from,
so raylib (which only animates skinned meshes) moves them with the body. Drops the rest.
--hue LO:HI:TO turns every saturated texture colour with a hue in LO..HI degrees to hue TO.

usage: python3 -I tools/art/prep_model.py in.glb out.glb [--hue LO:HI:TO] Name1 Name2 ...
"""
import io, json, struct, sys
import numpy as np
from PIL import Image


def read_glb(path):
    b = open(path, "rb").read()
    jl = struct.unpack("<I", b[12:16])[0]
    j = json.loads(b[20:20 + jl])
    o = 20 + jl
    bl = struct.unpack("<I", b[o:o + 4])[0]
    return j, bytearray(b[o + 8:o + 8 + bl])


def write_glb(path, j, bin_):
    while len(bin_) % 4:
        bin_ += b"\0"
    js = json.dumps(j, separators=(",", ":")).encode()
    js += b" " * (-len(js) % 4)
    total = 12 + 8 + len(js) + 8 + len(bin_)
    with open(path, "wb") as f:
        f.write(struct.pack("<III", 0x46546C67, 2, total))
        f.write(struct.pack("<II", len(js), 0x4E4F534A) + js)
        f.write(struct.pack("<II", len(bin_), 0x004E4942) + bin_)


def trs(nd):
    if "matrix" in nd:
        return np.array(nd["matrix"], dtype=np.float64).reshape(4, 4).T
    t = nd.get("translation", [0, 0, 0])
    x, y, z, w = nd.get("rotation", [0, 0, 0, 1])
    s = nd.get("scale", [1, 1, 1])
    r = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                  [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                  [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
    m = np.eye(4)
    m[:3, :3] = r * np.array(s)
    m[:3, 3] = t
    return m


def accessor(j, bin_, i):
    a = j["accessors"][i]
    v = j["bufferViews"][a["bufferView"]]
    comps = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[a["type"]]
    assert a["componentType"] == 5126 and v.get("byteStride", 4 * comps) == 4 * comps
    off = v.get("byteOffset", 0) + a.get("byteOffset", 0)
    return np.frombuffer(bin_, np.float32, a["count"] * comps, off).reshape(-1, comps).astype(np.float64)


def append(j, bin_, data, ctype, type_, minmax=False):
    while len(bin_) % 4:
        bin_ += b"\0"
    raw = data.tobytes()
    j["bufferViews"].append({"buffer": 0, "byteOffset": len(bin_), "byteLength": len(raw)})
    bin_ += raw
    a = {"bufferView": len(j["bufferViews"]) - 1, "componentType": ctype, "count": len(data), "type": type_}
    if minmax:
        a["min"], a["max"] = data.min(0).tolist(), data.max(0).tolist()
    j["accessors"].append(a)
    return len(j["accessors"]) - 1


def recolour(j, bin_, lo, hi, to):
    for im in j["images"]:
        v = j["bufferViews"][im["bufferView"]]
        off = v.get("byteOffset", 0)
        hsv = np.array(Image.open(io.BytesIO(bytes(bin_[off:off + v["byteLength"]]))).convert("RGB").convert("HSV"))
        deg = hsv[..., 0].astype(int) * 360 // 256
        hsv[..., 0][(deg >= lo) & (deg <= hi) & (hsv[..., 1] > 90)] = to * 256 // 360
        out = io.BytesIO()
        Image.fromarray(hsv, "HSV").convert("RGB").save(out, "PNG")
        while len(bin_) % 4:
            bin_ += b"\0"
        j["bufferViews"].append({"buffer": 0, "byteOffset": len(bin_), "byteLength": len(out.getvalue())})
        bin_ += out.getvalue()
        im["bufferView"] = len(j["bufferViews"]) - 1


def main():
    args = sys.argv[1:]
    hue = None
    if "--hue" in args:
        i = args.index("--hue")
        hue = [int(x) for x in args[i + 1].split(":")]
        del args[i:i + 2]
    src, dst, keep = args[0], args[1], set(args[2:])
    j, bin_ = read_glb(src)
    if hue:
        recolour(j, bin_, *hue)
    nodes = j["nodes"]
    skin = j["skins"][0]
    ibm = accessor(j, bin_, skin["inverseBindMatrices"]).reshape(-1, 4, 4).transpose(0, 2, 1)
    parent = {c: i for i, nd in enumerate(nodes) for c in nd.get("children", [])}
    world = {}

    def w(i):
        if i not in world:
            world[i] = (w(parent[i]) if i in parent else np.eye(4)) @ trs(nodes[i])
        return world[i]

    found = set()
    for i, nd in enumerate(nodes):
        if "mesh" not in nd or "skin" in nd:
            continue
        if nd["name"] not in keep:
            del nd["mesh"]
            continue
        found.add(nd["name"])
        joint = parent[i]
        k = skin["joints"].index(joint)
        m = np.linalg.inv(ibm[k]) @ np.linalg.inv(w(joint)) @ w(i)  # mesh space -> bind space
        nrm = np.linalg.inv(m[:3, :3]).T
        for prim in j["meshes"][nd["mesh"]]["primitives"]:
            at = prim["attributes"]
            p = accessor(j, bin_, at["POSITION"])
            p = (np.c_[p, np.ones(len(p))] @ m.T)[:, :3]
            at["POSITION"] = append(j, bin_, p.astype(np.float32), 5126, "VEC3", True)
            if "NORMAL" in at:
                n = accessor(j, bin_, at["NORMAL"]) @ nrm.T
                n /= np.linalg.norm(n, axis=1, keepdims=True)
                at["NORMAL"] = append(j, bin_, n.astype(np.float32), 5126, "VEC3")
            ids = np.zeros((len(p), 4), np.uint16)
            ids[:, 0] = k
            wts = np.zeros((len(p), 4), np.float32)
            wts[:, 0] = 1
            at["JOINTS_0"] = append(j, bin_, ids, 5123, "VEC4")
            at["WEIGHTS_0"] = append(j, bin_, wts, 5126, "VEC4")
        # A skinned mesh ignores its node's place: move it to the scene root, untransformed.
        nodes[joint]["children"].remove(i)
        for key in ("translation", "rotation", "scale", "matrix"):
            nd.pop(key, None)
        nd["skin"] = 0
        j["scenes"][0]["nodes"].append(i)
    missing = keep - found
    assert not missing, f"not in {src}: {sorted(missing)}"
    j["buffers"][0]["byteLength"] = len(bin_) + (-len(bin_) % 4)
    write_glb(dst, j, bin_)


main()
