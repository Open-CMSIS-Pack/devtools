# Reference Application Framework

(Proposal)

<!-- markdownlint-disable MD009 -->
<!-- markdownlint-disable MD013 -->
<!-- markdownlint-disable MD036 -->

The following section describes the structure of a standardized **Reference Application Framework** that allows to demonstrate a range of application examples.   such as:
  
- Cloud connectivity using SDKs from Cloud Service Providers
- Sensor reference examples
- Machine Learning applications that use sensors and audio inputs
- Middleware examples such as TCP/IP stack and file system

These reference examples can target various evaluation boards. It is also possible to target production hardware and use the examples are starting point for embedded IoT and ML applications. This is enabled by:

- Consistent bootstrap structure Standardized connections ation is enabled by software layers that used sta.

![Layers of Reference Examples](./images/Reference-Example.png "Layers of Reference Examples")

ToDo:

- explain structure of each layer
- add CMSIS-Stream and ML stacks to the

## Standardized Connections

The following chapter lists the standardized connections between project layers to enable standardized reference applications.

### Board Layer

This table lists the connections that can be exposed by a board layer.

`connect` Name         | Value                  | Description
:----------------------|:-----------------------|:-------------------- 
.                      |.                       | **Arduino Interface**
ARDUINO_UNO_UART       | CMSIS-Driver instance  | CMSIS-Driver UART connecting to UART on Arduino pins D0..D1
ARDUINO_UNO_SPI        | CMSIS-Driver instance  | CMSIS-Driver SPI connecting to SPI on Arduino pins D10..D13
ARDUINO_UNO_I2C        | CMSIS-Driver instance  | CMSIS-Driver I2C connecting to I2C on Arduino pins D20..D21
ARDUINO_UNO_I2C-Alt    | CMSIS-Driver instance  | CMSIS-Driver I2C connecting to I2C on Arduino pins D18..D19
ARDUINO_UNO_D0 .. D21  | CMSIS-Driver instance  | CMSIS-Driver GPIO connecting to Arduino pins D0..D21
.                      |.                       | **Startup Configuration**
Heap                   | Heap Size              | Memory heap configuration in startup

### Shield Layer

External hardware components are frequently available on Arduino Shields that connect to hardware boards. Components such as sensors can be added to 

**Arduino Shield Pinout**

There are several different Arduino pin mappings, however for standardization, a consistent naming is required. The pinout used by teh [Board Layer](#board-layer) is shown in the diagram below.

![Arduino Shield Pinout](./images/Arduino-Shield.png "Arduino Shield Pinout")
