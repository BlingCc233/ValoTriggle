#include "helper.h"
#include <math.h>
#include <stdbool.h>
#include <windows.h>

#define PI 3.14159265358979323846
#define deg2rad(deg) ((deg) * PI / 180.0)
#define rad2deg(rad) ((rad) * 180.0 / PI)

LARGE_INTEGER frequency, start, end;
int reaction_count = 0;
double total_reaction = 0.0;

typedef struct {
    char key_name[256];
    int key;
} SCAN_CODES;

void disable_quickedit() {
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    DWORD prevMode;

    GetConsoleMode(hInput, &prevMode);
    SetConsoleMode(hInput, prevMode & ~ENABLE_QUICK_EDIT_MODE);
}

void free_config(CONFIG* pCfg) {
    free(pCfg->hold_key);
    free(pCfg->target_color);
}

bool get_config(CONFIG* pCfg) {
    pCfg->target_color = get_str("target_color");
    pCfg->hold_key = get_str("hold_key");
    pCfg->scan_area_x = get_int("scan_area_x");
    pCfg->scan_area_y = get_int("scan_area_y");
    pCfg->color_sens = get_int("color_sens");
    pCfg->tap_time = get_float("tap_time");;
    pCfg->hold_mode = get_int("hold_mode");
    pCfg->key_up_rec_time = get_int("key_up_rec_time");


    if (pCfg->scan_area_x == -1 ||
        pCfg->scan_area_y == -1 ||
        pCfg->color_sens == -1 ||
        pCfg->tap_time == -1 ||
        pCfg->hold_key == 0 ||
        pCfg->hold_mode == -1 ||
        pCfg->target_color == 0)
    {
        return false;
    }

    return true;
}


unsigned int* get_screenshot(const char* save_name, int crop_width, int crop_height) {
    unsigned int* pixels = 0;
    HDC mem_dc = 0;
    FILE* file = 0;

    HDC screen_dc = GetDC(0);
    HBITMAP bitmap = 0;
    BITMAP bmp = { 0 };
    BITMAPFILEHEADER bmp_file_header = { 0 };

    if (screen_dc == 0) {
        printf("ERROR: GetDC() failed!");
        return 0;
    }

    int crop_x = 0;
    int crop_y = 0;

    int screen_width = GetDeviceCaps(screen_dc, DESKTOPHORZRES);
    int screen_height = GetDeviceCaps(screen_dc, DESKTOPVERTRES);

    /* Calculate coordinates for center crop */
    crop_x = (screen_width - crop_width) / 2;
    crop_y = (screen_height - crop_height) / 2;

    /* Create memory DC and compatible bitmap */
    mem_dc = CreateCompatibleDC(screen_dc);

    if (mem_dc == 0) {
        printf("ERROR: CreateCompatibleDC() failed!");
        ReleaseDC(0, screen_dc);
        return 0;
    }

    bitmap = CreateCompatibleBitmap(screen_dc, crop_width, crop_height);

    if (bitmap == 0) {
        printf("ERROR: CreateCompatibleBitmap() failed.");
        ReleaseDC(0, screen_dc);
        DeleteDC(mem_dc);
        return 0;
    }

    SelectObject(mem_dc, bitmap);

    /* Copy cropped screen contents to memory DC */
    if (BitBlt(mem_dc, 0, 0, crop_width, crop_height, screen_dc, crop_x, crop_y, SRCCOPY) == 0) {
        printf("ERROR: BitBlt() failed!");
        ReleaseDC(0, screen_dc);
        DeleteDC(mem_dc);
        DeleteObject(bitmap);
        return 0;
    }

    /* Prepare bitmap header */
    GetObject(bitmap, sizeof(BITMAP), &bmp);

    /* Create bitmap file header */
    bmp_file_header.bfType = 0x4D42; /* BM */
    bmp_file_header.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) +
        bmp.bmWidthBytes * crop_height;
    bmp_file_header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    /* Create bitmap info header */
    BITMAPINFOHEADER bmp_info_header;
    memset(&bmp_info_header, 0, sizeof(BITMAPINFOHEADER));
    bmp_info_header.biSize = sizeof(BITMAPINFOHEADER);
    bmp_info_header.biWidth = bmp.bmWidth;
    bmp_info_header.biHeight = bmp.bmHeight;
    bmp_info_header.biPlanes = 1;
    bmp_info_header.biBitCount = 32; /* Use 32-bit color */
    bmp_info_header.biCompression = BI_RGB;
    bmp_info_header.biSizeImage = bmp.bmWidthBytes * bmp.bmHeight;

    if (save_name) {
        /* Save bitmap to file */
        file = fopen(save_name, "wb");

        if (!file) {
            printf("ERROR: fopen() failed!");
            ReleaseDC(0, screen_dc);
            DeleteDC(mem_dc);
            DeleteObject(bitmap);
            return 0;
        }

        fwrite(&bmp_file_header, sizeof(BITMAPFILEHEADER), 1, file);
        fwrite(&bmp_info_header, sizeof(BITMAPINFOHEADER), 1, file);
    }

    /* Allocate buffer for bitmap data */
    pixels = (unsigned int*)malloc(bmp.bmWidthBytes * crop_height);

    if (!pixels) {
        printf("ERROR: malloc() failed!");
        ReleaseDC(0, screen_dc);
        DeleteDC(mem_dc);
        DeleteObject(bitmap);
        return 0;
    }

    /* Get bitmap data */
    if (GetDIBits(mem_dc, bitmap, 0, crop_height, pixels, (BITMAPINFO*)&bmp_info_header, DIB_RGB_COLORS) == 0) {
        printf("ERROR: GetDIBits() failed!");
        ReleaseDC(0, screen_dc);
        DeleteDC(mem_dc);
        DeleteObject(bitmap);
        free(pixels);
        return 0;
    }

    if (save_name) {
        fwrite(pixels, bmp.bmWidthBytes * crop_height, 1, file);
        fclose(file);
    }

    ReleaseDC(0, screen_dc);
    DeleteDC(mem_dc);
    DeleteObject(bitmap);

    return pixels;
}


