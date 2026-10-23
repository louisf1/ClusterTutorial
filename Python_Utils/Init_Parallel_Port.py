import parallel
import time

def cross_port_test():
    try:
        # Initialize BOTH ports found in your dmesg
        p0 = parallel.Parallel(0) # Onboard
        p1 = parallel.Parallel(1) # PCI Card
        
        print("--- STARTING DUAL PORT TEST ---")
        print("Note: If Pin 4 is 3V, we are looking for it to drop to 0V now.")
        
        while True:
            # 1. Turn EVERYTHING OFF
            print("Setting BOTH ports to 0V...")
            p0.setData(0x00)
            p1.setData(0x00)
            time.sleep(2)
            
            # 2. Turn EVERYTHING ON
            print("Setting BOTH ports to 5V (All data pins 2-9 should jump)...")
            p0.setData(0xFF)
            p1.setData(0xFF)
            time.sleep(2)
            
    except Exception as e:
        print(f"❌ Error: {e}")

if __name__ == "__main__":
    cross_port_test()
