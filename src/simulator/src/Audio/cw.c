#include "../../../common/src/Audio/cw.c"

extern char gui_cw_text[4096];
int gui_cw_text_pointer = 0;

void cw_move_buffer_up() {

    for (int i = 0; i <= 4010; i++) {
        gui_cw_text[i] = gui_cw_text[i + 10];
        gui_cw_text[i + 1] = '\0';
    }

    gui_cw_text_pointer -= 10;

}
void cw_message_sent(const char* str) {

    while(*str) {
        gui_cw_text[gui_cw_text_pointer+1] = '\0';
        gui_cw_text[gui_cw_text_pointer] = *str;
        gui_cw_text_pointer++;

        if (gui_cw_text_pointer >= 4000) {
            cw_move_buffer_up();
        }
        str++;
    }

    gui_cw_text[gui_cw_text_pointer+1] = '\0';
    gui_cw_text[gui_cw_text_pointer] = '\n';
    gui_cw_text_pointer++;
}
