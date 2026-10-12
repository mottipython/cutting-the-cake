#!/usr/bin/env python3
"""Turn the drawings in assets/src (flat magenta backgrounds) into the pictures the
screensaver builds in, under assets/art.

    tools/make_sprites.py

People and things are cut out of their background and saved as PNG with
transparency; the table and the icing are full pictures and are saved as JPEG.
Needs ImageMagick and numpy.
"""
import subprocess
import numpy as np

SRC, OUT = "assets/src", "assets/art"
# name: height in pixels (or ("w", width)); None keeps the scale of the picture named
OBJECTS = {
    "erasmas": 560, "arsibalt": 540, "arsibalt_yay": 540,
    "dath": 440, "dath_hmm": "dath", "dath_yay": 530,
    "spatula": 560, "banner": ("w", 900),
}
BACKGROUNDS = {"table": "1920x1072", "cake": "1024x1024"}

def load(path):
    w, h = map(int, subprocess.check_output(["magick", "identify", "-format", "%w %h", path]).split())
    raw = subprocess.check_output(["magick", path, "-depth", "8", "RGB:-"])
    return np.frombuffer(raw, np.uint8).reshape(h, w, 3).astype(np.float32)

def key(img):
    """Magenta background to alpha, and take its colour cast off the edges."""
    r, g, b = img[..., 0].copy(), img[..., 1].copy(), img[..., 2].copy()
    spill = np.minimum(r, b) - g
    cut = np.clip(spill, 0, None) * 0.92
    r, b = r - cut, b - cut
    alpha = np.clip(1.0 - (spill - 24) / 70.0, 0, 1)
    return np.dstack([r, g, b, alpha * 255]).clip(0, 255).astype(np.uint8)

def crop(a):
    """Trim to the drawing, ignoring stray specks left in the background."""
    solid = a[..., 3] > 128
    rows = np.where(solid.sum(axis=1) > 3)[0]
    cols = np.where(solid.sum(axis=0) > 3)[0]
    return a[rows.min():rows.max() + 1, cols.min():cols.max() + 1]

def save(rgba, w, h, path):
    subprocess.run(["magick", "-size", f"{rgba.shape[1]}x{rgba.shape[0]}", "-depth", "8", "RGBA:-",
                    "-filter", "Lanczos", "-resize", f"{w}x{h}!", "-define", "png:compression-level=9", path],
                   input=rgba.tobytes(), check=True)

def main():
    subprocess.run(["mkdir", "-p", OUT], check=True)
    cut = {n: crop(key(load(f"{SRC}/{n}.jpeg"))) for n in OBJECTS}
    scale = {}
    for n, size in OBJECTS.items():
        a = cut[n]
        if isinstance(size, str): f = scale[size]
        elif isinstance(size, tuple): f = size[1] / a.shape[1]
        else: f = size / a.shape[0]
        scale[n] = f
        w, h = max(1, round(a.shape[1] * f)), max(1, round(a.shape[0] * f))
        save(a, w, h, f"{OUT}/{n}.png")
        print(n, w, h)
    for n, size in BACKGROUNDS.items():
        # the icing is tinted a different colour for each cake, so it is stored nearly white
        extra = ["-fill", "white", "-colorize", "45%"] if n == "cake" else []
        subprocess.run(["magick", f"{SRC}/{n}.jpeg", "-resize", size + "!", *extra, "-quality", "90", f"{OUT}/{n}.jpg"], check=True)
        print(n, size)

if __name__ == "__main__":
    main()
