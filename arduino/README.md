# EMG Data Collection with ESP32

This repository provides a setup for collecting EMG data using an ESP32 module. Below are instructions for using both Arduino IDE.

## Using Arduino IDE

1. Copy the project folder to `~/Arduino/`.
2. Open the Arduino IDE.
3. Open the project from the `~/Arduino/` directory.
4. Connect your ESP32 to your computer.
5. Select the appropriate board and port.
6. Compile and upload the code to your ESP32.

## Using PlatformIO IDE

1. Clone the repository to your local machine.
2. Open the PlatformIO IDE.
3. Open the project folder.
4. Connect your ESP32 to your computer.
5. Compile and upload the code to your ESP32.

## Connecting Multiple Clients

To connect multiple clients using ESP32, refer to the example provided in the [NimBLE-Arduino GitHub Issues section](https://github.com/h2zero/NimBLE-Arduino/issues/99).

**Max Client: 9**

## Additional Resources

### Tutorials

- **ESP32 BLE Server Programming**: Detailed step-by-step tutorials on various ESP32 topics, including BLE server programming, can be found in the series by 小鱼创意 on Bilibili. Start with [ESP32之低功耗蓝牙（BLE）服务端编程 - 基于Arduino](https://www.bilibili.com/video/BV1XD4y1K7xW/?spm_id_from=333.337.search-card.all.click).

### Example Code

The code for setting up an ESP32 as a BLE server can be downloaded from the following link:
- [Code for ESP32 BLE Server](https://pan.baidu.com/s/1IxmHo1M8TLo13XMwSbZrfQ?pwd=2hgy) (Extraction Code: 2hgy)

### Other Tutorials in the Series

1. **ESP32简介及Arduino IDE开发环境的配置**
2. **ESP32之闪烁的LED灯（基于Arduino IDE）**
3. **ESP32之两个不同闪烁周期的LED灯（不使用delay函数）**
4. **ESP32之使用按键控制LED灯（GPIO数字输入）- 基于Arduino IDE**
5. **ESP32之软件消除按键抖动（使用RBD_Button库）- 基于Arduino IDE**
6. **ESP32之PWM简介及LEDC的使用（PWM信号的输出） - 基于Arduino IDE**
7. **ESP32之LED呼吸灯的实现（LEDC的使用）- 基于Arduino IDE**
8. **ESP32之软件定时器的使用（AsyncTimer库的使用）- 基于Arduino IDE**
9. **ESP32之ADC(模数转换器)介绍及使用 - 基于Arduino IDE**
10. **ESP32之使用电位器控制LED亮度（综合使用ADC和LEDC）- 基于Arduino IDE**
