import time
import random
from kuksa_client.grpc import VSSClient, Datapoint

# Configuration: Update to match your Sandbox or Master Node IP
SERVER_IP = "127.0.0.1" 
SERVER_PORT = 55555

def simulate_dashboard():
    with VSSClient(SERVER_IP, SERVER_PORT) as client:
        print(f"Connected to Kuksa Databroker at {SERVER_IP}:{SERVER_PORT}")
        
        # Initial values
        speed = 0.0
        rpm = 10
        Temp = 40
        total_kms = 12500.456  # Testing your Math.round() logic
        trip_kms = 12.345      # Testing your toFixed(1) logic
        fuel_liters = 405.0
        Gear=1
        
        while True:
            # 1. Simulate Gauges (Speed & RPM)
            speed = min(120.0, speed + random.uniform(-2, 5)) if speed < 100 else speed + random.uniform(-5, 2)
           
            if (rpm > 1100): 
                rpm = 10
            else:
                rpm = rpm + 10
                            
            if (Temp > 150): 
                Temp = 40
            else:
                Temp = Temp + 10
                
            if (Gear > 5): 
                Gear = 1
            else:
                Gear = Gear + 1

            if (fuel_liters > 1000): 
                fuel_liters = 10
            else:
                fuel_liters = fuel_liters + 100
                                
            # 2. Simulate Distance (Total & Trip)
            # Incrementing slightly each loop
            total_kms += 0.0123 
            trip_kms += 0.0123
            
            # 3. Simulate Indicators (Left blinker test)
            left_indicator = bool(random.getrandbits(1))
            right_indicator = bool(random.getrandbits(1))
            ParkingBrake_IsEngaged = bool(random.getrandbits(1))
            ParkingBrake_IsEnabled = bool(random.getrandbits(1))
            Transmission_CurrentGear = bool(random.getrandbits(1))
            ABS_IsEnabled = bool(random.getrandbits(1))
            ABS_IsEngaged = bool(random.getrandbits(1))
            IsEngaged = bool(random.getrandbits(1))
            Lights_Beam_High_IsOn = bool(random.getrandbits(1))
            Lights_Beam_Low_IsOn = bool(random.getrandbits(1))
            Lights_Parking_IsOn = bool(random.getrandbits(1))
            Lights_Running_IsOn = bool(random.getrandbits(1))
            IsAirConditioningActive = bool(random.getrandbits(1))
            ParkingBrake_IsEngaged = bool(random.getrandbits(1))
            ParkingLight_IsEngaged = bool(random.getrandbits(1))            
            Row1_PassengerSide_IsBelted = bool(random.getrandbits(1))
            Row1_Driver_IsBelted = bool(random.getrandbits(1))
            Row1_Middle_IsBelted = bool(random.getrandbits(1))
            Row2_PassengerSide_IsBelted = bool(random.getrandbits(1))
            Row2_Driver_IsBelted = bool(random.getrandbits(1))
            Row2_Middle_IsBelted = bool(random.getrandbits(1))
            IsServiceDue = bool(random.getrandbits(1))
            Is_Lights_Parking_IsOn = bool(random.getrandbits(1))
            IsFuelLevelLow = bool(random.getrandbits(1))
            
            # 4. Update Databroker
# Wrap each value in a Datapoint object
#Vehicle.Powertrain.FuelSystem.AverageConsumption=15
#Vehicle.Powertrain.FuelSystem.IsFuelLevelLow|boolean|fuelLevel
#Vehicle.Powertrain.FuelSystem.Range|uint32|displayRange
#Vehicle.Powertrain.FuelSystem.AverageConsumption|float|displayKpl

            updates = {
                "Vehicle.Speed": Datapoint(float(speed)),
                "Vehicle.OBD.EngineSpeed": Datapoint(float(rpm)),
                "Vehicle.OBD.CoolantTemperature": Datapoint(Temp),
                "Vehicle.OBD.FuelLevel": Datapoint(float(fuel_liters)),
                "Vehicle.Powertrain.FuelSystem.Range": Datapoint(float(fuel_liters)),
                "Vehicle.Powertrain.FuelSystem.AverageConsumption": Datapoint(float(fuel_liters/2)), 
                "Vehicle.Service.IsServiceDue": Datapoint(IsServiceDue),
                "Vehicle.TraveledDistanceSinceStart": Datapoint(float(total_kms)),
                "Vehicle.TripMeterReading": Datapoint(float(trip_kms)),
                "Vehicle.Powertrain.Transmission.CurrentGear": Datapoint(Gear),
                "Vehicle.ADAS.ABS.IsEnabled": Datapoint(ABS_IsEnabled),
                "Vehicle.ADAS.ABS.IsEngaged":  Datapoint(ABS_IsEngaged),

                "Vehicle.Body.Lights.Running.IsOn": Datapoint(Lights_Running_IsOn),
                "Vehicle.Body.Lights.Beam.High.IsOn": Datapoint(Lights_Beam_High_IsOn),
                "Vehicle.Body.Lights.Beam.Low.IsOn": Datapoint(Lights_Beam_Low_IsOn),
                "Vehicle.Powertrain.FuelSystem.IsFuelLevelLow": Datapoint(IsFuelLevelLow),
                "Vehicle.Body.Lights.Parking.IsOn": Datapoint(Is_Lights_Parking_IsOn),
                "Vehicle.Chassis.ParkingBrake.IsEngaged": Datapoint(ParkingBrake_IsEngaged),

                "Vehicle.Body.Lights.DirectionIndicator.Left.IsSignaling": Datapoint(left_indicator),
                "Vehicle.Body.Lights.DirectionIndicator.Right.IsSignaling": Datapoint(right_indicator),

                "Vehicle.Cabin.HVAC.IsAirConditioningActive": Datapoint(IsAirConditioningActive),

                "Vehicle.Cabin.Seat.Row1.DriverSide.IsBelted": Datapoint(Row1_Driver_IsBelted),
                "Vehicle.Cabin.Seat.Row1.Middle.IsBelted": Datapoint(Row1_Middle_IsBelted),
                "Vehicle.Cabin.Seat.Row1.PassengerSide.IsBelted": Datapoint(Row1_PassengerSide_IsBelted),
                "Vehicle.Cabin.Seat.Row2.DriverSide.IsBelted": Datapoint(Row2_Driver_IsBelted),
                "Vehicle.Cabin.Seat.Row2.Middle.IsBelted": Datapoint(Row2_Middle_IsBelted),
                "Vehicle.Cabin.Seat.Row2.PassengerSide.IsBelted": Datapoint(Row2_PassengerSide_IsBelted)
               }
            
            # The client expects a dictionary of path: Datapoint
            client.set_current_values(updates)
            
            for path, value in updates.items():
                client.set_current_values({path: value})
            
            print(f"Update -> Speed: {speed:.1f} km/h | ODO: {total_kms:.3f} | Trip: {trip_kms:.3f}")
            time.sleep(0.5)

if __name__ == "__main__":
    try:
        simulate_dashboard()
    except KeyboardInterrupt:
        print("\nSimulation stopped.")
