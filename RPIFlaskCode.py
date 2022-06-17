from flask import Flask, render_template
from datetime import datetime
import serial
import threading
import statistics
import time
from pushbullet import Pushbullet


app = Flask(__name__)
ser = serial.Serial('/dev/ttyACM0', 9600, timeout=1)
ser.flush()

motionDetections = []
temperatures = []
lastTempDrop = datetime.min
dataLock = threading.Lock()
pb = Pushbullet("{{pushbullet api key}}")


@app.route('/')
def display_data():
    dataLock.acquire()

    now = datetime.now()
    averageTempWeek = "None" if len(temperatures) == 0 else round(statistics.mean([x[1] for x in filter(lambda temp: ((now - temp[0]).days < 7), temperatures)]), 2)
    averageTempDay = "None" if len(temperatures) == 0 else round(statistics.mean([x[1] for x in filter(lambda temp: ((now - temp[0]).days < 1), temperatures)]), 2)
    motionDetectionsWeek = len(list(filter(lambda motion: ((now - motion).days < 7), motionDetections)))
    motionDetectionsDay = len(list(filter(lambda motion: ((now - motion).days < 1), motionDetections)))
    lastTempDropStr = "Never" if lastTempDrop == datetime.min else lastTempDrop.strftime('%Y-%m-%d %H:%M:%S')
    currentTemp = "None" if len(temperatures) == 0 else temperatures[-1][1]
    currentTempTime = "Never" if len(temperatures) == 0 else temperatures[-1][0].strftime('%Y-%m-%d %H:%M:%S')

    dataLock.release()

    return render_template('index.html',
                           averageTempWeek=averageTempWeek,
                           averageTempDay=averageTempDay,
                           motionDetectionsWeek=motionDetectionsWeek,
                           motionDetectionsDay=motionDetectionsDay,
                           lastTempDrop=lastTempDropStr,
                           currentTemp=currentTemp,
                           currentTempTime=currentTempTime
    )


def send_temp_drop():
    pb.push_note("Temperature Drop!", "Temperature drop recorded!")

def read_input_continuous():
    while True:
        if ser.in_waiting > 0:
            try:
                rawserial = ser.readline()
                line = rawserial.decode('utf-8').strip('\r\n')
            except Exception: continue

            dataLock.acquire()
            if line == 'motion':
                print('Got motion!')
                motionDetections.append(datetime.now())
                if (datetime.now() - motionDetections[0]).days > 7:
                    motionDetections.remove(0)
            elif line == 'TemperatureDrop':
                print('Got temp drop!')
                lastTempDrop = datetime.now()
                send_temp_drop()
            elif line.startswith('temperature;'):
                temp = float(line.split(';')[1])
                print('Got temperature:', temp)
                temperatures.append((datetime.now(), temp))
                if (datetime.now() - temperatures[0][0]).days > 7:
                    temperatures.remove(0)
            else:
                print('Unknown input')
                print(line)
            dataLock.release()

        time.sleep(0.01)


if __name__ == '__main__':
    inputThread = threading.Thread(target=read_input_continuous)
    inputThread.start()
    app.run(host='0.0.0.0', port=80, debug=True)
    inputThread.join()
