#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <linux/fb.h>

#include <unistd.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#define VERSTR "FBDraw 1.0"

int main(int argc, char** argv) {
    if (argc != 3 || !*argv[1] || !*argv[2]) {
        puts(VERSTR);
        fprintf(stderr, "%s FRAMEBUFFER COMMANDS\n", argv[0]);
        return 1;
    }
    int fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Failed to open framebuffer %s\n", argv[1]);
        return 1;
    }
    struct fb_var_screeninfo varinfo;
    struct fb_fix_screeninfo fixinfo;
    if (ioctl(fd, FBIOGET_VSCREENINFO, &varinfo) || ioctl(fd, FBIOGET_FSCREENINFO, &fixinfo)) {
        fprintf(stderr, "Failed to get framebuffer info for %s\n", argv[1]);
        return 1;
    }
    void* buf = mmap(NULL, varinfo.yres_virtual * fixinfo.line_length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (buf == MAP_FAILED) {
        fprintf(stderr, "Failed to map framebuffer %s into memory\n", argv[1]);
        return 1;
    }
    unsigned rgbmax[3] = {(1 << varinfo.red.length) - 1, (1 << varinfo.green.length) - 1, (1 << varinfo.blue.length) - 1};
    static uint32_t colorconv[3][256];
    for (uint32_t i = 0; i < 256; ++i) {
        colorconv[0][i] = ((i * rgbmax[0] + 127) / 255) << varinfo.red.offset;
        colorconv[1][i] = ((i * rgbmax[1] + 127) / 255) << varinfo.green.offset;
        colorconv[2][i] = ((i * rgbmax[2] + 127) / 255) << varinfo.blue.offset;
    }
    #define PUTPIX_FAST(PUTPIX_x, PUTPIX_y, PUTPIX_r, PUTPIX_g, PUTPIX_b) do {\
        unsigned PUTPIX_off = (PUTPIX_x) + (PUTPIX_y) * varinfo.xres_virtual;\
        switch ((unsigned char)varinfo.bits_per_pixel) {\
            case 16:\
                ((uint16_t*)buf)[PUTPIX_off] = colorconv[0][(PUTPIX_r)] | colorconv[1][(PUTPIX_g)] | colorconv[2][(PUTPIX_b)];\
                break;\
            case 32:\
                ((uint32_t*)buf)[PUTPIX_off] = colorconv[0][(PUTPIX_r)] | colorconv[1][(PUTPIX_g)] | colorconv[2][(PUTPIX_b)];\
                break;\
        }\
    } while (0)
    #define PUTPIX(PUTPIX_x, PUTPIX_y, PUTPIX_r, PUTPIX_g, PUTPIX_b) do {\
        if ((PUTPIX_x) >= 0 && (PUTPIX_x) < varinfo.xres && (PUTPIX_y) >= 0 && (PUTPIX_y) < varinfo.yres) {\
            PUTPIX_FAST((PUTPIX_x), (PUTPIX_y), (PUTPIX_r), (PUTPIX_g), (PUTPIX_b));\
        }\
    } while (0)
    char* cmd = argv[2];
    int retval = 0;
    bool verbose = false;
    uint8_t color[3] = {255, 255, 255};
    while (1) {
        nextcmd:;
        while (*cmd == ' ' || *cmd == '\n' || *cmd == ';') ++cmd;
        int cmdnamelen = 0;
        while (cmd[cmdnamelen] && cmd[cmdnamelen] != ' ' && cmd[cmdnamelen] != '\n' && cmd[cmdnamelen] != ';') ++cmdnamelen;
        --cmdnamelen;
        #define SKIPTOARGS() do {cmd += cmdnamelen; while (*cmd == ' ') ++cmd;} while(0)
        #define PARSEINT(PARSEINT_out) do {\
            bool PARSEINT_n;\
            if (*cmd == '-') {\
                PARSEINT_n = true;\
                ++cmd;\
            } else if (*cmd == '+') {\
                PARSEINT_n = false;\
                ++cmd;\
            } else {\
                PARSEINT_n = false;\
            }\
            PARSEINT_out = 0;\
            while (*cmd >= '0' && *cmd <= '9') {\
                PARSEINT_out *= 10;\
                PARSEINT_out += *cmd++ - '0';\
            }\
            if (PARSEINT_n) PARSEINT_out *= -1;\
        } while (0)
        #define NEXTARG() do {\
            while (*cmd == ' ') ++cmd;\
            if (*cmd == ',') {\
                ++cmd;\
                while (*cmd == ' ') ++cmd;\
            } else if (!*cmd || *cmd == '\n' || *cmd == ';') {\
                retval = 1;\
                fputs("Expected argument\n", stderr);\
                goto ret;\
            } else {\
                retval = 1;\
                fputs("Expected next argument\n", stderr);\
                goto ret;\
            }\
        } while (0)
        #define NEXTARG_OPT(NEXTARG_has) do {\
            while (*cmd == ' ') ++cmd;\
            if (*cmd == ',') {\
                ++cmd;\
                NEXTARG_has = 1;\
                while (*cmd == ' ') ++cmd;\
            } else if (!*cmd || *cmd == '\n' || *cmd == ';') {\
                NEXTARG_has = 0;\
            } else {\
                retval = 1;\
                fputs("Expected next argument\n", stderr);\
                goto ret;\
            }\
        } while (0)
        #define ENDARGS() do {\
            while (*cmd == ' ') ++cmd;\
            if (*cmd && *cmd != '\n' && *cmd != ';') {\
                retval = 1;\
                fputs("Unexpected character\n", stderr);\
                goto ret;\
            }\
        } while (0)
        switch (*cmd++) {
            case 'c':
                if (!cmdnamelen || (cmdnamelen == 4 && !strncmp(cmd, "olor", 4))) {
                    SKIPTOARGS();
                    char hexcode[6];
                    int hexlen = 0;
                    while (1) {
                        char c = tolower(*cmd);
                        if (c >= '0' && c <= '9') {
                            hexcode[hexlen++] = c;
                            ++cmd;
                        } else if (c >= 'a' && c <= 'f') {
                            hexcode[hexlen++] = c;
                            ++cmd;
                        } else {
                            if (hexlen == 3) {
                                if (hexcode[0] >= '0' && hexcode[0] <= '9') color[0] = hexcode[0] - '0';
                                else color[0] = hexcode[0] - 'a' + 10;
                                color[0] |= color[0] << 4;
                                if (hexcode[1] >= '0' && hexcode[1] <= '9') color[1] = hexcode[1] - '0';
                                else color[1] = hexcode[1] - 'a' + 10;
                                color[1] |= color[1] << 4;
                                if (hexcode[2] >= '0' && hexcode[2] <= '9') color[2] = hexcode[2] - '0';
                                else color[2] = hexcode[2] - 'a' + 10;
                                color[2] |= color[2] << 4;
                            }
                            break;
                        }
                        if (hexlen == 6) {
                            if (hexcode[0] >= '0' && hexcode[0] <= '9') color[0] = (hexcode[0] - '0') << 4;
                            else color[0] = (hexcode[0] - 'a' + 10) << 4;
                            if (hexcode[1] >= '0' && hexcode[1] <= '9') color[0] |= hexcode[1] - '0';
                            else color[0] |= hexcode[1] - 'a' + 10;
                            if (hexcode[2] >= '0' && hexcode[2] <= '9') color[1] = (hexcode[2] - '0') << 4;
                            else color[1] = (hexcode[2] - 'a' + 10) << 4;
                            if (hexcode[3] >= '0' && hexcode[3] <= '9') color[1] |= hexcode[3] - '0';
                            else color[1] |= hexcode[3] - 'a' + 10;
                            if (hexcode[4] >= '0' && hexcode[4] <= '9') color[2] = (hexcode[4] - '0') << 4;
                            else color[2] = (hexcode[4] - 'a' + 10) << 4;
                            if (hexcode[5] >= '0' && hexcode[5] <= '9') color[2] |= hexcode[5] - '0';
                            else color[2] |= hexcode[5] - 'a' + 10;
                            break;
                        }
                    }
                    ENDARGS();
                    if (verbose) printf("SET COLOR: #%02X%02X%02X\n", color[0], color[1], color[2]);
                    goto nextcmd;
                }
                break;
            case 'i':
                if (!cmdnamelen || (cmdnamelen == 3 && !strncmp(cmd, "nfo", 3))) {
                    SKIPTOARGS();
                    ENDARGS();
                    printf(
                        "INFO:\n"
                        "  ID: %s\n"
                        "  Type: %u (%u)\n"
                        "  Line length: %u\n"
                        "  Capabilities: 0x%04x\n"
                        "  Visible resolution: %ux%u\n"
                        "  Virtual resolution: %ux%u\n"
                        "  Offset: %u, %u\n"
                        "  BPP: %u\n"
                        "  Grayscale: %s\n"
                        "  Nonstandard: %s\n"
                        "  Red: off=%u, len=%u, msb_right=%s\n"
                        "  Green: off=%u, len=%u, msb_right=%s\n"
                        "  Blue: off=%u, len=%u, msb_right=%s\n"
                        "  Alpha: off=%u, len=%u, msb_right=%s\n",
                        fixinfo.id,
                        (unsigned)fixinfo.type, (unsigned)fixinfo.type_aux,
                        (unsigned)fixinfo.line_length,
                        (unsigned)fixinfo.capabilities,
                        (unsigned)varinfo.xres, (unsigned)varinfo.yres,
                        (unsigned)varinfo.xres_virtual, (unsigned)varinfo.yres_virtual,
                        (unsigned)varinfo.xoffset, (unsigned)varinfo.yoffset,
                        (unsigned)varinfo.bits_per_pixel,
                        (varinfo.grayscale) ? "Yes" : "No",
                        (varinfo.nonstd) ? "Yes" : "No",
                        (unsigned)varinfo.red.offset, (unsigned)varinfo.red.length, (varinfo.red.msb_right) ? "true" : "false",
                        (unsigned)varinfo.green.offset, (unsigned)varinfo.green.length, (varinfo.green.msb_right) ? "true" : "false",
                        (unsigned)varinfo.blue.offset, (unsigned)varinfo.blue.length, (varinfo.blue.msb_right) ? "true" : "false",
                        (unsigned)varinfo.transp.offset, (unsigned)varinfo.transp.length, (varinfo.transp.msb_right) ? "true" : "false"
                    );
                    goto nextcmd;
                }
                break;
            case 'l':
                if (!cmdnamelen || (cmdnamelen == 3 && !strncmp(cmd, "ine", 3))) {
                    SKIPTOARGS();
                    int x1, y1, x2, y2;
                    PARSEINT(x1);
                    NEXTARG();
                    PARSEINT(y1);
                    NEXTARG();
                    PARSEINT(x2);
                    NEXTARG();
                    PARSEINT(y2);
                    ENDARGS();
                    bool usefast;
                    if (x1 < 0 || x1 >= varinfo.xres) usefast = false;
                    else if (y1 < 0 || y1 >= varinfo.yres) usefast = false;
                    else if (x2 < 0 || x2 >= varinfo.xres) usefast = false;
                    else if (y2 < 0 || y2 >= varinfo.yres) usefast = false;
                    else usefast = true;
                    if (x1 > x2) {int tmp = x1; x1 = x2; x2 = tmp; tmp = y1; y1 = y2; y2 = tmp;}
                    if (usefast) {
                        if (y1 == y2) {
                            for (int x = x1; x <= x2; ++x) {
                                PUTPIX_FAST(x, y1, color[0], color[1], color[2]);
                            }
                        } else if (x1 == x2) {
                            for (int y = y1; y <= y2; ++y) {
                                PUTPIX_FAST(x1, y, color[0], color[1], color[2]);
                            }
                        } else {
                            int dx = x2 - x1;
                            int dy = y2 - y1;
                            int yi;
                            if (dy >= 0) {
                                yi = 1;
                            } else {
                                yi = -1;
                                dy = -dy;
                            }
                            int d = dy * 2 - dx;
                            int y = y1;
                            for (int x = x1; x <= x2; ++x) {
                                PUTPIX_FAST(x, y, color[0], color[1], color[2]);
                                if (d > 0) {
                                    y += yi;
                                    d += (dy - dx) * 2;
                                } else {
                                    d += dy * 2;
                                }
                            }
                        }
                    } else {
                        int dx = x2 - x1;
                        int dy = y2 - y1;
                        int yi;
                        if (dy >= 0) {
                            yi = 1;
                        } else {
                            yi = -1;
                            dy = -dy;
                        }
                        int d = dy * 2 - dx;
                        int y = y1;
                        for (int x = x1; x <= x2; ++x) {
                            PUTPIX(x, y, color[0], color[1], color[2]);
                            if (d > 0) {
                                y += yi;
                                d += (dy - dx) * 2;
                            } else {
                                d += dy * 2;
                            }
                        }
                    }
                    if (verbose) printf("DREW LINE: %d, %d, %d, %d%s\n", x1, y1, x2, y2, (usefast) ? "" : " (bounds checked)");
                    goto nextcmd;
                }
                break;
            case 'p':
                if (!cmdnamelen || (cmdnamelen == 4 && !strncmp(cmd, "ixel", 4))) {
                    SKIPTOARGS();
                    int x, y;
                    PARSEINT(x);
                    NEXTARG();
                    PARSEINT(y);
                    ENDARGS();
                    PUTPIX(x, y, color[0], color[1], color[2]);
                    if (verbose) printf("SET PIXEL: %d, %d\n", x, y);
                    goto nextcmd;
                }
                break;
            case 'r':
                if (!cmdnamelen || (cmdnamelen == 3 && !strncmp(cmd, "ect", 3))) {
                    SKIPTOARGS();
                    int x1, y1, x2, y2;
                    PARSEINT(x1);
                    NEXTARG();
                    PARSEINT(y1);
                    NEXTARG();
                    PARSEINT(x2);
                    NEXTARG();
                    PARSEINT(y2);
                    ENDARGS();
                    if (x1 < 0) x1 = 0;
                    else if (x1 >= varinfo.xres) x1 = varinfo.xres - 1;
                    if (y1 < 0) y1 = 0;
                    else if (y1 >= varinfo.yres) y1 = varinfo.yres - 1;
                    if (x2 < 0) x2 = 0;
                    else if (x2 >= varinfo.xres) x2 = varinfo.xres - 1;
                    if (y2 < 0) y2 = 0;
                    else if (y2 >= varinfo.yres) y2 = varinfo.yres - 1;
                    if (x1 > x2) {int tmp = x1; x1 = x2; x2 = tmp;}
                    if (y1 > y2) {int tmp = y1; y1 = y2; y2 = tmp;}
                    for (int y = y1; y <= y2; ++y) {
                        for (int x = x1; x <= x2; ++x) {
                            PUTPIX_FAST(x, y, color[0], color[1], color[2]);
                        }
                    }
                    if (verbose) printf("DREW RECT: %d, %d, %d, %d\n", x1, y1, x2, y2);
                    goto nextcmd;
                }
                break;
            case 'v':
                if (!cmdnamelen || (cmdnamelen == 6 && !strncmp(cmd, "ersion", 6))) {
                    SKIPTOARGS();
                    ENDARGS();
                    puts(VERSTR);
                    goto nextcmd;
                } else if (cmdnamelen == 6 && !strncmp(cmd, "erbose", 6)) {
                    SKIPTOARGS();
                    ENDARGS();
                    verbose = !verbose;
                    goto nextcmd;
                }
                break;
            case 0:
                goto ret;
        }
        --cmd;
        ++cmdnamelen;
        fprintf(stderr, "Invalid command: %.*s\n", cmdnamelen, cmd);
        retval = 1;
        break;
    }
    ret:;
    munmap(buf, varinfo.yres_virtual * fixinfo.line_length);
    close(fd);
    return retval;
}
