# LinkPoint — Hotspot personal + compartición de archivos

Aplicación de escritorio para **Windows 10/11** escrita en **Qt 6 + C++** (kit **MinGW 64-bit** o MSVC), que crea un punto de acceso WiFi para conectarte a tu PC desde el móvil (Android, ES File Explorer, navegador, etc.), **sin necesidad de que Windows detecte internet**.

## Funcionalidades

- **Doble motor de hotspot**, elegido automáticamente según el adaptador:
  1. `Hosted Network` (`netsh wlan hostednetwork`): no exige internet, soportado por drivers clásicos.
  2. `Mobile Hotspot` (API WinRT de Windows, vía PowerShell): la misma que usa la interfaz de Windows.
- **Servidor HTTP de archivos integrado**: comparte una carpeta con listado navegable; se abre desde ES File Explorer (HTTP), un navegador o cualquier cliente HTTP.
- UI compacta, oscura y moderna; estado del hotspot y del servidor en vivo.
- IP del equipo y dirección URL con botones de copiado.
- Botón de **Escritorio remoto (RDP)** hacia la propia PC.
- Los datos (SSID, contraseña, puerto, carpeta) se recuerdan entre sesiones.
- **Detección de permisos**: si la app no se ejecuta como administrador, muestra una UI con botón para reiniciarse elevada (UAC). El arranque del AP requiere privilegios.

## Requisitos

- Windows 10 o 11.
- Adaptador WiFi físico con soporte de punto de acceso (WiFi Direct / SoftAP).
- Qt 6.5+ (funciona desde 6.2) y CMake. Lo más sencillo: instalar Qt desde [qt.io](https://www.qt.io/download) con el kit **MinGW 11.2.0 64-bit** incluido. **No necesitas MSVC.**

## Compilar

### Opción A — Qt Creator (recomendada)

1. Abre Qt Creator.
2. `File → Open File or Project…` → selecciona `CMakeLists.txt`.
3. Elige el kit **MinGW 64-bit** (o MSVC 64-bit si lo prefieres).
4. Compila y ejecuta (`Ctrl+R`). Si no se está ejecutando con permisos de administrador, la app muestra un aviso con el botón **Reiniciar como administrador** (aparecerá la pantalla UAC de Windows).

> Si Qt Creator no puede lanzar el `.exe` con el error "insufficient permissions", abre **Projects → Run** y marca la casilla **Run as administrator** (o ejecuta el `.exe` desde el Explorador con clic derecho → "Ejecutar como administrador").

### Opción B — Línea de comandos

```bat
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:\Qt\6.7.2\mingw_64
cmake --build build --config Release
```

### Desplegar (DLLs)

Con el compilador MinGW en el PATH:

```bat
windeployqt build\LinkPoint.exe
```

Copia la carpeta completa donde quieras. Los scripts de PowerShell van embebidos en el ejecutable (recurso Qt), así que solo necesitas el `.exe` y sus DLLs.

## Uso

1. Abre LinkPoint (acepta UAC).
2. Escribe el **nombre de red (SSID)** y una **contraseña de 8 a 63 caracteres**.
3. (Opcional) Elige la **carpeta** a compartir y el **puerto** (por defecto 8080).
4. Pulsa **Iniciar Hotspot**. El servidor de archivos arranca solo.
5. En el teléfono: conéctate al WiFi creado y abre **ES File Explorer → HTTP** (o un navegador) con la dirección mostrada, p. ej. `http://192.168.137.1:8080`.

> El hotspot funciona aunque la PC **no tenga internet**: el teléfono obtiene una IP local (192.168.137.x) y accede a la PC en `192.168.137.1` para transferir archivos.

## Motor de hotspot: cómo funciona

Al abrir la app se detecta la capacidad del adaptador:

- Si `netsh wlan show drivers` reporta `Hosted network support: Yes`, se usa `netsh` (funciona sin internet).
- Si no, se usa la API WinRT `NetworkOperatorTetheringManager` mediante un script PowerShell embebido (la misma API del "Punto de acceso móvil" de Windows). Esta vía puede requerir un perfil de red activo; si Windows la bloquea, la app lo reporta en el registro.

Si ambas fallan, el adaptador no soporta punto de acceso software (raro en portátiles modernos; revisa `netsh wlan show drivers`).

## Diagnóstico "Sin motor disponible"

La app reporta la razón exacta de cada motor en el registro:

- `Hosted Network (netsh): No` — el driver del adaptador no expone la red hospedada clásica (común en Windows 11).
- `Mobile Hotspot (WinRT): …` — uno de estos motivos:
  - `sin perfil de red activo`: la PC no tiene ninguna conexión; conéctala a una red (Ethernet o WiFi) y reintenta.
  - `hardware sin soporte de punto de acceso`: el adaptador no soporta SoftAP/WiFi Direct; comprueba en *Configuración → Red e Internet → Punto de acceso móvil* si el interruptor está disponible, actualiza el driver (Intel/Realtek) o usa un adaptador USB WiFi compatible.
  - `deshabilitado por directiva` / `no disponible en esta edición` / `bloqueado por el operador`: limitación de política o sistema.

Puedes verificar manualmente con:

```bat
netsh wlan show drivers
```

## Estructura

```
LinkPoint/
├── CMakeLists.txt
├── resources/
│   ├── app.manifest          # Elevación bajo demanda + DPI
│   ├── app.rc
│   ├── styles.qss            # Tema oscuro
│   ├── resources.qrc
│   └── scripts/*.ps1         # Puente PowerShell → API WinRT
└── src/
    ├── main.cpp              # Detecta permisos y pide elevación
    ├── elevation.*           # Chequeo de token y relanzado como administrador
    ├── mainwindow.*          # UI
    ├── hotspotcontroller.*   # Arranque/parada/detección del AP
    └── httpserver.*          # Servidor HTTP de archivos
```
