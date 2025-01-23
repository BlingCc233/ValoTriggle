#include "helper.h"
#include <windows.h>

volatile int hold_mode = 1;

DWORD WINAPI toggle_hold_mode(LPVOID lpParam) {
    int x_key = get_key_code("tilde");

    while (true) {
        if (is_key_pressed(x_key)) {
            hold_mode = (hold_mode == 0) ? 1 : 0;
            printf("hold_mode toggled to %d\n", hold_mode);
            if (hold_mode == 0) {
                printf("\a");
            }
            else{
                printf("\a");
                Sleep(100);
                printf("\a");
            }
            Sleep(200);
        }
        Sleep(10);
    }

}

DWORD WINAPI adjust_color_sens(LPVOID lpParam) {
    CONFIG* cfg = (CONFIG*)lpParam;
    int plus_key = get_key_code("plus");
    int dash_key = get_key_code("dash");

    while (true) {
        if (is_key_pressed(plus_key)) {
            cfg->color_sens += 10;
            printf("color_sens increased to %d\n", cfg->color_sens);
            Sleep(200);
        }
        if (is_key_pressed(dash_key)) {
            cfg->color_sens -= 10;
            printf("color_sens decreased to %d\n", cfg->color_sens);
            Sleep(200);
        }
        Sleep(10);
    }
}

int main(int argc, char* argv[]) {
    int red, blue, green;
    int w_key = get_key_code("w");
    int a_key = get_key_code("a");
    int s_key = get_key_code("s");
    int d_key = get_key_code("d");
    CONFIG cfg = {0};

    if (!get_config(&cfg)) {
        printf("Error: The variable name could not be found in the txt file.");
        printf("\nPress enter to exit: ");
        getchar();
        return 1;
    }

	if (!get_valorant_colors(cfg.target_color, &red, &green ,&blue)) {
        printf("Error: Unknown color.");
        printf("\nPress enter to exit: ");
        getchar();
        return 1;
    } else {
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
        } else if (keys_pressed) {
            Sleep(cfg.key_up_rec_time);
            keys_pressed = false;
        }

        if (hold_mode == 1 || is_key_pressed(get_key_code(cfg.hold_key))) {
            unsigned int *pPixels = get_screenshot(0, cfg.scan_area_x, cfg.scan_area_y);

            if (pPixels == 0) {
                printf("ERROR: get_screenshot() failed!");
                printf("\nPress enter to exit: ");
                getchar();
                return 1;
            }

            if (is_color_found_DE(pPixels, pixel_count, red, green, blue, cfg.color_sens)) {
                if(!last_detected) {
                    // Sleep 50~120ms
                    Sleep(50 + rand() % 70);
                }
				left_click();
                stop_counter();
                last_detected = true;
                Sleep(cfg.tap_time);
            }
            free(pPixels);
        }
        else{
            Sleep(1);
        }
    }
}
