import ctypes

try:
    lib = ctypes.WinDLL('./bus_packet.dll')
except:
    print("ERROR: No se ha encontrado la librería 'bus_packet.dll' en el directorio actual.")
    print("¿Tienes el archivo bus_packet.dll en la misma carpeta que este archivo?")
    
BUS_PACKET_BUS_SIZE		        =	127
BUS_PACKET_ECF_SIZE		        =	2
BUS_PACKET_HEADER_SIZE	        =	2
BUS_PACKET_DATA_SIZE	        =	(BUS_PACKET_BUS_SIZE-BUS_PACKET_ECF_SIZE-BUS_PACKET_HEADER_SIZE)
BUS_PACKET_FRAME_SYNC_SIZE      =	4

BUS_PACKET_TYPE_TM		        =	0       # Para Telemetrías
BUS_PACKET_TYPE_TC		        =	1       # Para Telecomandos

BUS_PACKET_ECF_NOT_EXIST        =	0
BUS_PACKET_ECF_EXIST	        =	1       # RECOMENDADO: Poner siempre este


class bus_packet_t(ctypes.Structure):
    _fields_ = [
        ("packet_type", ctypes.c_uint8), 
        ("apid", ctypes.c_uint8), 
        ("ecf_flag", ctypes.c_uint8), 
        ("length", ctypes.c_uint8), 
        ("data", ctypes.c_uint8 * BUS_PACKET_DATA_SIZE), 
        ("ecf", ctypes.c_uint16), 
        ]
    
packet = bus_packet_t()
HAL_StatusTypeDef = ctypes.c_int8


fun_bus_packet_Decode = lib.bus_packet_Decode
fun_bus_packet_Decode.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.POINTER(bus_packet_t)]

fun_bus_packet_EncodePacketize = lib.bus_packet_EncodePacketize
fun_bus_packet_EncodePacketize.argtypes = [ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8, 
                                      ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint8, 
                                      ctypes.POINTER(ctypes.c_uint8)]



def bus_packet_EncodePacketize(packet_type, apid, ecf_flag, data, data_length):
    buffer_out = (ctypes.c_uint8 * BUS_PACKET_BUS_SIZE)()
    
    status = fun_bus_packet_EncodePacketize(packet_type, apid, ecf_flag, 
                                    (ctypes.c_uint8 * len(data))(*data), 
                                    data_length, buffer_out)
    
    return status, list(buffer_out[:data_length+4])  
    
    
def bus_packet_Decode(buffer):
    packet = bus_packet_t()
    status = fun_bus_packet_Decode((ctypes.c_uint8 * len(buffer))(*buffer), ctypes.byref(packet))
    return status, packet


    
    
if __name__ == "__main__":
    
    # Definimos los datos a empaquetar para el Space Packet
    # Las constantes están arriba definidas
    packet_type = BUS_PACKET_TYPE_TC  
    apid = 40  
    ecf_flag = BUS_PACKET_ECF_EXIST  
    data = [100, 1, 12, 234, 34, 5]  
    data_length = len(data)    
    
    # Empaquetamos los datos y los codificamos (comprobar SIEMPRE el status)
    status, buffer_out = bus_packet_EncodePacketize(packet_type, apid, ecf_flag, data,data_length)

    if status == 0:
        print(f"Encoded and Packetized Data: {buffer_out}")
    else:
        print("Error al codificar y paquetizar los datos.")
    
    
    
    # Comprobamos la codificación y paquetización (comprobar SIEMPRE el status)
    status, packet = bus_packet_Decode(buffer_out)
    
    if status == 0:
        print(f"Packet Type: {packet.packet_type}")
        print(f"APID: {packet.apid}")
        print(f"ECF Flag: {packet.ecf_flag}")
        print(f"Length: {packet.length}")
        print(f"Data: {list(packet.data[:packet.length])}")
        print(f"ECF: {packet.ecf}")
    else:
        print("Error al decodificar los datos.")   
    