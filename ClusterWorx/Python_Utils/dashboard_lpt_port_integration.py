import parallel
import time
from kuksa_client.grpc import VSSClient, Datapoint

TRIP_PATH = 'Vehicle.TripMeterReading'
TRIP_INCREMENT = 0.1

def update_dashboard_and_hardware():
    try:
        # 1. Initialize Hardware
        p = parallel.Parallel(0)
        
        with VSSClient('127.0.0.1', 55555) as client:
            print("🔍 Querying Databroker for current mileage...")
            
            # 2. Get the current value from the Broker
            response = client.get_current_values([TRIP_PATH])
            
            # Extract the value, or use 0.0 if the path is empty/new
            current_trip = response[TRIP_PATH].value
            if current_trip is None:
                current_trip = 12.3 # Fallback if no data exists
            
            print(f"📊 Startup Sync: Starting at {current_trip} km")
            print("🚀 Hardware Link Active. Pulsing LPT...")
            
            while True:
                # 3. Physical Pulse (Arduino sees P2=1)
                p.setData(0x01)
                
                # 4. Update the Value
                current_trip = round(current_trip + TRIP_INCREMENT, 1)
                
                # 5. Push back to Kuksa
                client.set_current_values({
                    TRIP_PATH: Datapoint(current_trip)
                })
                
                print(f"Distance: {current_trip} km | LED: ON")
                time.sleep(0.5)
                
                # Reset Pulse
                p.setData(0x00)
                time.sleep(0.5)

    except KeyboardInterrupt:
        p.setData(0x00)
        print("\n🛑 Simulation Stopped.")
    except Exception as e:
        print(f"❌ Error during sync: {e}")

if __name__ == "__main__":
    update_dashboard_and_hardware()