bool is_color_found(DWORD* pPixels, int pixel_count, int red, int green, int blue, int color_sens) {
    for (int i = 0; i < pixel_count; i++) {
        DWORD pixelColor = pPixels[i];

        int r = GetRed(pixelColor);
        int g = GetGreen(pixelColor);
        int b = GetBlue(pixelColor);

        if (r + color_sens >= red && r - color_sens <= red) {
            if (g + color_sens >= green && g - color_sens <= green) {
                if (b + color_sens >= blue && b - color_sens <= blue) {
                    return true;
                }
            }
        }
    }
    return false;
}

typedef struct {
    double h, s, v;
} HSV;

HSV rgb_to_hsv(int r, int g, int b) {
    HSV hsv;
    double r_p = r / 255.0;
    double g_p = g / 255.0;
    double b_p = b / 255.0;

    double c_max = fmax(r_p, fmax(g_p, b_p));
    double c_min = fmin(r_p, fmin(g_p, b_p));
    double delta = c_max - c_min;

    // Calculate H
    if (delta == 0) {
        hsv.h = 0;
    }
    else if (c_max == r_p) {
        hsv.h = 60 * fmod(((g_p - b_p) / delta), 6);
    }
    else if (c_max == g_p) {
        hsv.h = 60 * (((b_p - r_p) / delta) + 2);
    }
    else {
        hsv.h = 60 * (((r_p - g_p) / delta) + 4);
    }

    if (hsv.h < 0) hsv.h += 360;

    // Calculate S
    hsv.s = (c_max == 0) ? 0 : (delta / c_max);

    // Calculate V
    hsv.v = c_max;

    return hsv;
}

