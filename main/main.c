#include "include/myRC522.h"
#include "include/myServo.h"
#include "include/myNRF24.h"
#include "include/mySpeaker.h"
#include "include/myLaser.h"
#include "include/myElectromagnet.h"
#include "include/myIRSensor.h"
#include "driver/gptimer.h"
#include "mySPIFFS.h"
#include "myDCMotor.h"

static const char *TAG = "MAIN";

//******************************************FSM definition****************************************
// define state enum
typedef enum {
    STATE_INIT,
    STATE_NFC_READER,
    STATE_NFC_TO_PIANO,
    STATE_PIANO,
    STATE_PIANO_TO_LASER,
    STATE_LASER,
    STATE_LASER_TO_SIMON,
    STATE_SIMON,
    STATE_SIMON_TO_DISTANCE,
    STATE_DISTANCE,
    STATE_FINISH,
    STATE_STOP,
    NUM_STATES
} State;

// define event enum
typedef enum {
    EVENT_INIT,
    EVENT_INIT_FINISHED,
    EVENT_NFC_READER_FINISHED,
    EVENT_NFC_TO_PIANO_FINISHED,
    EVENT_PIANO_FINISHED,
    EVENT_PIANO_TO_LASER_FINISHED,
    EVENT_LASER_FINISHED,
    EVENT_LASER_TO_SIMON_FINISHED,
    EVENT_SIMON_FINISHED,
    EVENT_SIMON_TO_DISTANCE_FINISHED,
    EVENT_DISTANCE_FINISHED,
    EVENT_FINISH_FINISHED,
    EVENT_RESTART,
    NUM_EVENTS
} Event;

// state transfer definition
typedef State (*StateFunc)();

//state execution and return next state
State doInit(Event* event);
State doNFCReader(Event* event);
State doNFCToPiano(Event* event);
State doPiano(Event* event);
State doPianoToLaser(Event* event);
State doLaser(Event* event);
State doLaserToSimon(Event* event);
State doSimon(Event* event);
State doSimonToDistance(Event* event);
State doDistance(Event* event);
State doFinish(Event* event);
State doStop(Event* event);

//state  transfer table
StateFunc stateTable[NUM_STATES] = {
        doInit,
        doNFCReader,
        doNFCToPiano,
        doPiano,
        doPianoToLaser,
        doLaser,
        doLaserToSimon,
        doSimon,
        doSimonToDistance,
        doDistance,
        doFinish,
        doStop
};
//********************************************FSM DEFINITION END******************************************
NRF24_t dev;
gptimer_handle_t timer_handle;
gptimer_handle_t timer_handle_audio;
rc522_handle_t scanner;
mcpwm_cmpr_handle_t servo_laser_handle;
int flag = 0;

esp_err_t init();

void app_main(void) {
    State currentState = STATE_INIT;
    Event event = EVENT_INIT;

    while (!(currentState == STATE_STOP && flag == 1)) {
        currentState = stateTable[currentState](&event);
    }
}

esp_err_t init(){
    esp_err_t ret;
    ret = rc522_power_down(); //TODO fix commenting speaker, nrf, and rc522 parts
    if(ret != ESP_OK)   return ret;

    ret = nrf24_init(&dev);
    if(ret != ESP_OK)   return ret;

    init_spiffs();

    ret = speaker_init(&timer_handle);
    if(ret != ESP_OK)   return ret;

    ret = speaker_init_audio(&timer_handle_audio);
    if(ret != ESP_OK)   return ret;

    servo_init(48, &servo_laser_handle);
    rotate_servo(90, &servo_laser_handle); //block laser

    ret = rc522_init(&scanner, &timer_handle_audio);
    if(ret != ESP_OK)   return ret;

    ret = Electromagnet_init();
    ESP_ERROR_CHECK(Motor_init());

    return ret;
}

//*******************************************FSM transfer function*****************************************
State doInit(Event* event) {
    ESP_LOGI(TAG,"start init\n");
    if(*event != EVENT_INIT) return STATE_STOP;

    if(init() == ESP_OK){
        ESP_LOGI(TAG,"done init\n");
//        *event = EVENT_PIANO_FINISHED;
//        return STATE_PIANO_TO_LASER;
//        *event = EVENT_SIMON_TO_DISTANCE_FINISHED;
//        return STATE_DISTANCE;
//        *event = EVENT_DISTANCE_FINISHED;
//        return STATE_FINISH;
        *event = EVENT_INIT_FINISHED;
        return STATE_NFC_READER;
    }
    ESP_LOGI(TAG,"init fail\n");
    return STATE_INIT;
}

State doNFCReader(Event* event){
    ESP_LOGI(TAG,"start NFC\n");
    if(*event != EVENT_INIT_FINISHED)  return STATE_STOP;

    if(myRC522_start(&scanner) == ESP_OK){
//        *event = EVENT_NFC_TO_PIANO_FINISHED;
//        return STATE_PIANO;
        *event = EVENT_NFC_READER_FINISHED;
        return STATE_NFC_TO_PIANO;
    }
    ESP_LOGI(TAG,"done NFC\n");
    return STATE_NFC_READER;
}

