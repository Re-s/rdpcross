#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 6) { fprintf(stderr, "usage: %s display x y w h out.ppm\n", argv[0]); return 1; }
    Display *d = XOpenDisplay(argv[1]);
    if (!d) { fprintf(stderr, "no display\n"); return 1; }
    int x = atoi(argv[2]), y = atoi(argv[3]), w = atoi(argv[4]), h = atoi(argv[5]);
    XWindowAttributes attr;
    XGetWindowAttributes(d, RootWindow(d, DefaultScreen(d)), &attr);
    if (x + w > attr.width) w = attr.width - x;
    if (y + h > attr.height) h = attr.height - y;
    if (w <= 0 || h <= 0) { fprintf(stderr, "bad region\n"); return 1; }
    XImage *img = XGetImage(d, RootWindow(d, DefaultScreen(d)), x, y, w, h, AllPlanes, ZPixmap);
    if (!img) { fprintf(stderr, "XGetImage failed\n"); return 1; }
    FILE *f = fopen(argv[6], "wb");
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    unsigned char *row = malloc(w * 3);
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            unsigned long px = XGetPixel(img, i, j);
            row[i*3+0] = (px >> 16) & 0xff;
            row[i*3+1] = (px >> 8) & 0xff;
            row[i*3+2] = px & 0xff;
        }
        fwrite(row, 1, w * 3, f);
    }
    fclose(f);
    printf("wrote %s %dx%d\n", argv[6], w, h);
    return 0;
}
