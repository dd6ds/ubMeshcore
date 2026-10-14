//! ubMeshcore Core Library
//! Bluetooth companion for MeshCore nodes

use meshcore_proto::{packet::{Packet, DecodeError}, consts::*};
use std::sync::{Arc, Mutex};
use std::collections::HashMap;
use std::time::{SystemTime, UNIX_EPOCH};

pub use meshcore_proto::packet::Packet as MeshCorePacket;

/// MeshCore Node representation
#[derive(Debug, Clone)]
pub struct MeshNode {
    pub name: String,
    pub node_id: [u8; PATH_HASH_SIZE as usize],
    pub public_key: [u8; 32],
    pub last_seen: u64,
    pub rssi: i16,
    pub is_repeater: bool,
}

/// Bluetooth device representation
#[derive(Debug, Clone)]
pub struct BluetoothDevice {
    pub address: String,
    pub name: String,
    pub rssi: i16,
    pub connected: bool,
}

/// Channel information
#[derive(Debug, Clone)]
pub struct ChannelInfo {
    pub name: String,
    pub hash: String,
    pub key: Vec<u8>,
    pub message_count: u32,
    pub last_message_time: u64,
}

/// Main application state
pub struct MeshCoreApp {
    nodes: Arc<Mutex<HashMap<String, MeshNode>>>,
    #[allow(dead_code)]
    channels: Arc<Mutex<HashMap<String, ChannelInfo>>>,
    bluetooth_devices: Arc<Mutex<HashMap<String, BluetoothDevice>>>,
    current_node: Arc<Mutex<Option<String>>>,
    current_channel: Arc<Mutex<Option<String>>>,
}

impl MeshCoreApp {
    pub fn new() -> Self {
        MeshCoreApp {
            nodes: Arc::new(Mutex::new(HashMap::new())),
            channels: Arc::new(Mutex::new(HashMap::new())),
            bluetooth_devices: Arc::new(Mutex::new(HashMap::new())),
            current_node: Arc::new(Mutex::new(None)),
            current_channel: Arc::new(Mutex::new(None)),
        }
    }

    pub fn add_node(&self, node: MeshNode) {
        let mut nodes = self.nodes.lock().unwrap();
        nodes.insert(node.name.clone(), node);
    }

    pub fn get_nodes(&self) -> Vec<MeshNode> {
        let nodes = self.nodes.lock().unwrap();
        nodes.values().cloned().collect()
    }

    pub fn add_bluetooth_device(&self, device: BluetoothDevice) {
        let mut devices = self.bluetooth_devices.lock().unwrap();
        devices.insert(device.address.clone(), device);
    }

    pub fn get_bluetooth_devices(&self) -> Vec<BluetoothDevice> {
        let devices = self.bluetooth_devices.lock().unwrap();
        devices.values().cloned().collect()
    }

    pub fn set_current_node(&self, node_name: &str) {
        let mut current = self.current_node.lock().unwrap();
        *current = Some(node_name.to_string());
    }

    pub fn set_current_channel(&self, channel_name: &str) {
        let mut current = self.current_channel.lock().unwrap();
        *current = Some(channel_name.to_string());
    }

    pub fn parse_packet(&self, data: &[u8]) -> Option<MeshCorePacket> {
        Packet::read_from(data).ok()
    }

    pub fn create_channel_message(&self, sender_name: &str, text: &str) -> Vec<u8> {
        let timestamp = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap_or_default()
            .as_secs() as u32;
        
        let mut payload = Vec::new();
        payload.extend_from_slice(&timestamp.to_le_bytes());
        payload.push(0); // txt_type = 0
        payload.extend_from_slice(format!("{}: {}", sender_name, text).as_bytes());
        payload.push(0); // null terminator
        
        let mut packet = Packet::with_types(ROUTE_TYPE_FLOOD, PAYLOAD_TYPE_GRP_TXT);
        let _ = packet.set_payload(&payload);
        
        let mut buffer = vec![0u8; packet.encoded_len()];
        packet.write_to(&mut buffer).map(|_| buffer).unwrap_or_else(|| vec![])
    }

    pub fn decrypt_channel_message(&self, data: &[u8]) -> Option<(String, String, u64)> {
        if let Some(packet) = self.parse_packet(data) {
            let payload = packet.payload();
            if payload.len() > 5 {
                let timestamp = u32::from_le_bytes([payload[0], payload[1], payload[2], payload[3]]);
                let text = String::from_utf8_lossy(&payload[5..]).to_string();
                if let Some((sender, msg)) = text.split_once(": ") {
                    return Some((sender.to_string(), msg.to_string(), timestamp as u64));
                }
            }
        }
        None
    }
}

/// Bluetooth protocol handler
pub struct BluetoothProtocolHandler {
    app: MeshCoreApp,
}

impl BluetoothProtocolHandler {
    pub fn new(app: MeshCoreApp) -> Self {
        BluetoothProtocolHandler { app }
    }

    pub fn process_data(&self, data: &[u8]) -> Option<Vec<u8>> {
        if let Some(packet) = self.app.parse_packet(data) {
            match packet.payload_type() {
                PAYLOAD_TYPE_GRP_TXT => {
                    log::info!("Received channel text message");
                }
                PAYLOAD_TYPE_ADVERT => {
                    log::info!("Received advertisement");
                }
                _ => {
                    log::info!("Received packet type: {:#04x}", packet.payload_type());
                }
            }
            None
        } else {
            log::info!("Received non-MeshCore data");
            None
        }
    }

    pub fn connect_to_node(&self, _address: &str) -> Result<(), String> {
        Ok(())
    }

    pub fn disconnect(&self) -> Result<(), String> {
        Ok(())
    }
}

// FFI exports
use std::ptr;

pub type MeshCoreAppPtr = *mut MeshCoreApp;

#[no_mangle]
pub extern "C" fn meshcore_app_new() -> MeshCoreAppPtr {
    Box::into_raw(Box::new(MeshCoreApp::new()))
}

#[no_mangle]
pub extern "C" fn meshcore_app_free(ptr: MeshCoreAppPtr) {
    unsafe { drop(Box::from_raw(ptr)); }
}

#[no_mangle]
pub extern "C" fn meshcore_process_data(
    app_ptr: MeshCoreAppPtr,
    data_ptr: *const u8,
    data_len: usize,
) -> *mut u8 {
    let app = unsafe { &*app_ptr };
    let data = unsafe { std::slice::from_raw_parts(data_ptr, data_len) };
    
    if let Some((sender, message, _)) = app.decrypt_channel_message(data) {
        let response = format!("{{ \"sender\": \"{}\", \"message\": \"{}\" }}", sender, message);
        let vec = response.into_boxed_str().into_boxed_bytes();
        Box::into_raw(vec) as *mut u8
    } else {
        ptr::null_mut()
    }
}

#[no_mangle]
pub extern "C" fn meshcore_free_ptr(ptr: *mut u8, len: usize) {
    unsafe { drop(Box::from_raw(std::slice::from_raw_parts_mut(ptr, len))); }
}
