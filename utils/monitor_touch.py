import serial
import time
import sys

def main():
    print("==================================================")
    print(" Monitor de Touch CYD (COM3)")
    print(" Abre puerto sin reiniciar el ESP32.")
    print(" Presiona la pantalla táctil en distintos puntos:")
    print("  1. Esquina Superior Izquierda (MODO:)")
    print("  2. Esquina Inferior Derecha (SOS)")
    print("  3. Boton BALIZA (Inferior Izquierda)")
    print("==================================================")
    try:
        s = serial.Serial()
        s.port = 'COM3'
        s.baudrate = 115200
        s.dtr = False
        s.rts = False
        s.timeout = 0.2
        s.open()
    except Exception as e:
        print(f"Error al abrir COM3: {e}")
        return

    # Enviar comando de diagnostico
    s.write(b't\n')
    t0 = time.time()
    last_diag = time.time()
    try:
        while time.time() - t0 < 15:
            line = s.readline()
            if line:
                try:
                    txt = line.decode('latin-1', errors='replace').strip()
                    if txt:
                        print(f"[{time.strftime('%H:%M:%S')}] {txt}")
                except:
                    pass
            # Cada 3 segundos enviar 't' si no hay actividad
            if time.time() - last_diag > 3.0:
                s.write(b't\n')
                last_diag = time.time()
    except KeyboardInterrupt:
        pass
    finally:
        s.close()
        print("Fin de captura.")

if __name__ == '__main__':
    main()
