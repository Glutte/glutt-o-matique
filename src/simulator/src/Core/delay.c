void delay_init() {
}

void delay_us(int micros) {
    usleep(micros);
}

void delay_ms(int millis) {
    sleep(millis);
}

