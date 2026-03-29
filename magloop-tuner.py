import subprocess
import re
import requests
import logging

logger = logging.getLogger(__name__)
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s"
)

pattern = re.compile(r"set freq (\d+)")

UPCONVERTER_FREQ_HZ = 48_000_480 # Upconverter frequency

# Calibration data (position, frequency)
CALIBRATION = [
        (0, 4_659_000),
        (100, 4_927_500),
        (200, 5_255_250),
        (300, 5_611_250),
        (400, 6_013_750),
        (500, 6_468_750),
        (600, 7_003_000),
        (700, 7_601_250),
        (800, 8_307_750),
        (900, 9_118_000),
        (1000, 10_170_750),
        (1100, 11_470_500),
        (1200, 13_260_000),
        (1300, 15_965_500),
        (1400, 21_345_000),
    ]

antenna_controller = "192.168.100.140"

cmd = [
    "/usr/bin/stdbuf", "-oL", "-eL", # Use stdbuf to buffer STDOUT and STDERR from rtl_tcp
    "/usr/bin/rtl_tcp", "-a", "0.0.0.0",
    "-g", "15", # Set gain
    "-s", "1000000", # Set sample rate
    "-b", "60", # Number of buffers
    "-f", "7100000" # Tune to 40 meter band
]

def get_actuator_position(freq):
    if freq <= CALIBRATION[0][1]:
        return CALIBRATION[0][0]
    if freq >= CALIBRATION[-1][1]:
        return CALIBRATION[-1][0]

    for i in range(len(CALIBRATION) - 1):
        pos1, f1 = CALIBRATION[i]
        pos2, f2 = CALIBRATION[i + 1]

        if f1 <= freq <= f2:
            ratio = (freq - f1) / (f2 - f1)
            position = pos1 + ratio * (pos2 - pos1)
            return int(round(position))

    return None

process = subprocess.Popen(
    cmd,
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,
    text=True
)

try:
    for line in iter(process.stdout.readline, ''):
        retcode = process.poll()
        if retcode is not None:
            logger.error(f"child process exited with code {retcode}")
            break

        logger.info(line.strip())
        match = pattern.search(line)
        if match:
            frequency = int(match.group(1))
            logger.debug(f"Frequency set command {frequency}")

            rf_freq = frequency - UPCONVERTER_FREQ_HZ
            if rf_freq <= 0:
                logger.warning("Invalid frequency")
                continue

            position = get_actuator_position(rf_freq)
            if position is not None:
                url = f"http://{antenna_controller}/setPosition?abs={position}"
                logger.debug(f"URL: {url}")
                try:
                    response = requests.get(url, timeout=5)
                    logger.debug(f"HTTP {response.status_code}")
                except requests.RequestException as e:
                    logger.error(f"Request error: {e}")
            else:
                logger.error(f"Unable to get position for frequency {frequency}")

except KeyboardInterrupt:
    logger.info("Keyboard interrupt")

finally:
    process.terminate()
    process.wait()
