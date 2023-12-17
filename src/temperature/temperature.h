#define TEMP_V25 0.760
#define TEMP_AVG_SLOPE 0.0025
#define TEMP_V_BOARD 3.0

// Conpute the current temperature
void temperature_update();

// Return 1 if the the current temperature is valid, and write it into temp
int temperature_get(float *temp);
