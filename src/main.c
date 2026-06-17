/*
 * wavezbg - PSP VSH Wave custom colour plugin
 * Copyright (c) 2026 koutsie
 * https://kouts.the-sauna.icu/
 * https://koutsie.github.io/wavezbg/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "default_wave.h"
#include <pspdisplay.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <pspmodulemgr.h>
#include <string.h>

PSP_MODULE_INFO("wavezbg", 0x1000, 1, 0);
PSP_MAIN_THREAD_ATTR(0);

typedef struct {
    unsigned int c1;
    unsigned int c2;
    unsigned int c3;
    int num_colours;
} MonthColour;

MonthColour month_colours[35];
char base_path[6] = "ms0:/";

static inline int hex2int(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return 0;
}

unsigned int parse_hex_colour(const char* buf)
{
    int r = (hex2int(buf[1]) << 4) | hex2int(buf[2]);
    int g = (hex2int(buf[3]) << 4) | hex2int(buf[4]);
    int b = (hex2int(buf[5]) << 4) | hex2int(buf[6]);
    return (0xFF << 24) | (b << 16) | (g << 8) | r;
}

int parse_config_buffer(const char* buf)
{
    // im sure there's a better way to do this
    // honestly just got inspired by Bagieta doing
    // PAF stuff so, enjoy - or dont.
    int valid = 0;
    const char* p = buf;
    while (*p) {
        if (*p == '#' || *p == '\r' || *p == '\n' || *p == ' ') {
            p++;
            continue;
        }

        int index = 0;
        while (*p >= '0' && *p <= '9') {
            index = index * 10 + (*p - '0');
            p++;
        }
        while (*p == ' ' || *p == '=')
            p++;

        if (index >= 1 && index <= 35 && *p == '#') {
            month_colours[index - 1].c1 = parse_hex_colour(p);
            month_colours[index - 1].num_colours = 1;
            valid++;
            p += 7;
            while (*p == ' ')
                p++;
            if (*p == '#') {
                month_colours[index - 1].c2 = parse_hex_colour(p);
                month_colours[index - 1].num_colours = 2;
                p += 7;
                while (*p == ' ')
                    p++;
                if (*p == '#') {
                    month_colours[index - 1].c3 = parse_hex_colour(p);
                    month_colours[index - 1].num_colours = 3;
                    p += 7;
                }
            }
        }
        while (*p && *p != '\n')
            p++;
    }
    return valid;
}

void read_config()
{
    // defaults
    for (int i = 0; i < 35; i++) {
        month_colours[i].c1 = 0xFF0099FF;
        month_colours[i].num_colours = 1;
    }
    parse_config_buffer(default_wave_txt);

    SceUID fd = sceIoOpen("ef0:/seplugins/wave.txt", PSP_O_RDONLY, 0777);
    if (fd < 0)
        fd = sceIoOpen("ef0:/wave.txt", PSP_O_RDONLY, 0777);
    if (fd >= 0) {
        base_path[0] = 'e';
        base_path[1] = 'f';
    } else {
        fd = sceIoOpen("ms0:/seplugins/wave.txt", PSP_O_RDONLY, 0777);
        if (fd < 0)
            fd = sceIoOpen("ms0:/wave.txt", PSP_O_RDONLY, 0777);
        base_path[0] = 'm';
        base_path[1] = 's';
    }

    if (fd < 0) {
        SceUID fd_out = sceIoOpen("ef0:/seplugins/wave.txt", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
        if (fd_out >= 0) {
            sceIoWrite(fd_out, default_wave_txt, sizeof(default_wave_txt) - 1);
            sceIoClose(fd_out);
            base_path[0] = 'e';
            base_path[1] = 'f';
        } else {
            sceIoMkdir("ms0:/seplugins", 0777);
            fd_out = sceIoOpen("ms0:/seplugins/wave.txt", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
            if (fd_out >= 0) {
                sceIoWrite(fd_out, default_wave_txt, sizeof(default_wave_txt) - 1);
                sceIoClose(fd_out);
            }
            base_path[0] = 'm';
            base_path[1] = 's';
        }
    } else {
        char buf[2048];
        int bytes = sceIoRead(fd, buf, sizeof(buf) - 1);
        sceIoClose(fd);
        if (bytes > 0) {
            buf[bytes] = '\0';
            if (parse_config_buffer(buf) == 0) {
                char err_path[64];
                strcpy(err_path, base_path);
                strcat(err_path, "wavez_ERROR.txt");
                SceUID fd_err = sceIoOpen(err_path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
                if (fd_err >= 0) {
                    // PEBCAK
                    char err_msg[] = "ERROR: wave.txt is empty or improperly formatted...";
                    sceIoWrite(fd_err, err_msg, sizeof(err_msg) - 1);
                    sceIoClose(fd_err);
                }
            }
        }
    }
}

void write_bmp(const char* filename, int start_month, int count)
{
    char bmp_path[64];
    strcpy(bmp_path, base_path);
    strcat(bmp_path, "wavez_cache/");
    strcat(bmp_path, filename);

    SceUID fd = sceIoOpen(bmp_path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0)
        return;

    for (int i = 0; i < count; i++) {
        int m = start_month + i;
        if (m >= 35)
            m = 34;

        unsigned char bmp_data[6176];
        memset(bmp_data, 0, sizeof(bmp_data));

        bmp_data[0] = 'B';
        bmp_data[1] = 'M';
        *((unsigned int*)&bmp_data[2]) = 6176;
        *((unsigned int*)&bmp_data[10]) = 54;
        *((unsigned int*)&bmp_data[14]) = 40;
        *((unsigned int*)&bmp_data[18]) = 60;
        *((unsigned int*)&bmp_data[22]) = 34;
        *((unsigned short*)&bmp_data[26]) = 1;
        *((unsigned short*)&bmp_data[28]) = 24;
        *((unsigned int*)&bmp_data[34]) = 6120;

        int r1 = month_colours[m].c1 & 0xFF;
        int g1 = (month_colours[m].c1 >> 8) & 0xFF;
        int b1 = (month_colours[m].c1 >> 16) & 0xFF;

        int r2 = r1, g2 = g1, b2 = b1;
        int r3 = r1, g3 = g1, b3 = b1;

        if (month_colours[m].num_colours >= 2) {
            r2 = month_colours[m].c2 & 0xFF;
            g2 = (month_colours[m].c2 >> 8) & 0xFF;
            b2 = (month_colours[m].c2 >> 16) & 0xFF;
        }
        if (month_colours[m].num_colours == 3) {
            r3 = month_colours[m].c3 & 0xFF;
            g3 = (month_colours[m].c3 >> 8) & 0xFF;
            b3 = (month_colours[m].c3 >> 16) & 0xFF;
        }

        int offset = 54;
        for (int y = 0; y < 34; y++) {
            int cur_r, cur_g, cur_b;
            if (month_colours[m].num_colours == 3) {
                if (y < 17) {
                    cur_r = r1 + (y * (r2 - r1)) / 16;
                    cur_g = g1 + (y * (g2 - g1)) / 16;
                    cur_b = b1 + (y * (b2 - b1)) / 16;
                } else {
                    cur_r = r2 + ((y - 17) * (r3 - r2)) / 16;
                    cur_g = g2 + ((y - 17) * (g3 - g2)) / 16;
                    cur_b = b2 + ((y - 17) * (b3 - b2)) / 16;
                }
            } else {
                cur_r = r1 + (y * (r2 - r1)) / 33;
                cur_g = g1 + (y * (g2 - g1)) / 33;
                cur_b = b1 + (y * (b2 - b1)) / 33;
            }

            for (int x = 0; x < 60; x++) {
                bmp_data[offset++] = cur_b;
                bmp_data[offset++] = cur_g;
                bmp_data[offset++] = cur_r;
            }
        }

        sceIoWrite(fd, bmp_data, 6176);
    }
    sceIoClose(fd);
}

void generate_wave_bmp()
{
    // pregen so we aint slow
    char cache_path[32];
    strcpy(cache_path, base_path);
    strcat(cache_path, "wavez_cache");
    sceIoMkdir(cache_path, 0777);

    char check_path[64];
    strcpy(check_path, cache_path);
    strcat(check_path, "/w1.bmp");
    SceUID fd = sceIoOpen(check_path, PSP_O_RDONLY, 0777);
    if (fd >= 0) {
        sceIoClose(fd);
        return;
    }

    write_bmp("w1.bmp", 0, 12);
    write_bmp("w2.bmp", 12, 22);
}

int patch_wave_strings()
{
    // only for 6.61
    // patches taken to make this better tbh
    int patched = 0;

    char replace1[36];
    memset(replace1, 0, sizeof(replace1));
    strcpy(replace1, base_path);
    strcat(replace1, "wavez_cache/w1.bmp");

    char replace2[36];
    memset(replace2, 0, sizeof(replace2));
    strcpy(replace2, base_path);
    strcat(replace2, "wavez_cache/w2.bmp");

    SceUID ids[100];
    int count = 0;
    if (sceKernelGetModuleIdList(ids, sizeof(ids), &count) >= 0) {
        for (int i = 0; i < count; i++) {
            SceKernelModuleInfo info;
            memset(&info, 0, sizeof(info));
            info.size = sizeof(info);

            if (sceKernelQueryModuleInfo(ids[i], &info) >= 0) {
                if (strstr(info.name, "system_plugin_bg") != NULL || strstr(info.name, "sysconf_plugin") != NULL || strstr(info.name, "vsh") != NULL) {

                    char* addr = (char*)info.text_addr;
                    char* end_addr = addr + info.text_size + info.data_size;

                    while (addr < end_addr - 34) {
                        if (addr[0] == 'f' && addr[1] == 'l' && addr[2] == 'a' && addr[3] == 's' && addr[4] == 'h') {
                            if (strncmp(addr, "flash0:/vsh/resource/01-12.bmp", 30) == 0) {
                                memcpy(addr, replace1, 30);
                                sceKernelDcacheWritebackInvalidateRange(addr, 30);
                                patched = 1;
                            } else if (strncmp(addr, "flash0:/vsh/resource/01-12_03g.bmp", 34) == 0) {
                                memcpy(addr, replace1, 34);
                                sceKernelDcacheWritebackInvalidateRange(addr, 34);
                                patched = 1;
                            } else if (strncmp(addr, "flash0:/vsh/resource/13-27.bmp", 30) == 0) {
                                memcpy(addr, replace2, 30);
                                sceKernelDcacheWritebackInvalidateRange(addr, 30);
                                patched = 1;
                            }
                        }
                        addr++;
                    }
                }
            }
        }
    }

    return patched;
}

int main_thread(SceSize args, void* argp)
{
    (void)args;
    (void)argp;

    read_config();
    generate_wave_bmp();

    while (1) {
        if (patch_wave_strings()) {
            sceKernelExitDeleteThread(0);
            return 0;
        }
        sceKernelDelayThread(100000);
    }

    return 0;
}

int module_start(SceSize args, void* argp)
{
    if (sceKernelDevkitVersion() != 0x06060110) {
        return 1;
    }
    SceUID thid = sceKernelCreateThread("wavezbg_thread", main_thread, 0x18, 0x10000, 0, NULL);
    if (thid >= 0) {
        sceKernelStartThread(thid, args, argp);
    }
    return 0;
}

int module_stop(void)
{
    return 0;
}