bool is_color_found_HSV(DWORD* pPixels, int pixel_count, int red, int green, int blue, double tolerance) {
    HSV target = rgb_to_hsv(red, green, blue);

    for (int i = 0; i < pixel_count; i++) {
        DWORD pixelColor = pPixels[i];
        int r = GetRed(pixelColor);
        int g = GetGreen(pixelColor);
        int b = GetBlue(pixelColor);

        HSV current = rgb_to_hsv(r, g, b);

        // 计算HSV空间中的距离
        double h_diff = fmin(fabs(current.h - target.h), 360 - fabs(current.h - target.h)) / 180.0;
        double s_diff = fabs(current.s - target.s);
        double v_diff = fabs(current.v - target.v);

        // 综合距离评分（可以调整权重）
        double distance = sqrt(h_diff * h_diff + s_diff * s_diff + v_diff * v_diff);

        if (distance < tolerance) {
            return true;
        }
    }
    return false;
}

// Delta E 2000
/*
 * threshold: 2 ~ 6
 */

typedef struct {
    double l, a, b;
} Lab;

typedef struct {
    double x, y, z;
} XYZ;

// sRGB到XYZ的转换矩阵
const double SRGB_TO_XYZ[3][3] = {
        {0.4124564, 0.3575761, 0.1804375},
        {0.2126729, 0.7151522, 0.0721750},
        {0.0193339, 0.1191920, 0.9503041}
};

// 标准白点 D65
const double WHITE_X = 95.047;
const double WHITE_Y = 100.000;
const double WHITE_Z = 108.883;

// Gamma校正
double gamma_correction(double value) {
    if (value <= 0.04045) {
        return value / 12.92;
    }
    return pow((value + 0.055) / 1.055, 2.4);
}

// XYZ到Lab转换辅助函数
double xyz_to_lab_helper(double value) {
    if (value > 0.008856) {
        return pow(value, 1.0/3.0);
    }
    return (7.787 * value) + (16.0 / 116.0);
}

// RGB到XYZ的转换
XYZ rgb_to_xyz(int r, int g, int b) {
    double sr = gamma_correction(r / 255.0);
    double sg = gamma_correction(g / 255.0);
    double sb = gamma_correction(b / 255.0);

    XYZ xyz;
    xyz.x = (SRGB_TO_XYZ[0][0] * sr + SRGB_TO_XYZ[0][1] * sg + SRGB_TO_XYZ[0][2] * sb) * WHITE_X;
    xyz.y = (SRGB_TO_XYZ[1][0] * sr + SRGB_TO_XYZ[1][1] * sg + SRGB_TO_XYZ[1][2] * sb) * WHITE_Y;
    xyz.z = (SRGB_TO_XYZ[2][0] * sr + SRGB_TO_XYZ[2][1] * sg + SRGB_TO_XYZ[2][2] * sb) * WHITE_Z;

    return xyz;
}

// XYZ到Lab的转换
Lab xyz_to_lab(XYZ xyz) {
    double x = xyz.x / WHITE_X;
    double y = xyz.y / WHITE_Y;
    double z = xyz.z / WHITE_Z;

    x = xyz_to_lab_helper(x);
    y = xyz_to_lab_helper(y);
    z = xyz_to_lab_helper(z);

    Lab lab;
    lab.l = (116.0 * y) - 16.0;
    lab.a = 500.0 * (x - y);
    lab.b = 200.0 * (y - z);

    return lab;
}

// RGB到Lab的直接转换
Lab rgb_to_lab(int r, int g, int b) {
    XYZ xyz = rgb_to_xyz(r, g, b);
    return xyz_to_lab(xyz);
}

