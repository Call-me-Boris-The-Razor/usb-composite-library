"""
PlatformIO Extra Script
Автоматический переход в DFU через 1200 bps touch перед прошивкой
"""

Import("env")
import serial
import time

def before_upload(source, target, env):
    """Отправляет 1200 bps touch для перехода в DFU bootloader"""
    
    upload_port = env.get("UPLOAD_PORT")
    
    # Если порт не указан, пробуем найти COM порт
    if not upload_port:
        import serial.tools.list_ports
        ports = list(serial.tools.list_ports.comports())
        for port in ports:
            if "USB" in port.description or "Serial" in port.description:
                upload_port = port.device
                break
    
    if not upload_port:
        print("No COM port found, assuming already in DFU mode")
        return
    
    print(f"Triggering DFU mode via 1200 bps touch on {upload_port}...")
    
    try:
        # Открываем порт на 1200 bps
        ser = serial.Serial(upload_port, 1200)
        ser.dtr = True
        time.sleep(0.1)
        ser.dtr = False
        ser.close()
        
        # Ждём пока плата перейдёт в DFU (увеличенная задержка)
        print("Waiting for DFU bootloader...")
        time.sleep(4)
        
    except Exception as e:
        print(f"Could not trigger DFU: {e}")
        print("Please enter DFU mode manually (BOOT + RESET)")

env.AddPreAction("upload", before_upload)
