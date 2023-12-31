/**
 * @file main.c
 * @author ElectroNick (nick@dsd.dev)
 * @brief Main file of DeepDeck, an open source Macropad based on ESP32
 * @version 0.2
 * @date 2022-12-08
 * @copyright Copyright (c) 2022
 *
 * MIT License
 * Copyright (c) 2022
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
 *
 * DeepDeck, a product by DeepSea Developments.
 * More info on DeepDeck @ www.deepdeck.co
 * DeepseaDev.com
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "driver/touch_pad.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "esp_pm.h"

// HID Ble functions
#include "hal_ble.h"

// Deepdeck functions
#include "matrix.h"
#include "keymap.h"
#include "keyboard_config.h"
#include "battery_monitor.h"
#include "nvs_funcs.h"
#include "nvs_keymaps.h"
#include "mqtt.h"

#include "esp_err.h"

#include "plugins.h"
#include "deepdeck_tasks.h"
#include "gesture_handles.h"
#include "wifi_handles.h"
#include "server.h"
#include "spiffs.h"
#include "keys.h"


#include "esp_console.h"
#include "driver/uart.h"

#include "esp_system.h"

#include "esp_sleep.h"
#include "esp_timer.h"



uint8_t check_blink(char * string, uint8_t * pos, uint8_t * time)
{
	char  base_string[25] = "blink [0][1] 30\n";
	uint8_t row=0;
	uint8_t col=0;
	char time_aux[3] = {0};
	*pos = 0;
	*time = 0;

	int i = 0;
	for(i = 0;i<7; i++)
	{
		if(string[i] != base_string[i])
			return 1;

	}
	ESP_LOGI("check_blink","OK %d",i);

	// ---------------- ROW -------------------

	if(string[i] >= '0' && string[i] <= '3')
	{
		row = string[i] - '0';
		i++;
	}
	else //out of boundries
		return 1;	
	
	ESP_LOGI("check_blink","OK %d",i);
	//if data is not ] then its wrong
	if(string[i] != ']') 
		return 1;
	i++;
	
	ESP_LOGI("check_blink","OK %d",i);
	//if data is not [ then its wrong
	if(string[i] != '[') 
		return 1;
	i++;

	ESP_LOGI("check_blink","OK %d",i);
	// ---------- COL -------------
	if(string[i] >= '0' && string[i] <= '3')
	{
		col = string[i] - '0';
		i++;
	}
	else // out of boundries
		return 1;
	
	ESP_LOGI("check_blink","OK %d",i);
	//if data is not ] then its wrong
	if(string[i] != ']') 
		return 1;
	i++;

	ESP_LOGI("check_blink","OK %d",i);
	//if data is not [ then its wrong
	if(string[i] != ' ') 
		return 1;
	i++;

	ESP_LOGI("check_blink","OK %d",i);
	// --------- TIME ------------

	if(string[i] >= '0' && string[i] <= '9')
	{
		time_aux[0] = string[i];
		i++;
	}
	else //Not numeric value, return error
		return 1;

	ESP_LOGI("check_blink","OK %d",i);
	//Check if the number has 2 digits or not
	if(string[i] >= '0' && string[i] <= '9') 
	{
		time_aux[1] = string[i];
		i++;
	}
	else if(string[i] == 0) // Time just have 1 number. parse and return ok
	{
		*time = time_aux[0] - '0';
		ESP_LOGI("--","pos: %d, time: %d",*pos,*time);
		ESP_LOGI("--","time_aux: %s, row: %d, col: %d",time_aux, row, col);
		*pos = row*4 + col;
		return 0; //Parsing succesfull
	}
	else
		return 1;

	ESP_LOGI("check_blink","OK %d",i);
	// Time has 2 digits. check if its 30 or less.
	if(string[i] == 0)
	{
		ESP_LOGI("check_blink","%d %d",(time_aux[0] - '0')*10,(time_aux[1] - '0')  );
		*time = (time_aux[0] - '0')*10 + (time_aux[1] - '0');
		i++;
		if(*time > 30)
			return 1;
	}
	else	
		return 1;

	 ESP_LOGI("check_blink","OK %d",i);

	*pos = row*4 + col;

	ESP_LOGI("--","pos: %d, time: %d",*pos,*time);
	ESP_LOGI("--","time_aux: %s, row: %d, col: %d",time_aux, row, col);



	return 0;
}


/**
 * @brief Main tasks of ESP32. This is a tasks with priority level 1.
 *
 * This task init all the basic hardware and then init the tasks needed to run DeepDeck
 */
void app_main()
{

	esp_log_level_set("*", ESP_LOG_INFO);

	xTaskCreate(rgb_leds_task, "rgb_leds_task", MEM_LEDS_TASK, NULL, PRIOR_LEDS_TASK, NULL);
	ESP_LOGI("rgb_leds_task", "initialized");

    char chr[9]={0};
	char c=0;

	char command[25]={};
	int i=0;

	uint8_t pos = 0;
	uint8_t time;
	uint8_t error;


    while(1){
        
        scanf("%c", chr);
		if(chr[0])
		{
			printf("%c", chr[0]);
			
			if (chr[0]!='\n')
			{
				command[i] = chr[0];
				i++;
			}
			else
			{
				command[i] = 0;
				i = 0;
				printf("\nData entered : %s \n", command);

				error = check_blink(command, &pos, &time);

				if(error)
				{
					printf("ERROR\n");
				}
				else
				{
					rgb_mode_t mode = {.pos=pos, .secs=time};
					ESP_LOGI("main","pos: %d, time: %d",pos, time);
					xQueueSend(keyled_q, &mode, 0);
					printf("OK\n");
				}
			}
			chr[0] = 0;
			
			// echo to help visualization
		}
    }

	
}