// Delta E 2000色差计算
double delta_e_2000(Lab lab1, Lab lab2) {
    // 计算C'
    double c1 = sqrt(lab1.a * lab1.a + lab1.b * lab1.b);
    double c2 = sqrt(lab2.a * lab2.a + lab2.b * lab2.b);
    double c_avg = (c1 + c2) / 2.0;

    // 计算G
    double c_avg_pow7 = pow(c_avg, 7);
    double g = 0.5 * (1.0 - sqrt(c_avg_pow7 / (c_avg_pow7 + pow(25.0, 7))));

    // 计算a'
    double a1_prime = lab1.a * (1.0 + g);
    double a2_prime = lab2.a * (1.0 + g);

    // 计算C'
    double c1_prime = sqrt(a1_prime * a1_prime + lab1.b * lab1.b);
    double c2_prime = sqrt(a2_prime * a2_prime + lab2.b * lab2.b);

    // 计算h'
    double h1_prime = rad2deg(atan2(lab1.b, a1_prime));
    if (h1_prime < 0) h1_prime += 360;
    double h2_prime = rad2deg(atan2(lab2.b, a2_prime));
    if (h2_prime < 0) h2_prime += 360;

    // 计算ΔL', ΔC', ΔH'
    double delta_l_prime = lab2.l - lab1.l;
    double delta_c_prime = c2_prime - c1_prime;

    double delta_h_prime;
    if (c1_prime * c2_prime == 0) {
        delta_h_prime = 0;
    } else {
        if (fabs(h2_prime - h1_prime) <= 180) {
            delta_h_prime = h2_prime - h1_prime;
        } else if (h2_prime - h1_prime > 180) {
            delta_h_prime = h2_prime - h1_prime - 360;
        } else {
            delta_h_prime = h2_prime - h1_prime + 360;
        }
    }
    double delta_H_prime = 2.0 * sqrt(c1_prime * c2_prime) * sin(deg2rad(delta_h_prime / 2.0));

    // 计算L', C', H'的加权平均值
    double l_prime_avg = (lab1.l + lab2.l) / 2.0;
    double c_prime_avg = (c1_prime + c2_prime) / 2.0;

    double h_prime_avg;
    if (c1_prime * c2_prime == 0) {
        h_prime_avg = h1_prime + h2_prime;
    } else {
        if (fabs(h1_prime - h2_prime) <= 180) {
            h_prime_avg = (h1_prime + h2_prime) / 2.0;
        } else if (h1_prime + h2_prime < 360) {
            h_prime_avg = (h1_prime + h2_prime + 360) / 2.0;
        } else {
            h_prime_avg = (h1_prime + h2_prime - 360) / 2.0;
        }
    }

    // 计算T
    double t = 1.0 - 0.17 * cos(deg2rad(h_prime_avg - 30)) +
               0.24 * cos(deg2rad(2.0 * h_prime_avg)) +
               0.32 * cos(deg2rad(3.0 * h_prime_avg + 6.0)) -
               0.20 * cos(deg2rad(4.0 * h_prime_avg - 63));

    // 计算RT
    double delta_theta = 30 * exp(-pow((h_prime_avg - 275) / 25, 2));
    double rc = 2.0 * sqrt(pow(c_prime_avg, 7) / (pow(c_prime_avg, 7) + pow(25.0, 7)));
    double rt = -sin(deg2rad(2.0 * delta_theta)) * rc;

    // 计算补偿系数
    double sl = 1.0 + ((0.015 * pow(l_prime_avg - 50, 2)) /
                       sqrt(20 + pow(l_prime_avg - 50, 2)));
    double sc = 1.0 + 0.045 * c_prime_avg;
    double sh = 1.0 + 0.015 * c_prime_avg * t;

    // 计算最终的色差值
    double delta_l = delta_l_prime / sl;
    double delta_c = delta_c_prime / sc;
    double delta_h = delta_H_prime / sh;

    return sqrt(pow(delta_l, 2) + pow(delta_c, 2) + pow(delta_h, 2) +
                rt * delta_c * delta_h);
}

