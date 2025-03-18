## Imports
# Import SQLite3
import sqlite3

# Import YAML
import yaml

# Additional Imports
import os, requests

## Information Tracking
# Database Information:     ble_data.db
# Table Information:        ble_data
# Header Information:       MAC, Name, Appearance, FirstSeen, LastSeen, TxPower, RSSI, CurrentLatitude, CurrentLongitude, AltitudeMeters, AccuracyMeters, AddrType, ManufData

## Variables

# Database Table Name
db_table_name = "ble_data"

# UUID APIs
comp_uuid_url_api = 'https://api.bitbucket.org/2.0/repositories/bluetooth-SIG/public/src/main/assigned_numbers/company_identifiers/company_identifiers.yaml'
appear_uuid_url_api = 'https://api.bitbucket.org/2.0/repositories/bluetooth-SIG/public/src/main/assigned_numbers/core/appearance_values.yaml'
address_type_uuid_url_api = 'https://api.bitbucket.org/2.0/repositories/bluetooth-SIG/public/src/main/assigned_numbers/'


# Static Strings
#   - Not needed since not outputing anything?

# Debug Bit
dbg = 0

## Functions

# Function for Obtaining a YAML Content Return from a Provided URL API
def grab_online_record(url_string):
    # Attempt to download the remote page
    url_response = requests.get(url_string)
    # Check for issues
    try:
        url_response.raise_for_status()
    except Exception as exc:
        print("[!] There was a problem:\t{0}".format(exc))
    # Debugging
    if dbg != 0:
        print("Length of Response:\t{0}".format(len(url_response.text)))
        print("Headers:\t{0}".format(url_response.headers))
        print("Encoding:\t{0}".format(url_response.encoding))
        #print("Text:\t{0}".format(url_response.text))
        print("JSON:\t{0}".format(url_response.json))
        print("Content:\t{0}".format(url_response.content))
    
    # Convert the content
    content = url_response.content.decode("utf-8")
    # Load the YAML file
    yaml_content = yaml.safe_load(content)
    #yaml_content = yaml.safe_load(content, Loader=yaml.BaseLoader)
    
    if dbg != 0:
        print("Content Convert:\t{0}".format(content))
        print("YAML:\t{0}".format(yaml_content))

    # Return the YAML Conect
    return yaml_content


## Code

# Connect to the Bletsubushi SQLite3 database
conn = sqlite3.connect('ble.db')
cursor = conn.cursor()
# Configure the text factory for aiding with decoding
conn.text_factory = lambda b: b.decode(errors = 'ignore')

# Function to extract and summarize data based on specified columns
def extract_and_summarize_info(column_name):
    query = f'SELECT {column_name}, COUNT(DISTINCT MAC) FROM {db_table_name} GROUP BY {column_name}'
    #print(query)
    cursor.execute(query)
    data = cursor.fetchall()
    summary_stats = {}
    for row in data:
        summary_stats[row[0]] = row[1]
    return summary_stats

# Function to find addresses occurring more than once
def find_duplicate_addresses():
    query = f'SELECT MAC, COUNT(*) FROM {db_table_name} GROUP BY MAC HAVING COUNT(*) > 1'
    cursor.execute(query)
    duplicates = cursor.fetchall()
    duplicate_addresses = [address[0] for address in duplicates]
    return duplicate_addresses


#test = cursor.execute("SELECT Appearance FROM ble_data")
#print(test)

# Summarize data for specified columns
uniq_addr__appearance = extract_and_summarize_info('Appearance')
uniq_addr__addr_type = extract_and_summarize_info('AddrType')
uniq_addr__name = extract_and_summarize_info('Name')
uniq_addr__manf = extract_and_summarize_info('ManufData')

# Find addresses occurring more than once
duplicate_addresses = find_duplicate_addresses()

# Print summary statistics
print("Number of Unique Bluetooth Addresses per Unique Appearance:")
print(uniq_addr__appearance)

print("\nNumber of Unique Bluetooth Addresses per Unique Address Type:")
print(uniq_addr__addr_type)

print("\nNumber of Unique Bluetooth Addresses per Unique Name:")
print(uniq_addr__name)

