/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Matthias P. Braendli, Maximilien Cuony
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
*/

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
