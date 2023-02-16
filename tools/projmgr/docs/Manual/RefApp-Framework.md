# Reference Application Framework

(Proposal)

<!-- markdownlint-disable MD009 -->
<!-- markdownlint-disable MD013 -->
<!-- markdownlint-disable MD036 -->

The following section describes the structure of a standardized **Reference Application Framework** that allows to demonstrate a range of application examples:
  
- Cloud connectivity using SDKs from Cloud Service Providers
- Sensor reference examples
- Machine Learning applications that use sensors and audio inputs
- Middleware examples such as TCP/IP stack and file system

These reference examples can target various evaluation boards. It is also possible to target production hardware and use the examples are starting point for embedded IoT and ML applications. This is enabled by:

- Software layers with defined and standardized interfaces that contain re-usable parts of applications.
- Description of standardized connections (provided and consumed interfaces) between these software layers. 
- Consistent bootstrap and application startup that initializes. 
- Board software layer that provides connections for many different applications.

![Layers of Reference Examples](./images/Reference-Example.png "Layers of Reference Examples")

ToDo:

- explain structure of each layer
- add CMSIS-Stream and ML stacks to the

## Software Layers

The following table lists the various software layers types that are used to compose reference applications.

Software Layer Type    | Description of the operations
:----------------------|:----------------------------------
Board                  | System startup: board/device hardware initialization; transfers control to application. Exposes various drivers and interfaces.
Shield                 | Provides software definitions and support for additional hardware provided on shields that extend a physical hardware board. 
RTOS                   | Provides a CMSIS-RTOS2 complained RTOS; various RTOS implementations may provide extra functionality.
Socket                 | Provides an IoT_Socket for network connectivity.

Each of the software layers is described in the following section.

### Software Layer Type: Board

Provides system startup, board/device hardware initialization, and transfers control to the application. It also exposes various drivers and interfaces.

**Features:**

- System startup
- Heap and Stack configuration
- Device/Board hardware initialization
- Event Recorder initialization [optional] 
- RTOS initialization and startup [optional]
- Application startup for applications with and without RTOS
- STDIO re-targeting

**Software modules:**

- Startup file
- System file
- Main module with function ‘main’
- Drivers and interfaces (CMSIS-Drivers, GPIO, STDIO)
- Linker script definition

The startup of the Board software layer supports uses two function call entry points:

- `app_initialize` initializes the software stack and is executed in Handler mode.
- `app_main` transfers execution to the application. For RTOS-based applications this is a RTOS thread.