State doNFCToPiano(Event* event){
    ESP_LOGI(TAG,"start NFCToPiano\n");
    if(*event != EVENT_NFC_READER_FINISHED) return STATE_STOP;

    *event = EVENT_NFC_TO_PIANO_FINISHED;
    ESP_LOGI(TAG,"done NFCToPiano\n");
    return STATE_PIANO;
    if(notifyPiano(&dev) == ESP_OK){
        *event = EVENT_NFC_TO_PIANO_FINISHED;
        ESP_LOGI(TAG,"done NFCToPiano\n");
        return STATE_PIANO;
    }
    ESP_LOGI(TAG,"fail NFCToPiano\n");
    return STATE_STOP;
}

State doPiano(Event* event){
    if(*event != EVENT_NFC_TO_PIANO_FINISHED) return STATE_STOP;

    if(isPianoFinished(&timer_handle_audio, &dev) == ESP_OK){
        *event = EVENT_PIANO_FINISHED;
        return STATE_PIANO_TO_LASER;
    }
    return STATE_PIANO_TO_LASER;
}

State doPianoToLaser(Event *event){
    if(*event != EVENT_PIANO_FINISHED)  return STATE_STOP;

    rotate_servo(0, &servo_laser_handle);
    ESP_LOGI(TAG, "rotate servo");

    if(isReadyForLaser(&dev) == ESP_OK){
        *event = EVENT_PIANO_TO_LASER_FINISHED;
        return STATE_LASER;
    }
    return STATE_LASER;
}

State doLaser(Event* event){
    if(*event != EVENT_PIANO_TO_LASER_FINISHED)  return STATE_STOP;

    ESP_LOGI(TAG,"Laser running...\n");

    bool ret = laserSensorMain();

    if (ret == true) {
        ESP_LOGI(TAG,"Laser done\n");
        *event = EVENT_SIMON_FINISHED;
        return STATE_SIMON_TO_DISTANCE;
    } else {
        ESP_LOGI(TAG,"Not supposed to happen...");
        *event = EVENT_PIANO_TO_LASER_FINISHED;
        return STATE_LASER;
    }
}

State doLaserToSimon(Event* event){
    *event = EVENT_SIMON_FINISHED;
    return STATE_SIMON_TO_DISTANCE;
}

State doSimon(Event* event){
    ESP_LOGI(TAG,"Simon running...\n");
    *event = EVENT_SIMON_FINISHED;
    return STATE_SIMON_TO_DISTANCE;
}

State doSimonToDistance(Event* event){
    if (*event != EVENT_SIMON_FINISHED) return STATE_STOP;

    esp_err_t ret = setEM(0);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG,"simon to distance done, EM should be off");
        *event = EVENT_SIMON_TO_DISTANCE_FINISHED;
        return STATE_DISTANCE;
    }

    return STATE_DISTANCE;
}

State doDistance(Event* event){
    if (*event != EVENT_SIMON_TO_DISTANCE_FINISHED) return STATE_STOP;
    ESP_LOGI(TAG, "start distance");

    play_sine_start(220, 20, &timer_handle);

    //call func
    bool ret = irSensorMain(&timer_handle); //fc only returns after game is done

    if (ret == true) {
        *event = EVENT_DISTANCE_FINISHED;
        ESP_LOGI(TAG,"Distance done\n");
        play_sine_stop(&timer_handle);
        return STATE_FINISH;

    } else {
        ESP_LOGI(TAG,"Not supposed to happen...");
        *event = EVENT_SIMON_TO_DISTANCE_FINISHED;
        return STATE_DISTANCE;
    }
}

State doFinish(Event* event){
    ESP_ERROR_CHECK(ElevatorMotor());
    play_speaker_audio("/spiffs/pass.txt", &timer_handle_audio);
//    vTaskDelay(pdMS_TO_TICKS(2500));
//    play_speaker_audio("/spiffs/c4.txt", &timer_handle_audio);
//    vTaskDelay(pdMS_TO_TICKS(1100));
//    play_speaker_audio("/spiffs/d4.txt", &timer_handle_audio);
//    vTaskDelay(pdMS_TO_TICKS(1100));
//    play_speaker_audio("/spiffs/e4.txt", &timer_handle_audio);
//    vTaskDelay(pdMS_TO_TICKS(1100));
//    play_speaker_audio("/spiffs/f4.txt", &timer_handle_audio);
//    vTaskDelay(pdMS_TO_TICKS(1100));
//    play_speaker_audio("/spiffs/g4.txt", &timer_handle_audio);
//    vTaskDelay(pdMS_TO_TICKS(1100));
//    play_speaker_audio("/spiffs/a4.txt", &timer_handle_audio);
//    vTaskDelay(pdMS_TO_TICKS(1100));
//    play_speaker_audio("/spiffs/b4.txt", &timer_handle_audio);
//    vTaskDelay(pdMS_TO_TICKS(1100));
//    play_speaker_audio("/spiffs/c5.txt", &timer_handle_audio);
//    vTaskDelay(pdMS_TO_TICKS(1100));
    *event = EVENT_FINISH_FINISHED;
    return STATE_STOP;
}

State doStop(Event* event){
    if(*event != EVENT_FINISH_FINISHED) return STATE_STOP;

    ESP_LOGI(TAG, "start detect restart");
    if(detectRestartTag(&scanner) == ESP_OK){
        *event = EVENT_INIT_FINISHED;
        return STATE_NFC_READER;
    }
    else{
        flag = 1;
    }
    return STATE_NFC_READER;
}