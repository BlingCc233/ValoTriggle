#include "helper.h"
#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <tlhelp32.h>


// 生成简单的随机UUID字符串
void generate_uuid(char* uuid) {
    static const char hex_chars[] = "0123456789abcdef";
    int i;
    
    srand((unsigned int)time(NULL));
    
    for(i = 0; i < 32; i++) {
        uuid[i] = hex_chars[rand() % 16];
        if(i == 7 || i == 11 || i == 15 || i == 19) {
            uuid[i+1] = '-';
            i++;
        }
    }
    uuid[36] = '\0';
}

// 修改进程名称
void set_process_name(const char* name) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 process = { sizeof(process) };
    DWORD pid = GetCurrentProcessId();
    
    if(Process32First(snapshot, &process)) {
        do {
            if(process.th32ProcessID == pid) {
                HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
                if(hProcess != NULL) {
                    SetWindowTextA(GetConsoleWindow(), name);
                    CloseHandle(hProcess);
                }
                break;
            }
        } while(Process32Next(snapshot, &process));
    }
    CloseHandle(snapshot);
}

// 修改窗口标题和进程名称
void set_window_and_process_name(const char* name) {
    // 设置控制台窗口标题
    SetConsoleTitleA(name);
    
    // 修改进程名称
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 process = { sizeof(process) };
    DWORD pid = GetCurrentProcessId();
    
    if(Process32First(snapshot, &process)) {
        do {
            if(process.th32ProcessID == pid) {
                HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
                if(hProcess != NULL) {
                    CloseHandle(hProcess);
                }
                break;
            }
        } while(Process32Next(snapshot, &process));
    }
    CloseHandle(snapshot);
}

// 重命名当前执行文件
void rename_executable(const char* new_name) {
    char current_path[MAX_PATH];
    char new_path[MAX_PATH];
    
    GetModuleFileNameA(NULL, current_path, MAX_PATH);
    strcpy(new_path, current_path);
    char* last_slash = strrchr(new_path, '\\');
    if(last_slash) {
        sprintf(last_slash + 1, "%s.exe", new_name);
        MoveFileA(current_path, new_path);
    }
}


volatile int hold_mode = 1;

DWORD WINAPI toggle_hold_mode(LPVOID lpParam) {
    int x_key = get_key_code("f3");

    while (true) {
        if (is_key_pressed(x_key)) {
            hold_mode = (hold_mode == 0) ? 1 : 0;
            printf("hold_mode toggled to %d\n", hold_mode);
            if (hold_mode == 0) {
                printf("\a");
            }
            Sleep(200);
        }
        Sleep(10);
    }

}

DWORD WINAPI adjust_color_sens(LPVOID lpParam) {
    CONFIG* cfg = (CONFIG*)lpParam;
    int plus_key = get_key_code("f4");
    int dash_key = get_key_code("f5");

    while (true) {
        if (is_key_pressed(plus_key)) {
            cfg->color_sens += 5;
            printf("color_sens increased to %d\n", cfg->color_sens);
            Sleep(200);
        }
        if (is_key_pressed(dash_key)) {
            cfg->color_sens -= 5;
            printf("color_sens decreased to %d\n", cfg->color_sens);
            Sleep(200);
        }
        Sleep(10);
    }
}

int main(int argc, char* argv[]) {
    char uuid[37];
    generate_uuid(uuid);
    set_process_name(uuid);
    // 设置窗口标题和进程名
    set_window_and_process_name(uuid);
    
    // 重命名可执行文件
    rename_executable(uuid);
    
    int red, blue, green;
    int w_key = get_key_code("w");
    int a_key = get_key_code("a");
    int s_key = get_key_code("s");
    int d_key = get_key_code("d");
    CONFIG cfg = { 0 };

    if (!get_config(&cfg)) {
        printf("Error: The variable name could not be found in the txt file.");
        printf("\nPress enter to exit: ");
        getchar();
        return 1;
    }

    if (!get_valorant_colors(cfg.target_color, &red, &green, &blue)) {
        printf("Error: Unknown color.");
        printf("\nPress enter to exit: ");
        getchar();
        return 1;
    }
    else {
        printf("red: %d\t", red);
        printf("green: %d\t", green);
        printf("blue: %d\n", blue);
    }

    init_performance_counters();
    int pixel_count = cfg.scan_area_x * cfg.scan_area_y;
    get_screenshot(0, cfg.scan_area_x, cfg.scan_area_y); // Call it once
    print_logo();

    bool last_detected = false;
    bool keys_pressed = false;
    hold_mode = cfg.hold_mode;

    HANDLE hThread = CreateThread(NULL, 0, toggle_hold_mode, NULL, 0, NULL);
    if (hThread == NULL) {
        printf("Error: Unable to create thread.");
        printf("\nPress enter to exit: ");
        getchar();
        return 1;
    }

    HANDLE hAdjustThread = CreateThread(NULL, 0, adjust_color_sens, &cfg, 0, NULL);
    if (hAdjustThread == NULL) {
        printf("Error: Unable to create adjust_color_sens thread.");
        printf("\nPress enter to exit: ");
        getchar();
        return 1;
    }

    if (get_key_code(cfg.hold_key) == -1) {
        printf("The hold_key you choose could not be found");
        printf("\nPress enter to exit: ");
        getchar();
        return 1;
    }

    while (true) {
        start_counter();

        if (is_key_pressed(w_key) || is_key_pressed(a_key) || is_key_pressed(s_key) || is_key_pressed(d_key)) {
            keys_pressed = true;
            continue;
        }
        else if (keys_pressed) {
            Sleep(cfg.key_up_rec_time);
            keys_pressed = false;
        }

        if (hold_mode == 1 || is_key_pressed(get_key_code(cfg.hold_key))) {
            unsigned int* pPixels = get_screenshot(0, cfg.scan_area_x, cfg.scan_area_y);

            if (pPixels == 0) {
                printf("ERROR: get_screenshot() failed!");
                printf("\nPress enter to exit: ");
                getchar();
                return 1;
            }

            if (is_color_found_DE_single(pPixels, pixel_count, red, green, blue, cfg.color_sens)) {
                if (!last_detected) {
                    // Sleep 0~20ms
                    Sleep(rand() % 20);
                }
                left_click();
                stop_counter();
                last_detected = true;
                Sleep(cfg.tap_time);
            }    else{    last_detected = false;}
            free(pPixels);
        }
        else {
            Sleep(1);
        }
    }
}
