import asyncio
import json
import time
from datetime import datetime, timezone
import threading
import requests
from bleak import BleakClient, BleakScanner

# Debug parameters
debug_logging = True
running_mode = "standalone"
needed_client_numbers = 8

vec_myo_ware_shields_lock = threading.Lock()
shield_list_number = 0
data_received_lock = threading.Lock()
data_received = [False] * 10

last_memory_check = 0
peripheral_interval = 10
interval = 100

got_connect_config = False
enable_connection_params = False

vec_myo_ware_shields = []
vec_myo_ware_clients = []

# UUIDs for the MyoWare service and characteristic
myo_ware_service_uuid = "ec3af789-2154-49f4-a9fc-bc6c88e9e930"
myo_ware_characteristic_uuid = "f3a56edf-8f1e-4533-93bf-5601b2e91308"

# JSON document for storing notifications
on_notify_call_buffer = {}
notify_buffer_lock = threading.Lock()  # Lock for accessing on_notify_call_buffer
serial_pre_values = [0] * 10

# WiFi credentials
ssid = "boluo-wifi"
password = "54448449"

host = "cdn.1f2.net"
port = 80
config_path = "/emg_central_connect_config.json"

class ConnectionParams:
    def __init__(self):
        self.min_interval = None
        self.max_interval = None
        self.latency = None
        self.timeout = None
        self.scan_interval = None
        self.scan_window = None

connection_params = ConnectionParams()

# NTP client setup
async def get_ntp_time():
    while True:
        try:
            response = requests.get("http://worldtimeapi.org/api/timezone/Asia/Hong_Kong")
            if response.status_code == 200:
                data = response.json()
                return datetime.fromisoformat(data['datetime']).timestamp()
        except Exception as e:
            print(f"Error fetching NTP time: {e}")
        await asyncio.sleep(1)

ntp_time = 0
boot_time_millis = 0

def get_formatted_date_time(elapsed_millis):
    raw_time = ntp_time + elapsed_millis / 1000
    timestamp = datetime.fromtimestamp(raw_time, timezone.utc).strftime('%Y-%m-%d %H:%M:%S')
    milliseconds = elapsed_millis % 1000
    return f"{timestamp}.{milliseconds:03d}"

def print_memory_usage():
    import psutil
    memory_info = psutil.virtual_memory()
    print(f"Free memory: {memory_info.available} bytes")