// 颜色检测函数
bool is_color_found_DE_single(DWORD* pPixels, int pixel_count, int red, int green, int blue, double threshold) {
    // 计算原始方阵的边长
    int side_length = (int)sqrt(pixel_count);

    // 计算中心6x6区域的边界
    int center_start = side_length/2 - 3;
    int center_end = center_start + 6;

    const int WHITE_THRESHOLD = 240;
    const double MAX_WHITE_RATIO = 0.6;
    const double TAR_RATIO = 0.1;

    int white_pixel_count = 0;
    int valid_pixel_count = 0;

    Lab target = rgb_to_lab(red, green, blue);

    // 先统计白色像素
    for(int row = 0; row < side_length; row++) {
        for(int col = 0; col < side_length; col++) {
            // 跳过中心6x6区域
            if(row >= center_start && row < center_end &&
               col >= center_start && col < center_end) {
                continue;
            }

            valid_pixel_count++;
            int i = row * side_length + col;
            DWORD pixelColor = pPixels[i];
            int r = GetRValue(pixelColor);
            int g = GetGValue(pixelColor);
            int b = GetBValue(pixelColor);

            if(r > WHITE_THRESHOLD &&
               g > WHITE_THRESHOLD &&
               b > WHITE_THRESHOLD) {
                white_pixel_count++;
            }
        }
    }

    // 检查白色像素比例
    double white_ratio = (double)white_pixel_count / valid_pixel_count;
    if(white_ratio > MAX_WHITE_RATIO) {
        return false;
    }

    int target_count = 0;

    // 再检查颜色匹配
    for(int row = 0; row < side_length; row++) {
        for(int col = 0; col < side_length; col++) {
            // 跳过中心6x6区域
            if(row >= center_start && row < center_end &&
               col >= center_start && col < center_end) {
                continue;
            }

            int i = row * side_length + col;
            DWORD pixelColor = pPixels[i];
            int r = GetRValue(pixelColor);
            int g = GetGValue(pixelColor);
            int b = GetBValue(pixelColor);

            Lab current = rgb_to_lab(r, g, b);
            double difference = delta_e_2000(target, current);

            if (difference < threshold) {
                target_count++;
            }
        }
    }

    double tar_ratio = (double)target_count / valid_pixel_count;
    if(tar_ratio >= TAR_RATIO) {
        return true;
    }
    return false;
}


bool is_color_found_DE(DWORD* pPixels, int pixel_count, int red, int green, int blue, double threshold) {
    // 计算原始方阵的边长
    int side_length = (int)sqrt(pixel_count);

    double total_deltaE = 0.0;
    int white_pixel_count = 0;
    int valid_pixel_count = 0; // 用于统计参与计算的有效像素数
    const int WHITE_THRESHOLD = 240;
    const double MAX_WHITE_RATIO = 0.6;

    // 计算中心6x6区域的边界
    int center_start = side_length/2 - 3;
    int center_end = center_start + 6;

    // 预先计算目标颜色的Lab值
    double r = red / 255.0;
    double g = green / 255.0;
    double b = blue / 255.0;

    r = (r > 0.04045) ? pow((r + 0.055) / 1.055, 2.4) : r / 12.92;
    g = (g > 0.04045) ? pow((g + 0.055) / 1.055, 2.4) : g / 12.92;
    b = (b > 0.04045) ? pow((b + 0.055) / 1.055, 2.4) : b / 12.92;

    r *= 100.0;
    g *= 100.0;
    b *= 100.0;

    double X = r * 0.4124564 + g * 0.3575761 + b * 0.1804375;
    double Y = r * 0.2126729 + g * 0.7151522 + b * 0.0721750;
    double Z = r * 0.0193339 + g * 0.1191920 + b * 0.9503041;

    X /= 95.047;
    Y /= 100.000;
    Z /= 108.883;

    X = (X > 0.008856) ? pow(X, 1.0/3.0) : (7.787 * X + 16.0/116.0);
    Y = (Y > 0.008856) ? pow(Y, 1.0/3.0) : (7.787 * Y + 16.0/116.0);
    Z = (Z > 0.008856) ? pow(Z, 1.0/3.0) : (7.787 * Z + 16.0/116.0);

    double L2 = (116.0 * Y) - 16.0;
    double a2 = 500.0 * (X - Y);
    double b2 = 200.0 * (Y - Z);

    // 遍历像素
    for(int row = 0; row < side_length; row++) {
        for(int col = 0; col < side_length; col++) {
            // 跳过中心6x6区域
            if(row >= center_start && row < center_end &&
               col >= center_start && col < center_end) {
                continue;
            }

            int i = row * side_length + col;
            valid_pixel_count++;

            int curr_blue = pPixels[i] & 0xFF;
            int curr_green = (pPixels[i] >> 8) & 0xFF;
            int curr_red = (pPixels[i] >> 16) & 0xFF;

            // 检查是否接近白色
            if(curr_red > WHITE_THRESHOLD &&
               curr_green > WHITE_THRESHOLD &&
               curr_blue > WHITE_THRESHOLD) {
                white_pixel_count++;
            }

            // RGB到XYZ
            r = curr_red / 255.0;
            g = curr_green / 255.0;
            b = curr_blue / 255.0;

            r = (r > 0.04045) ? pow((r + 0.055) / 1.055, 2.4) : r / 12.92;
            g = (g > 0.04045) ? pow((g + 0.055) / 1.055, 2.4) : g / 12.92;
            b = (b > 0.04045) ? pow((b + 0.055) / 1.055, 2.4) : b / 12.92;

            r *= 100.0;
            g *= 100.0;
            b *= 100.0;

            X = r * 0.4124564 + g * 0.3575761 + b * 0.1804375;
            Y = r * 0.2126729 + g * 0.7151522 + b * 0.0721750;
            Z = r * 0.0193339 + g * 0.1191920 + b * 0.9503041;

            X /= 95.047;
            Y /= 100.000;
            Z /= 108.883;

            X = (X > 0.008856) ? pow(X, 1.0/3.0) : (7.787 * X + 16.0/116.0);
            Y = (Y > 0.008856) ? pow(Y, 1.0/3.0) : (7.787 * Y + 16.0/116.0);
            Z = (Z > 0.008856) ? pow(Z, 1.0/3.0) : (7.787 * Z + 16.0/116.0);

            double L1 = (116.0 * Y) - 16.0;
            double a1 = 500.0 * (X - Y);
            double b1 = 200.0 * (Y - Z);

            double deltaL = L1 - L2;
            double deltaA = a1 - a2;
            double deltaB = b1 - b2;

            total_deltaE += sqrt(deltaL*deltaL + deltaA*deltaA + deltaB*deltaB);
        }
    }

    // 检查白色像素比例
    double white_ratio = (double)white_pixel_count / valid_pixel_count;
    if(white_ratio > MAX_WHITE_RATIO) {
        return false;
    }

    // 计算非白色像素的平均色差
    double avg_deltaE = total_deltaE / (valid_pixel_count);
    return avg_deltaE < threshold;
}

