#!/bin/python3

import sqlite3
from flask import Flask, request, jsonify, render_template_string, redirect, url_for
from datetime import datetime
import pytz

app = Flask(__name__)

DATABASE = 'data/data_geiger_counter.db'
API_KEY = 'your_api_key_here'  # Replace with your actual API key

def get_db():
    conn = sqlite3.connect(DATABASE)
    conn.row_factory = sqlite3.Row
    return conn

def init_db():
    with get_db() as conn:
        conn.execute('''
            CREATE TABLE IF NOT EXISTS data (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                year INTEGER,
                month INTEGER,
                day INTEGER,
                hour INTEGER,
                minute INTEGER,
                second INTEGER,
                tick INTEGER,
                duration INTEGER,
                timestamp TEXT
            )
        ''')

        conn.execute('''
            CREATE TABLE IF NOT EXISTS status (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                year INTEGER,
                month INTEGER,
                day INTEGER,
                hour INTEGER,
                minute INTEGER,
                second INTEGER,
                solar_panel_voltage FLOAT,
                solar_panel_boosted_voltage FLOAT,
                battery_voltage FLOAT,
                timestamp TEXT
            )
        ''')
        conn.commit()

ota_switch_state = False
last_ota_time = None

@app.route('/')
def index():
    return redirect(url_for('get_metrics'))

@app.route('/otaswitch')
def ota_switch():
    global last_page_access_time
    last_page_access_time = datetime.now()
    return render_template_string('''
        <!DOCTYPE html>
        <html>
        <head>
            <title>OTA Toggle</title>
        </head>
        <body>
            <h1>OTA Switch: {{ 'ON' if ota_switch_state else 'OFF' }}</h1>
            <p>Last OTA time: {{ last_ota_time if last_ota_time else "Never" }}</p>
            <form action="/toggleotaswitch" method="post">
                <button type="submit">Toggle OTA</button>
            </form>
        </body>
        </html>
    ''', ota_switch_state=ota_switch_state, last_ota_time=last_ota_time, last_page_access_time=last_page_access_time)

@app.route('/otaswitchstate')
def get_ota_switch_state():
    global ota_switch_state
    return str(ota_switch_state), 200

@app.route('/toggleotaswitch', methods=['POST'])
def toggle_ota():
    global ota_switch_state, last_ota_time
    ota_switch_state = not ota_switch_state
    return redirect(url_for('ota_switch'))

@app.route("/changeotaswitch", methods=['GET'])
def change_ota_switch_state():
    global ota_switch_state
    state = request.args.get('state')
    if not state:
        return "Missing state parameter", 400
    if (state == "True" or state == "true" or state =="1"):
        ota_switch_state = True
    else:
        ota_switch_state = False
    return f"OTA State Changed to {ota_switch_state}",200

@app.route("/savedata", methods=['POST'])
def save_data():
    print("save_data")
    api_key = request.headers.get('x-api-key')
    if api_key != API_KEY:
        return "Unauthorized", 401
    
    payload = request.json
    print(payload)
    status = payload['status']
    solar_panel_voltage = status['solar_panel_voltage']
    solar_panel_boosted_voltage = status['solar_panel_boosted_voltage']
    battery_voltage = status['battery_voltage']

    data = payload['data']
    year = data['year']
    month = data['month']
    day = data['day']
    hour = data['hour']
    minute = data['minute']
    second = data['second']
    ticks = data['ticks']
    duration = data['duration']
    print(duration)

    timestamp = datetime.now(pytz.timezone('Asia/Tokyo')).isoformat()

    with get_db() as conn:
        conn.execute('''
            INSERT INTO data (year, month, day, hour, minute, second, tick, duration, timestamp)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        ''', (year, month, day, hour, minute, second, ticks, duration, timestamp))

        conn.execute('''
            INSERT INTO status (year, month, day, hour, minute, second, solar_panel_voltage, solar_panel_boosted_voltage, battery_voltage, timestamp)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ''', (year, month, day, hour, minute, second, solar_panel_voltage, solar_panel_boosted_voltage, battery_voltage, timestamp))
        conn.commit()

    return "Data saved successfully", 200

@app.route("/alldata", methods=['GET'])
def all_data():
    with get_db() as conn:
        cursor = conn.execute('SELECT * FROM data')
        rows = cursor.fetchall()
        return jsonify([dict(row) for row in rows]), 200

@app.route("/allstatus", methods=['GET'])
def all_status():
    with get_db() as conn:
        cursor = conn.execute('SELECT * FROM status')
        rows = cursor.fetchall()
        return jsonify([dict(row) for row in rows]), 200


@app.route("/data", methods=['GET'])
def data_by_hour():
    hour = request.args.get('hour')
    if not hour:
        return "Missing hour parameter", 400

    with get_db() as conn:
        cursor = conn.execute('SELECT * FROM data WHERE hour = ?', (hour,))
        rows = cursor.fetchall()
        return jsonify([dict(row) for row in rows]), 200

@app.route('/metrics', methods=['GET'])
def get_metrics():
    with get_db() as conn:
        # Fetch the latest data
        data_cursor = conn.execute('''
            SELECT * FROM data ORDER BY id DESC LIMIT 1
        ''')
        data_row = data_cursor.fetchone()
        
        # Fetch the latest status
        status_cursor = conn.execute('''
            SELECT * FROM status ORDER BY id DESC LIMIT 1
        ''')
        status_row = status_cursor.fetchone()
        
        # Check if data and status are available
        if not data_row or not status_row:
            return "No data available", 404

        # Format data
        response_str = (
          f'Year{{id="GeigerCounter1"}} {data_row["year"]}\n'
          f'Month{{id="GeigerCounter1"}} {data_row["month"]}\n'
          f'Day{{id="GeigerCounter1"}} {data_row["day"]}\n'
          f'Hour{{id="GeigerCounter1"}} {data_row["hour"]}\n'
          f'Minute{{id="GeigerCounter1"}} {data_row["minute"]}\n'
          f'Second{{id="GeigerCounter1"}} {data_row["second"]}\n'
          f'Duration{{id="GeigerCounter1"}} {data_row["duration"]}\n'
          f'Tick{{id="GeigerCounter1"}} {data_row["tick"]}\n'
          f'Solar_Panel_Voltage{{id="GeigerCounter1"}} {status_row["solar_panel_voltage"]}\n'
          f'Solar_Panel_Boosted_Voltage{{id="GeigerCounter1"}} {status_row["solar_panel_boosted_voltage"]}\n'
          f'Battery_Voltage{{id="GeigerCounter1"}} {status_row["battery_voltage"]}\n'
          )

        response = app.response_class(
          response=response_str,
          status=200,
          mimetype='text/plain')
        return response


if __name__ == "__main__":
    init_db()
    app.run(host="0.0.0.0", port=8000)