**Not using RTOS:**
`extern int app_main (void);` defines the entry point of the application; executes as a single thread with default settings (priority, stack).
b.)	Board layer using RTOS:
Before RTOS is started the ‘app_initialize’ is called where the user can create application threads. Default weak implementation in board layer:
void app_initialize (void) {
  osThreadNew(app_main, NULL, NULL);
}
By default, ‘app_main’ is the thread that starts executing.
I.	Application not using RTOS:
‘app_main’ application entry starts executing as a single thread with default settings (priority, stack)
II.	Application using RTOS:
‘app_main’ is the application entry thread with default settings (can be overridden by re-implementing ‘app_initialize’

### Board

**Provided Connections**

`connect` Name         | Value                  | Description
:----------------------|:-----------------------|:-------------------- 
.                      |.                       | **Arduino Shield Interface**
ARDUINO_UNO_UART       | CMSIS-Driver instance  | CMSIS-Driver UART connecting to UART on Arduino pins D0..D1
ARDUINO_UNO_SPI        | CMSIS-Driver instance  | CMSIS-Driver SPI connecting to SPI on Arduino pins D10..D13
ARDUINO_UNO_I2C        | CMSIS-Driver instance  | CMSIS-Driver I2C connecting to I2C on Arduino pins D20..D21
ARDUINO_UNO_I2C-Alt    | CMSIS-Driver instance  | CMSIS-Driver I2C connecting to I2C on Arduino pins D18..D19
ARDUINO_UNO_D0 .. D21  | -                      | CMSIS-Driver GPIO connecting to Arduino pins D0..D21
.                      |.                       | **CMSIS Driver Interfaces**
CMSIS_ETH              | CMSIS-Driver instance  | CMSIS-Driver Ethernet (combined MAC and PHY)
CMSIS_ETH_MAC          | CMSIS-Driver instance  | CMSIS-Driver Ethernet MAC
CMSIS_ETH_PHY          | CMSIS-Driver instance  | CMSIS-Driver Ethernet PHY
CMSIS_WiFi             | CMSIS-Driver instance  | CMSIS-Driver WiFi
CMSIS_VIO              | CMSIS-Driver instance  | CMSIS-Driver Ethernet VIO for LEDs
.                      |.                       | **I/O Retargeting**
STDERR                 |.                       | Standard Error output
STDIN                  |.                       | Standard Input
STDOUT                 |.                       | Standard Output
.                      |.                       | **Memmory allocation**
Heap                   | Heap Size              | Memory heap configuration in startup

### Shield

Provides support for external hardware on shields that can be plugged into the board module. Currently only Arduino Uno shields (connections defined by ‘ARDUINO_UNO_’) are supported. Potentially also shields with other connections could be covered.

Shields feature various hardware modules: WiFi, Sensors, …

Features:
-	Drivers for shield’s hardware modules

Connections (consumed):
-	ARDUIINO_UNO_* (provided by board layer)
-	STDIN/OUT (if used by the driver)
-	RTOS2 (if used by the driver)

Connections (provided):
-	CMSIS_WiFi (WiFi Shield)
-	<Sensor>
-	<Sensor>_<Feature>

Note regarding sensors:
<Sensor> is typically a sensor name (for example NXP sensors: FXLS8974, FXAS21002, …
Sensor can use different buses (I2C, SPI) and have optional interrupt lines. It makes sense to define sensor connections with specifying features to cover various layer combinations. Examples of connections:
FXLS8974 sensor:
-	Connected via I2C:
o	FXLS8974_I2C: connected via I2C
o	FXLS8974_INT: interrupt line (optional)
-	Connected via SPI:
o	FXLS8974_SPI: connected via SPI
o	FXLS8974_CS: SPI CS (can be any Arduino Dx pin to cover multiple devices on the same SPI bus)
o	FXLS8974_INT: interrupt line (optional)

External hardware components are frequently available on Arduino Shields that connect to hardware boards. Components such as sensors can be added to 

**Arduino Shield Pinout**

There are several different Arduino pin mappings, however for standardization, a consistent naming is required. The pinout used by the [Board Layer](#board-layer) is shown in the diagram below.

![Arduino Shield Pinout](./images/Arduino-Shield.png "Arduino Shield Pinout")


### RTOS

Provides a CMSIS-RTOS2 compliant RTOS. Various implementation can be used. We have layers for RTX and FreeRTOS.

Connections (provided):
-	CMSIS-RTOS2

RTOS2 might be used by drivers (Board and Shield layer) and also by the application itself.
Note: AWS IoT stack uses FreeRTOS directly however the underlaying CMSIS-Drivers might require CMSIS-RTOS2 API (provided by the CMSIS-RTOS2 FreeRTOS wrapper).

Note: RTOS is initialized and started in the Board layer.


### Socket

Provides an IoT Socket compliant socket. Various implementation can be used. We have layers for: WiFi CMSIS-Driver (with built-in TCP/IP stack), MW-Network over Ethernet (using Ethernet CMSIS-Driver) or WiFi (using WiFi CMSIS-Driver in bypass mode), lwIP over Ethernet (using Ethernet CMSIS-Driver), VSocket (AVH).

Connections (provided):
-	IoT_Socket

Connections (consumed) depend on the implementation. Examples: CMSIS_WiFi, CMSIS_ETH, VSocket, …

Note: Socket is initialized and started from the application by call the following function:
extern int32_t socket_startup (void);

