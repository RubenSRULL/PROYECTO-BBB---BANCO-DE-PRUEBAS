#----- LIBRERIAS -----#
from flask import Flask, render_template, request
import paho.mqtt.client as mqtt
import subprocess
import time
import json
import os

#----- OBJETOS -----#
app = Flask(__name__)

#----- VARIABLES -----#
topicReceive = "esp32/output"
topicSend = "esp32/input"
broker_IP = "10.251.249.63"
broker_PORT = 1883
mosquitto_path = r"C:\Program Files\mosquitto\mosquitto.exe"
mosquitto_conf = r"C:\Program Files\mosquitto\mosquitto.conf"
latest_data = {"velocity": [], "thrust": [], "torque": [], "current": []}
config = None
client = None
porcentaje, velocidad, empuje, par, corriente = [], [], [], [], []

#----- FUNCIONES MQTT -----#
def iniciar_broker():
    try:
        if not any("mosquitto" in p for p in os.popen('tasklist').read().splitlines()):
            subprocess.Popen(
                f'"{mosquitto_path}" -v -c "{mosquitto_conf}"',
                shell=True,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL
            )
            print("Broker Mosquitto iniciado en segundo plano.")
            time.sleep(3)
        else:
            print("Broker Mosquitto ya en ejecución.")
    except Exception as e:
        print(f"Error al iniciar el broker: {e}")


def on_message(client, userdata, msg):
    global porcentaje, velocidad, empuje, par, corriente
    data = json.loads(msg.payload.decode())
    porcentaje.append(round(data.get("%", 0.0), 2))
    velocidad.append(round(data.get("RPM", 0.0), 2))
    empuje.append(round(data.get("Empuje", 0.0), 2))
    par.append(round(data.get("Par", 0.0), 2))
    corriente.append(round(data.get("Intensidad", 0.0), 2))


def iniciar_mqtt():
    global client
    if client is None:
        try:
            iniciar_broker()
            client = mqtt.Client()
            client.on_message = on_message
            client.connect(broker_IP, broker_PORT)
            client.subscribe(topicReceive)
            client.loop_start()
            print("Cliente MQTT conectado.")
        except Exception as e:
            print(f"Error al conectar MQTT: {e}")


#----- RUTA PRINCIPAL -----#
@app.route('/', methods=['GET', 'POST'])
def index():
    global config

    if request.method == 'POST':
        action = request.form.get('action')
        print(f"Acción recibida: {action}")

        if action == 'start':
            config = {
                "propName": request.form.get('propName'),
                "diameter": request.form.get('diameter', type=float),
                "pitch": request.form.get('pitch', type=float),
                "motorName": request.form.get('motorName'),
                "kv": request.form.get('kv', type=float),
                "maxCurrent": request.form.get('maxCurrent', type=float),
                "testName": request.form.get('testName'),
                "vel_init": request.form.get('vel_init', type=float),
                "vel_last": request.form.get('vel_last', type=float),
                "stepTime": request.form.get('stepTime', type=float),
                "step": request.form.get('step', type=int),
                "cicles": request.form.get('cicles', type=int),
                "measure_rpm": 'measure_rpm' in request.form,
                "measure_thrust": 'measure_thrust' in request.form,
                "measure_torque": 'measure_torque' in request.form,
                "measure_current": 'measure_current' in request.form
            }

            print("Configuración enviada con START:")
            iniciar_mqtt()
            payload = json.dumps({"action": action, "data": config})
            client.publish(topicSend, payload)
            print(f"→ MQTT enviado: {payload}")

        elif action in ['tare1', 'tare2', 'calibrate1', 'calibrate2', 'stop']:
            print(f"→ Ejecutando acción simple: {action}")
            iniciar_mqtt()
            payload = json.dumps({"action": action})
            client.publish(topicSend, payload)
            print(f"→ MQTT enviado: {payload}")

    return render_template('index.html')


#----- MAIN -----#
if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
