#!/usr/bin/env python3
"""
AutoShieldX - Machine Learning Dataset Generator
Generates realistic automotive CAN network telemetry datasets containing
both normal driving cycles and 5 laboratory attack patterns.

Features included:
1. message_frequency_hz : Frequency of arriving CAN messages (Hz)
2. can_id               : CAN Identifier (decimal, e.g. 256 for 0x100)
3. speed_kmh            : Vehicle speed in km/h
4. rpm                  : Engine RPM
5. temperature_c        : Engine coolant temperature (Celsius)
6. vibration_g          : SW-420 sensor acceleration (G)
7. steering_deg         : Steering wheel angle (degrees)
8. wheel_speed_kmh      : Wheel encoder speed (km/h)
9. sequence_delta       : Difference from previous sequence number
10. time_delta_ms       : Inter-frame arrival time in milliseconds
Label                   : 0 = NORMAL, 1 = ATTACK
Attack_Category         : NORMAL, INJECTION, REPLAY, DOS, FUZZY, FALSE_DATA
"""

import csv
import math
import random
import os

def generate_normal_driving(num_samples=3000):
    rows = []
    speed = 0.0
    target_speed = 65.0
    rpm = 850.0
    temp = 25.0
    seq = 1000
    
    for i in range(num_samples):
        # Drive cycle transitions
        if i % 400 == 0:
            target_speed = random.choice([0.0, 45.0, 70.0, 110.0, 50.0])
        
        # Smooth longitudinal acceleration
        err = target_speed - speed
        speed += max(-3.0, min(3.0, err * 0.25)) + random.gauss(0, 0.2)
        speed = max(0.0, speed)
        
        # RPM gear calculation
        gear = 80.0 if speed < 20 else (50.0 if speed < 40 else (36.0 if speed < 65 else (26.0 if speed < 95 else 20.0)))
        rpm = max(800.0, 850.0 + (speed * gear) + random.gauss(0, 20.0))
        
        # Thermal curve
        if temp < 88.0:
            temp += 0.08
        else:
            temp = 90.0 + math.sin(i * 0.05) * 1.5 + random.gauss(0, 0.1)
            
        # SW-420 Road vibration simulation
        vib = 0.05 + (speed / 250.0) * 0.35 + abs(random.gauss(0, 0.05))
        
        # Gentle highway steering oscillation
        steering = math.sin(i * 0.1) * 6.0 + random.gauss(0, 0.5)
        
        # Wheel speed with minor tire slip
        wheel_speed = max(0.0, speed + random.gauss(0, 0.3))
        
        # Normal 20 Hz transmission rate (50ms interval)
        freq = 20.0 + random.gauss(0, 1.0)
        time_delta_ms = 50.0 + random.gauss(0, 2.0)
        
        can_id = 256 if (i % 2 == 0) else 512 # 0x100 Powertrain or 0x200 Dynamics
        seq += 1
        seq_delta = 1.0
        
        rows.append([
            round(freq, 2), can_id, round(speed, 2), round(rpm, 2),
            round(temp, 2), round(vib, 4), round(steering, 2),
            round(wheel_speed, 2), seq_delta, round(time_delta_ms, 2),
            0, "NORMAL"
        ])
    return rows

