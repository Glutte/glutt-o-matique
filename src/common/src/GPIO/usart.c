#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>
#include "Core/common.h"
#include "GPIO/usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// The ISR writes into this buffer
static char nmea_sentence[MAX_NMEA_SENTENCE_LEN];
static int  nmea_sentence_last_written = 0;


// Once a completed NMEA sentence is received in the ISR,
// it is appended to this queue
static QueueHandle_t usart_nmea_queue;

void usart_gps_init() {
    usart_nmea_queue = xQueueCreate(15, MAX_NMEA_SENTENCE_LEN);
    if (usart_nmea_queue == 0) {
        while(1); /* fatal error */
    }

    usart_gps_specific_init();

}

void usart_gps_puts(const char* str) {
    vTaskSuspendAll();
    return usart_puts(USART3, str);
    xTaskResumeAll();
}

#define MAX_MSG_LEN 80
static char usart_debug_message[MAX_MSG_LEN];

void usart_debug_timestamp() {
    // Don't call printf here, to reduce stack usage
    uint64_t now = timestamp_now();
    if (now == 0) {
        usart_puts(USART2, "[0] ");
    }
    else {
        char ts_str[64];
        int i = 63;

        ts_str[i--] = '\0';
        ts_str[i--] = ' ';
        ts_str[i--] = ']';

        while (now > 0 && i >= 0) {
            ts_str[i--] = '0' + (now % 10);
            now /= 10;
        }
        ts_str[i] = '[';

        usart_puts(USART2, &ts_str[i]);
    }
}

void usart_debug(const char *format, ...) {
    va_list list;
    va_start(list, format);
    vsnprintf(usart_debug_message, MAX_MSG_LEN-1, format, list);

    vTaskSuspendAll();
    usart_debug_timestamp();
    usart_puts(USART2, usart_debug_message);
    xTaskResumeAll();

    va_end(list);
}

void usart_debug_puts(const char* str) {
    vTaskSuspendAll();
    usart_debug_timestamp();
    usart_puts(USART2, str);
    xTaskResumeAll();
}

void usart_debug_puts_no_header(const char* str) {
    vTaskSuspendAll();
    usart_puts(USART2, str);
    xTaskResumeAll();
}

int usart_get_nmea_sentence(char* nmea) {
    return xQueueReceive(usart_nmea_queue, nmea, portMAX_DELAY);
}


static void usart_clear_nmea_buffer(void) {
    for (int i = 0; i < MAX_NMEA_SENTENCE_LEN; i++) {
        nmea_sentence[i] = '\0';
    }
    nmea_sentence_last_written = 0;
}

void usart_process_char(char c) {

    if (c == 'h') {
        usart_debug_puts("help: no commands supported yet!\r\n");
    } else {
        usart_debug("Unknown command %c\r\n", c);
    }

}

void usart_gps_process_char(char c) {

    if (nmea_sentence_last_written == 0) {
        if (c == '$') {
            // Likely new start of sentence
            nmea_sentence[nmea_sentence_last_written] = c;
            nmea_sentence_last_written++;
        }
    }
    else if (nmea_sentence_last_written < MAX_NMEA_SENTENCE_LEN) {
        nmea_sentence[nmea_sentence_last_written] = c;
        nmea_sentence_last_written++;

        if (c == '\n') {
            int success = xQueueSendToBackFromISR(
                    usart_nmea_queue,
                    nmea_sentence,
                    NULL);

            if (success == pdFALSE) {
                trigger_fault(FAULT_SOURCE_USART);
            }

            usart_clear_nmea_buffer();
        }
    }
    else {
        // Buffer overrun without a meaningful NMEA message.
        usart_clear_nmea_buffer();
    }

}