async def notify_callback(sender, data):
    global boot_time_millis
    current_millis = int(time.time() * 1000)
    elapsed_millis = current_millis - boot_time_millis

    try:
        address = sender.obj.peripheral().identifier().UUIDString() 
    except AttributeError:
        print("Error: Could not retrieve the device address from the sender object.")
        return

    if running_mode == "up-to-host":
        # Add data to JSON document
        with notify_buffer_lock:
            peripheral = on_notify_call_buffer.setdefault(address, {})
            values_array = peripheral.setdefault("values", [])

            for i in range(len(data) // 2):
                value = int.from_bytes(data[i*2:i*2+2], byteorder='big')
                adjusted_elapsed_millis = elapsed_millis - (len(data) // 2 - 1 - i) * peripheral_interval
                timestamp = get_formatted_date_time(adjusted_elapsed_millis)
                values_array.append({"timestamp": timestamp, "value": value})

            # Emit the data to the front end
            for entry in values_array:
                def ack_callback():
                    print(f"Data sent successfully: {entry}")

                def error_callback(e):
                    print(f"Error sending data: {e}")

                socketio.emit('emg_data', {
                    'timestamp': entry['timestamp'],
                    'mac': address,
                    'value': entry['value']
                }, callback=ack_callback, error_callback=error_callback)

            print(json.dumps(on_notify_call_buffer))

            # Clear the document for the next notification
            on_notify_call_buffer.clear()
    elif running_mode == "standalone":
        address_index = next((i for i, a in enumerate(vec_myo_ware_shields) if a == address), None)
        if address_index is None:
            print("Address not found in vector")
            return

        for i in range(len(data) // 2):
            serial_pre_values[i] = int.from_bytes(data[i*2:i*2+2], byteorder='big')

        with data_received_lock:
            data_received[address_index] = True

        all_received = all(data_received[:len(vec_myo_ware_shields)])

        if all_received:
            if debug_logging:
                print(f"Received all peripheral data: {serial_pre_values}")
                
            # Reset data_received array for the next batch
            with data_received_lock:
                for i in range(len(vec_myo_ware_shields)):
                    data_received[i] = False

async def on_advertised_device(device, advertisement_data):
    print(f"Detected device: {device.address} with RSSI: {device.rssi}")
    print(f"Advertisement Data: {advertisement_data}")

    if myo_ware_service_uuid in [str(uuid) for uuid in advertisement_data.service_uuids]:
        if debug_logging:
            print(f"Found MyoWare Wireless Shield: {device.address}")

        with vec_myo_ware_shields_lock:
            if device.address not in vec_myo_ware_shields:
                vec_myo_ware_shields.append(device.address)
                global shield_list_number
                shield_list_number += 1
                print(f"ShieldListNumber: {shield_list_number}")
            else:
                if debug_logging:
                    print(f"Duplicate Shield found: {device.address}")

async def fetch_connection_params():
    global got_connect_config, running_mode, enable_connection_params, needed_client_numbers

    connect_config_attempts = 0
    while not got_connect_config and connect_config_attempts <= 5:
        try:
            response = requests.get(f"http://{host}:{port}{config_path}")
            if response.status_code == 200:
                connect_config_json = response.json()

                running_mode = connect_config_json["runningMode"]
                enable_connection_params = connect_config_json["enableconnectionParams"]
                needed_client_numbers = connect_config_json["NeededClientNumbers"]

                connection_params.min_interval = connect_config_json["minInterval"]
                connection_params.max_interval = connect_config_json["maxInterval"]
                connection_params.latency = connect_config_json["latency"]
                connection_params.timeout = connect_config_json["timeout"]
                connection_params.scan_interval = connect_config_json["scanInterval"]
                connection_params.scan_window = connect_config_json["scanWindow"]

                got_connect_config = True
                return True
        except Exception as e:
            print(f"Error fetching connection parameters: {e}")
            connect_config_attempts += 1
            await asyncio.sleep(1)
    return False

async def connect_to_shields():
    global vec_myo_ware_clients
    print("Start Connect to MyoWare Wireless Shields...")

    while len(vec_myo_ware_clients) < needed_client_numbers:
        print(f"NeededClientNumbers: {needed_client_numbers}")
        print(f"Current Client Number: {len(vec_myo_ware_clients)}")

        for address in vec_myo_ware_shields:
            print(f"Current trying to connect address: {address}")

            if address in [client.address for client in vec_myo_ware_clients]:
                continue

            shield_connected = False
            shield_connected_try_times = 0

            while not shield_connected and shield_connected_try_times < 5:
                try:
                    client = BleakClient(address)
                    await client.connect()
                    shield_connected = client.is_connected
                    if shield_connected:
                        vec_myo_ware_clients.append(client)
                        print(f"Connected to {address}")
                        break
                except Exception as e:
                    print(f"Error connecting to {address}: {e}")

                shield_connected_try_times += 1
                await asyncio.sleep(0.2)

            if not shield_connected:
                print(f"Trials to connect to the shield exceed 5 times, give up connect.")

            if shield_connected:
                try:
                    await client.start_notify(myo_ware_characteristic_uuid, notify_callback)
                    print("Subscribed to notifications")
                except Exception as e:
                    print(f"Error subscribing to notifications: {e}")

async def main(socketio_instance):
    global ntp_time, boot_time_millis, socketio
    socketio = socketio_instance  # Use the passed SocketIO instance

    # Fetch NTP time
    ntp_time = await get_ntp_time()
    boot_time_millis = int(time.time() * 1000)
    print(f"NTP time obtained: {datetime.fromtimestamp(ntp_time)}")

    # Fetch ConnectionParams from the URL
    if await fetch_connection_params():
        print("Connection parameters fetched successfully.")
    else:
        print("Failed to fetch connection parameters.")

    # Start scanning for MyoWare Wireless Shields
    if debug_logging:
        print(f"Scanning for MyoWare Wireless Shields: {myo_ware_service_uuid}")

    scanner = BleakScanner(detection_callback=on_advertised_device)
    await scanner.start()
    await asyncio.sleep(20)
    await scanner.stop()

    print("Scan done!")
    print(f"Found {len(vec_myo_ware_shields)} MyoWare Wireless Shields")

    if not vec_myo_ware_shields:
        print("No MyoWare Wireless Shields found!")
        return

    await connect_to_shields()

    while True:
        await asyncio.sleep(10)