int get_key_code(char* input_key) {

    SCAN_CODES keys[] =
    {
        {"left_mouse_button", 0x01},
        {"right_mouse_button", 0x02},
        {"x1", 0x05},
        {"x2", 0x06},
        {"num_0", 0x30},
        {"num_1", 0x31},
        {"num_2", 0x32},
        {"num_3", 0x33},
        {"num_4", 0x34},
        {"num_5", 0x35},
        {"num_6", 0x36},
        {"num_7", 0x37},
        {"num_8", 0x38},
        {"num_9", 0x39},
        {"a", 0x41},
        {"b", 0x42},
        {"c", 0x43},
        {"d", 0x44},
        {"e", 0x45},
        {"f", 0x46},
        {"g", 0x47},
        {"h", 0x48},
        {"i", 0x49},
        {"j", 0x4A},
        {"k", 0x4B},
        {"l", 0x4C},
        {"m", 0x4D},
        {"n", 0x4E},
        {"o", 0x4F},
        {"p", 0x50},
        {"q", 0x51},
        {"r", 0x52},
        {"s", 0x53},
        {"t", 0x54},
        {"u", 0x55},
        {"v", 0x56},
        {"w", 0x57},
        {"x", 0x58},
        {"y", 0x59},
        {"z", 0x5A},
        {"backspace", 0x08},
        {"down_arrow", 0x28},
        {"enter", 0x0D},
        {"esc", 0x1B},
        {"home", 0x24},
        {"insert", 0x2D},
        {"left_alt", 0xA4},
        {"left_ctrl", 0xA2},
        {"left_shift", 0xA0},
        {"page_down", 0x22},
        {"page_up", 0x21},
        {"right_alt", 0xA5},
        {"right_ctrl", 0xA3},
        {"right_shift", 0xA1},
        {"space", 0x20},
        {"tab", 0x09},
        {"up_arrow", 0x26},
        {"f1", 0x70},
        {"f2", 0x71},
        {"f3", 0x72},
        {"f4", 0x73},
        {"f5", 0x74},
        {"f6", 0x75},
        {"f7", 0x76},
        {"f8", 0x77},
        {"f9", 0x78},
        {"f10", 0x79},
        {"f11", 0x7A},
        {"f12", 0x7B}

    };


    int num_keys = sizeof(keys) / sizeof(keys[0]);

    for (int i = 0; i < num_keys; i++) {
        if (strcmp(keys[i].key_name, input_key) == 0) {
            return keys[i].key;
        }
    }
    return -1;
}