def generate_attack_samples(num_per_attack=300):
    rows = []
    
    # 1. Injection / Spoofing
    for i in range(num_per_attack):
        freq = random.uniform(18.0, 25.0)
        can_id = random.choice([256, 512, 1638, 1365]) # Includes unauthorized rogue IDs (0x666, 0x555)
        speed = random.uniform(40.0, 120.0)
        rpm = random.uniform(1500.0, 4500.0)
        temp = random.uniform(70.0, 95.0)
        vib = random.uniform(0.1, 0.6)
        steering = random.uniform(-15.0, 15.0)
        wheel_speed = speed + random.gauss(0, 1.0)
        seq_delta = random.choice([0.0, -5.0, 150.0, 500.0]) # Abnormal sequence jump
        time_delta_ms = random.uniform(30.0, 60.0)
        rows.append([round(freq, 2), can_id, round(speed, 2), round(rpm, 2),
                     round(temp, 2), round(vib, 4), round(steering, 2),
                     round(wheel_speed, 2), seq_delta, round(time_delta_ms, 2),
                     1, "INJECTION"])
                     
    # 2. Replay Attack
    for i in range(num_per_attack):
        freq = random.uniform(19.0, 22.0)
        can_id = random.choice([256, 512])
        speed = random.uniform(60.0, 80.0)
        rpm = random.uniform(2200.0, 3100.0)
        temp = random.uniform(85.0, 90.0)
        vib = random.uniform(0.1, 0.4)
        steering = random.uniform(-5.0, 5.0)
        wheel_speed = speed
        seq_delta = random.choice([-10.0, -25.0, -1.0, 0.0]) # Stale/decreasing sequence
        time_delta_ms = random.uniform(45.0, 55.0)
        rows.append([round(freq, 2), can_id, round(speed, 2), round(rpm, 2),
                     round(temp, 2), round(vib, 4), round(steering, 2),
                     round(wheel_speed, 2), seq_delta, round(time_delta_ms, 2),
                     1, "REPLAY"])

    # 3. DoS / Flooding
    for i in range(num_per_attack):
        freq = random.uniform(120.0, 450.0) # Extreme flooding frequency (> 80 Hz)
        can_id = 256 # Dominant Powertrain CAN ID
        speed = random.uniform(60.0, 90.0)
        rpm = random.uniform(2500.0, 3500.0)
        temp = random.uniform(80.0, 95.0)
        vib = random.uniform(0.1, 0.5)
        steering = 0.0
        wheel_speed = speed
        seq_delta = 1.0
        time_delta_ms = random.uniform(1.5, 6.0) # Burst intervals of 2-5ms
        rows.append([round(freq, 2), can_id, round(speed, 2), round(rpm, 2),
                     round(temp, 2), round(vib, 4), round(steering, 2),
                     round(wheel_speed, 2), seq_delta, round(time_delta_ms, 2),
                     1, "DOS"])

    # 4. Fuzzy / Random Traffic
    for i in range(num_per_attack):
        freq = random.uniform(5.0, 80.0)
        can_id = random.randint(1, 2047) # Random 11-bit ID
        speed = random.uniform(-20.0, 350.0) # Fuzzed speeds
        rpm = random.uniform(-50.0, 14000.0)
        temp = random.uniform(-30.0, 180.0)
        vib = random.uniform(0.0, 4.0)
        steering = random.uniform(-400.0, 400.0)
        wheel_speed = random.uniform(-20.0, 350.0)
        seq_delta = random.choice([0.0, 1.0, 45.0, -12.0, 300.0])
        time_delta_ms = random.uniform(5.0, 150.0)
        rows.append([round(freq, 2), can_id, round(speed, 2), round(rpm, 2),
                     round(temp, 2), round(vib, 4), round(steering, 2),
                     round(wheel_speed, 2), seq_delta, round(time_delta_ms, 2),
                     1, "FUZZY"])

    # 5. False Data / Signal Manipulation
    for i in range(num_per_attack):
        freq = random.uniform(19.0, 22.0)
        can_id = random.choice([256, 512])
        # Inject physically impossible combinations
        scenario = random.choice([1, 2, 3, 4])
        if scenario == 1:
            speed = random.uniform(280.0, 380.0) # Out-of-bounds speed
            wheel_speed = speed
            rpm = random.uniform(5000.0, 7000.0)
            temp = 90.0
        elif scenario == 2:
            speed = 100.0
            wheel_speed = speed
            rpm = random.uniform(9500.0, 13000.0) # Out-of-bounds engine RPM
            temp = 92.0
        elif scenario == 3:
            speed = 80.0
            wheel_speed = speed
            rpm = 2800.0
            temp = random.uniform(140.0, 195.0) # Out-of-bounds temperature
        else:
            speed = 150.0
            wheel_speed = 10.0 # Physical inconsistency: |150 - 10| = 140 km/h
            rpm = 4000.0
            temp = 89.0
            
        vib = random.uniform(0.2, 2.5)
        steering = random.uniform(-10.0, 10.0)
        seq_delta = 1.0
        time_delta_ms = random.uniform(48.0, 52.0)
        rows.append([round(freq, 2), can_id, round(speed, 2), round(rpm, 2),
                     round(temp, 2), round(vib, 4), round(steering, 2),
                     round(wheel_speed, 2), seq_delta, round(time_delta_ms, 2),
                     1, "FALSE_DATA"])
                     
    return rows

def main():
    os.makedirs("ml/dataset", exist_ok=True)
    out_file = "ml/dataset/vehicle_can_telemetry_dataset.csv"
    
    headers = [
        "message_frequency_hz", "can_id", "speed_kmh", "rpm",
        "temperature_c", "vibration_g", "steering_deg",
        "wheel_speed_kmh", "sequence_delta", "time_delta_ms",
        "label", "attack_category"
    ]
    
    print("[Dataset Generator] Generating normal vehicle driving patterns...")
    normal_data = generate_normal_driving(num_samples=4000)
    
    print("[Dataset Generator] Generating 5 laboratory attack patterns...")
    attack_data = generate_attack_samples(num_per_attack=350)
    
    all_data = normal_data + attack_data
    random.seed(42)
    random.shuffle(all_data)
    
    with open(out_file, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(headers)
        writer.writerows(all_data)
        
    print(f"[Dataset Generator] Successfully generated {len(all_data)} records.")
    print(f"  - Normal Samples : {len(normal_data)}")
    print(f"  - Attack Samples : {len(attack_data)}")
    print(f"  - Saved to       : {out_file}\n")

if __name__ == "__main__":
    main()
