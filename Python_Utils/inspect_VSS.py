import inspect
from kuksa_client.grpc import VSSClient

# Filter to show only functions/methods
methods = inspect.getmembers(VSSClient, predicate=inspect.isfunction)
for name, _ in methods:
    if not name.startswith('_'):
        print(f"Available method: {name}")