bool get_valorant_colors(const char* pColor, int* pRed, int* pGreen, int* pBlue) {
    if (strcmp(pColor, "purple") == 0) {
        *pRed = 250;
        *pGreen = 100;
        *pBlue = 250;

        return true;
    }
    else if (strcmp(pColor, "yellow") == 0) {
        *pRed = 254;
        *pGreen = 254;
        *pBlue = 64;

        return true;
    }
    else if(strcmp(pColor, "red") == 0){
        *pRed = 254;
        *pGreen = 64;
        *pBlue = 64;

        return true;
    }
    else {
        return false;
    }
}

char* get_str(char* key_name) {
    FILE* file;
    char line[256];

    char* key;
    char* value;

    file = fopen("config.txt", "r");

    if (!file) {
        printf("The config.txt file was not found. Using default config.\n");

        file = fopen("config.txt", "w");
        if (file) {
            fprintf(file, "hold_mode=1\n");
            fprintf(file, "hold_key=left_shift\n");
            fprintf(file, "target_color=purple\n");
            fprintf(file, "color_sens=20\n");
            fprintf(file, "tap_time=220\n");
            fprintf(file, "scan_area_x=8\n");
            fprintf(file, "scan_area_y=8\n");
            fprintf(file, "key_up_rec_time=100\n");
            fclose(file);
        }
        else {
            printf("Error: Could not create the .txt file.\n");
        }
        file = fopen("config.txt", "r");
    }


    // Read the file line by line
    while (fgets(line, sizeof(line), file)) {
        key = strtok(line, "=");
        value = strtok(0, "=");

        if (value != 0) {
            // Remove newline character from value if present
            size_t len = strlen(value);
            if (len > 0 && value[len - 1] == '\n') {
                value[len - 1] = '\0';
            }
        }

        if (strcmp(key_name, key) == 0) {
            if (strcmp(key_name, "hold_key") == 0) {
                printf("The value of hold_key is: %s\n", value);
            }
            fclose(file);
            char* ret = (char*)malloc(strlen(value) + 1);
            strcpy(ret, value);
            return ret;
        }
    }


    fclose(file);
    printf("Error: The variable name could not be found in the txt file.\n");
    return 0;
}


int get_int(char* key_name) {
    char* str = get_str(key_name);
    if (str == 0) {
        return -1;
    }
    int ret = atoi(str);
    free(str);
    return ret;
}

double get_float(char* key_name) {
    double dnum = 0;
    char* str = get_str(key_name);
    if (str == 0) {
        return -1;
    }
    char local_value[256];
    strcpy(local_value, str);
    sscanf(local_value, "%lf", &dnum);
    free(str);
    return dnum;
}

double get_reaction_average(double total_reaction, int reaction_count) {
    return total_reaction / reaction_count;
}

void init_performance_counters() {
    QueryPerformanceFrequency(&frequency);
}

void start_counter() {
    QueryPerformanceCounter(&start); // Start timer
}

void stop_counter() {
    double elapsed_time;
    QueryPerformanceCounter(&end); // End timer
    elapsed_time = (double)(end.QuadPart - start.QuadPart) * 1000.0 / frequency.QuadPart;
    reaction_count++;
    total_reaction += elapsed_time;
    printf("\rReaction time: %.2f ms ", get_reaction_average(total_reaction, reaction_count));
}

void left_click() {
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);  // Press left mouse button
    Sleep(187);                                      // Wait 187ms
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);    // Release left mouse button
}

bool is_key_pressed(int hold_key) {
    return GetAsyncKeyState(hold_key) & 0x8000;
}



void print_logo() {
    printf("==================================================\n");
    printf("Version 2.2                     \n");
    printf("==================================================\n");
    printf("\n");
}
