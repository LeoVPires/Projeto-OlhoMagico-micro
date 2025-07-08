#ifndef CAMERA_PINS_H
#define CAMERA_PINS_H

// Pinos da câmera para o módulo AI-Thinker ESP32-CAM
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1 // Usar -1 se não estiver conectado
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define LED_GPIO_NUM       4 // LED da placa, pode variar
#define I2C_SDA_GPIO_NUM   14 // Se estiver usando um sensor de luz, por exemplo
#define I2C_SCL_GPIO_NUM   15

#endif // CAMERA_PINS_H