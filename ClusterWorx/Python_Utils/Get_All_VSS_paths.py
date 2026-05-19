import json
import random
import time
from kuksa_client.grpc import VSSClient, Datapoint, VSSClientError

# Map VSS strings to actual Python casting functions
TYPE_MAP = {
    'boolean': bool,
    'float': float,
    'double': float,
    'int8': int,
    'int16': int,
    'int32': int,
    'uint32': int,
    'string': str
}

def create_datapoint(vss_type):
    print(f"TYPE RECEIVED: ({vss_type.value})")
    selectp = vss_type.value
    """Generates a random value based on the VSS data type."""
    if selectp in ['float', 'double']:
        return round(random.uniform(0.0, 100.0), 2)
    elif selectp in ['uint8', 'int8', 'uint16', 'int16', 'int32', 'uint32']:
        return random.randint(0, 100)
    elif selectp == 'boolean':
        return random.choice([True, False])
    else:
        return "Simulated String"

def traverse_vss(node, path=""):
    with VSSClient('127.0.0.1', 55555) as client:
      # Check if this node is an actual signal (a leaf)
      if node.get('type') in ['sensor', 'actuator', 'attribute']:
        datatype = node.get('datatype', 'unknown')
    
        # Extract limits directly from the JSON keys, NOT the info string
        # We use .get(key, default) so the script doesn't crash if they are missing
        v_min = node.get('min', "No Min")
        v_max = node.get('max', "No Max")      
        #print(f"PATH: {path}")
        #print(f" -> Range: [{v_min} to {v_max}] | Type: {datatype}")
        
        # Extract the data type (float, int32, string, etc.)
        description = node.get('description', 'No description')
        dp = create_datapoint(Datapoint(datatype))
        #v_min = dp.get('min', "No Min")
        #v_max = dp.get('max', "No Max")      
        
        print(f"Value for {path} IS {dp} ")
        path = path.strip()
        datatype = datatype.strip()
        print(f"#{path}|{datatype}|NO_HMI_Property")

           
    # If it has children, dive deeper
    if 'children' in node:
        for child_name, child_node in node['children'].items():
            # Build the path (e.g., Vehicle -> Vehicle.Speed)
            new_path = f"{path}.{child_name}" if path else child_name
            traverse_vss(child_node, new_path)

# Load your VSS JSON file
with open('/home/louis/ClusterWorx/vss_paths.json', 'r') as f:
    vss_data = json.load(f)

# Start traversing from the root (usually 'Vehicle')
traverse_vss(vss_data['Vehicle'], "Vehicle")