print("\nNumber of Unique Bluetooth Addresses per Unique Manufacturer Data:")
print(uniq_addr__manf)

# Close the database connection
conn.close()


### Reference Information

## Advertisement Flags
'''
case ESP_BLE_AD_TYPE_FLAG:  // 0x01
case ESP_BLE_AD_TYPE_16SRV_PART:  // 0x02
case ESP_BLE_AD_TYPE_16SRV_CMPL:  // 0x03
case ESP_BLE_AD_TYPE_32SRV_PART:  // 0x04
case ESP_BLE_AD_TYPE_32SRV_CMPL:  // 0x05
case ESP_BLE_AD_TYPE_128SRV_PART:  // 0x06
case ESP_BLE_AD_TYPE_128SRV_CMPL:  // 0x07
case ESP_BLE_AD_TYPE_NAME_SHORT:  // 0x08
case ESP_BLE_AD_TYPE_NAME_CMPL:  // 0x09
case ESP_BLE_AD_TYPE_TX_PWR:  // 0x0a
case ESP_BLE_AD_TYPE_DEV_CLASS:  // 0x0b
case ESP_BLE_AD_TYPE_SM_TK:  // 0x10
case ESP_BLE_AD_TYPE_SM_OOB_FLAG:  // 0x11
case ESP_BLE_AD_TYPE_INT_RANGE:  // 0x12
case ESP_BLE_AD_TYPE_SOL_SRV_UUID:  // 0x14
case ESP_BLE_AD_TYPE_128SOL_SRV_UUID:  // 0x15
case ESP_BLE_AD_TYPE_SERVICE_DATA:  // 0x16
case ESP_BLE_AD_TYPE_PUBLIC_TARGET:  // 0x17
case ESP_BLE_AD_TYPE_RANDOM_TARGET:  // 0x18
case ESP_BLE_AD_TYPE_APPEARANCE:  // 0x19
case ESP_BLE_AD_TYPE_ADV_INT:  // 0x1a
case ESP_BLE_AD_TYPE_32SOL_SRV_UUID: return "ESP_BLE_AD_TYPE_32SOL_SRV_UUID";
case ESP_BLE_AD_TYPE_32SERVICE_DATA:  // 0x20
case ESP_BLE_AD_TYPE_128SERVICE_DATA:  // 0x21
case ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE:  // 0xff
'''
# TODO: Add dissection of advertisement information + capture of advertisement with the BLE War Driver

## Appearance Flags
appear_data = grab_online_record(appear_uuid_url_api)
#print(appear_data)

## Address Type
#address_type_data = grab_online_record(address_type_uuid_url_api)
# Definitions Lifted from other resources
BLE_ADDR_TYPE_PUBLIC = 0x00
BLE_ADDR_TYPE_RANDOM = 0x01
BLE_ADDR_TYPE_RPA_PUBLIC = 0x02
BLE_ADDR_TYPE_RPA_RANDOM = 0x03

## Manufacturer Data
manuf_data_id = grab_online_record(comp_uuid_url_api)
#print(manuf_data_id)

# Function to find matching UUID/value for a company ID
def find_uuid_string(comp_id, yaml_content):
    # Cycle through UUIDs
    for item in yaml_content['company_identifiers']:
        # Check UUID if matches
        if item['value'] == comp_id:
            return item['name']
    return "-=Unknown=-"

