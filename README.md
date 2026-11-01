# Sistema de Control PI y Telemetría IoT con ESP32

Sistema embebido basado en el microcontrolador **ESP32** para la adquisición multicanal de variables termohigrométricas, regulación de temperatura por ventilación mecánica mediante un lazo de control **PI** (Proporcional-Integral), lógica de protección por histéresis mediante relé, registro local en almacenamiento **MicroSD** con estampado de tiempo **NTP** y publicación periódica de telemetría a la nube en **ThingSpeak**.

---

## Tabla de Contenidos
- [Características Principales](#-características-principales)
- [Diagrama de Conexiones (Pinout)](#-diagrama-de-conexiones-pinout)
- [Dependencias y Librerías](#-dependencias-y-librerías)
- [Estructura del Registro Local (SD)](#-estructura-del-registro-local-sd)
- [Lógica de Control](#-lógica-de-control)
- [Instalación y Configuración](#-instalación-y-configuración)
- [Configuración de Seguridad](#-configuración-de-seguridad)

---

## Características Principales

* **Monitoreo Triaxial de Clima:** 3 sensores digitales DHT22 distribuidos para lectura independiente de temperatura y humedad relativa.
* **Control de Posicionamiento PI:** Modulación angular de servomotor para compuerta de ventilación en función del error térmico.
* **Actuación de Seguridad:** Control de relé por umbrales críticos de temperatura ($> 65^\circ\text{C}$) o humedad ($> 35\%$).
* **Interfaz de Usuario Local:** Pantalla OLED SSD1306 (128x64 px) con métricas en tiempo real, diagnósticos de conectividad y estado de periféricos.
* **Datalloger Local:** Generación y cierre diario de archivos `.csv` en tarjeta MicroSD con nombres dinámicos basados en fecha POSIX (`/AAAAMMDD.csv`).
* **Sincronización Horaria Global:** Protocolo NTP sobre UDP para sellado de tiempo de precisión.
* **Telemetría Cloud:** Publicación mediante peticiones HTTP REST a ThingSpeak (8 canales analíticos).

---

## Diagrama de Conexiones (Pinout)

| Componente / Periférico | Pin ESP32 (GPIO) | Protocolo / Modo | Función |
| :--- | :--- | :--- | :--- |
| **Sensor DHT22 #1** | `GPIO 32` | Digital One-Wire | Sensor principal de proceso |
| **Sensor DHT22 #2** | `GPIO 33` | Digital One-Wire | Sensor de cámara/colector |
| **Sensor DHT22 #3** | `GPIO 25` | Digital One-Wire | Sensor ambiente de referencia |
| **Servomotor** | `GPIO 13` | PWM | Compuerta de flujo / escape |
| **Módulo Relé** | `GPIO 12` | Digital Output | Extractor o elemento calefactor |
| **Pantalla OLED (SDA)** | `GPIO 21` | $I^2C$ Data | Transmisión de frames gráficos |
| **Pantalla OLED (SCL)** | `GPIO 22` | $I^2C$ Clock | Reloj de bus $I^2C$ (`0x3C`) |
| **MicroSD (MOSI)** | `GPIO 23` | SPI | Bus de datos hacia tarjeta SD |
| **MicroSD (MISO)** | `GPIO 19` | SPI | Bus de datos desde tarjeta SD |
| **MicroSD (SCK)** | `GPIO 18` | SPI | Reloj de bus SPI |
| **MicroSD (CS)** | `GPIO 5` | SPI (Chip Select) | Selección de esclavo SD |

---

##  Dependencias y Librerías

Para compilar este firmware en **Arduino IDE**, instala las siguientes bibliotecas desde el **Gestor de Bibliotecas** (`Ctrl + Shift + I`):

1. **Adafruit SSD1306** (por *Adafruit*) - Controlador para pantalla OLED monocromática.
2. **Adafruit GFX Library** (por *Adafruit*) - Manejador de primitivas gráficas y tipografías.
3. **DHT sensor library** (por *Adafruit*) - Lectura de sensores DHT11 / DHT22 / AM2302.
4. **Adafruit Unified Sensor** (por *Adafruit*) - Capa de abstracción base de Adafruit.
5. **ESP32Servo** (por *Kevin Harrington*) - Control PWM de servos compatible con ESP32.
6. **NTPClient** (por *Fabrice Weinberg*) - Cliente ligero para consultar servidores de tiempo NTP.

### Librerías nativas incluidas con el Core ESP32 (no requieren descarga externa):
* `Wire.h` (comunicación $I^2C$)
* `WiFi.h` y `WiFiUdp.h` (conectividad inalámbrica 802.11 b/g/n)
* `SD.h` (sistema de archivos FAT para tarjetas SD)
* `time.h` (funciones estándar de tiempo de biblioteca C)

> **Para usuarios de PlatformIO:** Puedes copiar este bloque en tu `platformio.ini`:
> ```ini
> [env:esp32dev]
> platform = espressif32
> board = esp32dev
> framework = arduino
> monitor_speed = 115200
> lib_deps =
>     adafruit/Adafruit SSD1306 @ ^2.5.7
>     adafruit/Adafruit GFX Library @ ^1.11.5
>     adafruit/DHT sensor library @ ^1.4.4
>     adafruit/Adafruit Unified Sensor @ ^1.1.9
>     madhephaestus/ESP32Servo @ ^1.1.0
>     arduino-libraries/NTPClient @ ^3.2.1
> ```

---

##  Estructura del Registro Local (SD)

Cada registro se guarda en formato CSV append-only con una fila cada ciclo de ejecución (aprox. 5 segundos):

```csv
Fecha,Hora,Temp1,Hum1,Temp2,Hum2,Temp3,Hum3,GiroServo,EstadoRele
2026/04/10,14:22:15,58.3,24.5,62.1,20.1,30.4,46.0,115,0
```

---

##  Lógica de Control

### 1. Algoritmo PI (Ventilación)
El control regula la temperatura del proceso modificando la apertura angular del servomotor:

$$e(t) = T_{\text{setpoint}} - T_{\text{proceso}}$$

$$u(t) = K_p \cdot e(t) + K_i \int_0^t e(\tau) \, d\tau$$

* **Setpoint:** $60.0^\circ\text{C}$
* **Ganancia Proporcional ($K_p$):** $2.06$
* **Ganancia Integral ($K_i$):** $0.0692$
* **Saturación:** Acotado dinámicamente entre $1^\circ$ y $175^\circ$.

### 2. Condición del Relé
El relé conmuta a estado activo (`HIGH` / `1`) si se cumple cualquiera de las siguientes condiciones:
* Humedad del sensor 2 $> 35.0\%$
* Temperatura del sensor 1 $> 65.0^\circ\text{C}$

---

##  Configuración de Seguridad

Para evitar subir información sensible a repositorios públicos, gestiona las claves mediante un archivo de encabezado `secrets.h`.

1. Crea un archivo llamado `secrets.h` en la misma carpeta que el código fuente:
```cpp
#pragma once

#define SECRET_SSID "TU_RED_WIFI"
#define SECRET_PASSWORD "TU_CONTRASENA"
#define SECRET_THINGSPEAK_API_KEY "TU_API_KEY_THINGSPEAK"
```

2. Añádelo al archivo `.gitignore`:
```gitignore
secrets.h
```

---

##  Puesta en Marcha

1. Clonar el repositorio:
   ```bash
   git clone https://github.com/tu-usuario/tu-repositorio.git
   cd tu-repositorio
   ```
2. Abrir el proyecto en Arduino IDE o PlatformIO.
3. Asegurar que las librerías mencionadas estén instaladas.
4. Crear el archivo `secrets.h` con tus credenciales de red y ThingSpeak.
5. Seleccionar la placa **ESP32 Dev Module** y el puerto COM asignado.
6. Compilar y cargar el firmware.