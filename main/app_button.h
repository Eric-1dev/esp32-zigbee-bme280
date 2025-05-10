#include <stdio.h>

#define BUTTON_GPIO             GPIO_NUM_9
#define LONG_PRESS_TIEMOUT_MS   3000
#define SHORT_PRESS_TIEMOUT_MS  500

typedef void (*button_event_cb)(void);

void button_task(void *pvParameters);
bool is_button_pressed(void);
void register_long_press_callback(button_event_cb cb);
void register_short_press_callback(button_event_cb cb);