# Function for Printing Appearance Type Translation
#   -> Nota Bene: The appearance ID is the first THREE HEX bytes, and the LAST HEX byte is for additional sub-category
def find_appear_string(appear_id, yaml_content):
    # Create variables for categoy and subcategory search
    category_id = (int(appear_id) & 0xFFF0)>>4
    subcategory_id = (int(appear_id) & 0x000F)
    # Variables for holding Category and Subcategory names
    category_name = "-=???=-"
    subcategory_name = "-=SUB-???=-"
    # Debugging
    if dbg != 0:
        test = (int(appear_id) & 0xFFF0)>>4     # Shifting to the right a nibble (4 bits)
        print("\tTest:\t{0}\t\t{1}".format(test, test.to_bytes(2, 'big')))
        test2 = (int(appear_id) & 0x000F)       # Bit masking for only the last byte
        print("\tTest2:\t{0}\t\t{1}".format(test2, test2.to_bytes(2, 'big')))
    # Cycle through Categories
    for category_entry in yaml_content['appearance_values']:
        # Check if Category matches
        if dbg != 0:
            print(category_entry)
            print(category_entry['category'])
        if category_entry['category'] == category_id:
            if dbg != 0:
                print("I FOUND IT")
                print("\tCategory:\t{0}\t\t{1}".format(category_entry['category'], category_entry['category'].to_bytes(2, 'big')))
                print("\tName:\t{0}".format(category_entry['name']))
            #return category_entry['name']
            category_name = category_entry['name']
            # Check for a subcategory
            if 'subcategory' in category_entry:
                for subcategory_entry in category_entry['subcategory']:
                    if subcategory_entry['value'] == subcategory_id:
                        print("FOUND SUB-CATEGORRY")
                        subcategory_name = subcategory_entry['name']
            else:
                subcategory_name = ""
    # Check for Default Values
    if category_name == "-=???=-":
        category_name = "0x{0:03x}".format(category_id)
    if subcategory_name == "-=SUB-???=-":
        subcategory_name = "0x{0:02x}".format(subcategory_id)
    # Return the Category and Subcategory names
    return category_name, subcategory_name

# Function for Printing Appearance ID Summary
def print_appearance_data(unique_data_dictionary):
    print("\nAppearance ID Summary:")
    for appear_id, count in unique_data_dictionary.items():
        if dbg != 0:
            print("Appear ID:\t{0}\t\t{1}".format(appear_id, int(appear_id).to_bytes(2, 'big')))
            print("\tHex 2B :\t{0}".format(int(appear_id).to_bytes(2, 'big'))) # <--- Most likely the correct configuration
        category_name, subcategory_name = find_appear_string(appear_id, appear_data)
        if dbg != 0:
            print("\tAppear ID-SubID:\t{0}-{1}\t\t-\t\t{2}\t:\t{3}".format(((int(appear_id) & 0xFFF0)>>4), ((int(appear_id) & 0x000F)), category_name, subcategory_name))
        print("\t{0}\t\t:\t\t{1} {2}".format(count, category_name, subcategory_name))

# Function for Printing Device Name Summary
def print_device_name_data(unique_data_dictionary):
    print("\nDevice Name Summary:")
    for device_name, count in unique_data_dictionary.items():
        print("\t{0}\t\t:\t\t{1}".format(count, device_name))

# Function for Printing Address Type Summary
def print_addr_type_data(unique_data_dictionary):
    print("\nAddress Type Summary:")
    for address_type, count in unique_data_dictionary.items():
        # Variables
        address_type_name = "-=UNKNOWN=-"
        # Debugging
        if dbg != 0:
            print("Addr_T:\t{0}\t\tType:\t{1}".format(address_type, type(address_type)))
        # Convert str to int for comparison
        address_type = int(address_type)
        # Check the Address Type
        if address_type == BLE_ADDR_TYPE_PUBLIC:
            address_type_name = "Public"
        elif address_type == BLE_ADDR_TYPE_RANDOM:
            address_type_name = "Random"
        elif address_type == BLE_ADDR_TYPE_RPA_PUBLIC:
            address_type_name = "RPA Public"
        elif address_type == BLE_ADDR_TYPE_RPA_RANDOM:
            address_type_name = "RPA Random"
        else:
            print("Address Type is Unknown")
        # Print the Data
        print("\t{0}\t\t:\t\t{1}".format(count, address_type_name))

# Function for Printing Manufacturer Data Summaries
def print_manuf_data(unique_data_dictionary):
    print("\nManufacturer Data Summary:")
    # Cycle through the Manufacturer Data and Associated Count
    for manuf_data, count in unique_data_dictionary.items():
        # Print Out Information
        #print("\t0x{0}\t:\t{1}".format(manuf_data.encode('utf-8').hex(), count))
        print("\t{0}\t\t:\t\t0x{1}".format(count, manuf_data.encode('utf-8').hex()))

print_appearance_data(uniq_addr__appearance)
print_device_name_data(uniq_addr__name)
print_addr_type_data(uniq_addr__addr_type)
print_manuf_data(uniq_addr__manf)
