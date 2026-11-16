import sqlite3
import os

db_path = os.path.join(os.path.dirname(__file__), '..', 'healthcare.db')
conn = sqlite3.connect(db_path)
c = conn.cursor()

# Set clean initial stock
c.execute("UPDATE hospital_resources SET occupied_quantity = 2 WHERE resource_type = 'icu_beds'")
c.execute("UPDATE hospital_resources SET occupied_quantity = 2 WHERE resource_type = 'oxygen_cylinders'")
c.execute("UPDATE hospital_resources SET occupied_quantity = 1 WHERE resource_type = 'blood_units'")
c.execute("UPDATE hospital_resources SET occupied_quantity = 1 WHERE resource_type = 'ventilators'")
c.execute("UPDATE hospital_resources SET occupied_quantity = 5 WHERE resource_type = 'general_beds'")
c.execute("UPDATE hospital_resources SET occupied_quantity = 2 WHERE resource_type = 'doctors_duty'")

conn.commit()
print("Resources reset successfully:")
for row in c.execute("SELECT resource_type, total_quantity, occupied_quantity FROM hospital_resources"):
    print(f"  {row[0]}: Total={row[1]}, Occupied={row[2]}, Available={row[1]-row[2]}")
conn.close()
