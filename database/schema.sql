-- MeshCore SQLite Database Schema
-- Version: 1.0

-- Enable WAL mode for better performance
PRAGMA journal_mode=WAL;
PRAGMA synchronous=NORMAL;

-- Contacts table
CREATE TABLE IF NOT EXISTS contacts (
    id TEXT PRIMARY KEY,
    name TEXT,
    long_name TEXT,
    node_id TEXT UNIQUE,
    hardware_id TEXT,
    path TEXT,
    path_hash_size INTEGER DEFAULT 0,
    last_seen DATETIME,
    last_advert DATETIME,
    last_message DATETIME,
    is_favorite INTEGER DEFAULT 0,
    is_muted INTEGER DEFAULT 0,
    color TEXT,
    latitude REAL,
    longitude REAL,
    elevation REAL,
    position_timestamp DATETIME,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Indexes for contacts
CREATE INDEX IF NOT EXISTS idx_contacts_name ON contacts(name);
CREATE INDEX IF NOT EXISTS idx_contacts_node_id ON contacts(node_id);
CREATE INDEX IF NOT EXISTS idx_contacts_last_seen ON contacts(last_seen);
CREATE INDEX IF NOT EXISTS idx_contacts_favorite ON contacts(is_favorite);
CREATE INDEX IF NOT EXISTS idx_contacts_position ON contacts(latitude, longitude);

-- Contact metadata table
CREATE TABLE IF NOT EXISTS contact_metadata (
    contact_id TEXT NOT NULL,
    key TEXT NOT NULL,
    value TEXT,
    PRIMARY KEY (contact_id, key),
    FOREIGN KEY (contact_id) REFERENCES contacts(id) ON DELETE CASCADE
);

-- Contact capabilities table
CREATE TABLE IF NOT EXISTS contact_capabilities (
    contact_id TEXT NOT NULL,
    capability TEXT NOT NULL,
    value INTEGER DEFAULT 0,
    PRIMARY KEY (contact_id, capability),
    FOREIGN KEY (contact_id) REFERENCES contacts(id) ON DELETE CASCADE
);

-- Contact regions table
CREATE TABLE IF NOT EXISTS contact_regions (
    contact_id TEXT NOT NULL,
    region_name TEXT NOT NULL,
    PRIMARY KEY (contact_id, region_name),
    FOREIGN KEY (contact_id) REFERENCES contacts(id) ON DELETE CASCADE
);

-- Direct messages table
CREATE TABLE IF NOT EXISTS direct_messages (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    sender_id TEXT NOT NULL,
    receiver_id TEXT NOT NULL,
    content TEXT NOT NULL,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    is_outgoing INTEGER DEFAULT 0,
    is_read INTEGER DEFAULT 0,
    is_delivered INTEGER DEFAULT 0,
    is_failed INTEGER DEFAULT 0,
    retry_count INTEGER DEFAULT 0,
    next_retry DATETIME,
    status TEXT DEFAULT 'sent',
    region_scopes TEXT,
    FOREIGN KEY (sender_id) REFERENCES contacts(id),
    FOREIGN KEY (receiver_id) REFERENCES contacts(id)
);

-- Indexes for direct messages
CREATE INDEX IF NOT EXISTS idx_direct_messages_sender ON direct_messages(sender_id);
CREATE INDEX IF NOT EXISTS idx_direct_messages_receiver ON direct_messages(receiver_id);
CREATE INDEX IF NOT EXISTS idx_direct_messages_timestamp ON direct_messages(timestamp);
CREATE INDEX IF NOT EXISTS idx_direct_messages_conversation ON direct_messages(sender_id, receiver_id);
CREATE INDEX IF NOT EXISTS idx_direct_messages_unread ON direct_messages(receiver_id, is_read, timestamp);
CREATE INDEX IF NOT EXISTS idx_direct_messages_status ON direct_messages(status);

-- Conversations view (for efficient conversation queries)
CREATE VIEW IF NOT EXISTS conversations AS
SELECT 
    CASE 
        WHEN is_outgoing = 1 THEN receiver_id 
        ELSE sender_id 
    END AS contact_id,
    MAX(timestamp) AS last_message_time,
    COUNT(*) AS message_count,
    SUM(CASE WHEN is_read = 0 AND is_outgoing = 0 THEN 1 ELSE 0 END) AS unread_count
FROM direct_messages
GROUP BY contact_id
ORDER BY last_message_time DESC;

-- Channels table
CREATE TABLE IF NOT EXISTS channels (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    description TEXT,
    creator_id TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    is_favorite INTEGER DEFAULT 0,
    is_muted INTEGER DEFAULT 0,
    color TEXT,
    default_region TEXT,
    message_retention_hours INTEGER DEFAULT 0,
    notification_settings TEXT
);

-- Indexes for channels
CREATE INDEX IF NOT EXISTS idx_channels_name ON channels(name);
CREATE INDEX IF NOT EXISTS idx_channels_creator ON channels(creator_id);
CREATE INDEX IF NOT EXISTS idx_channels_favorite ON channels(is_favorite);

-- Channel messages table
CREATE TABLE IF NOT EXISTS channel_messages (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    channel_id TEXT NOT NULL,
    sender_id TEXT NOT NULL,
    content TEXT NOT NULL,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    region_scopes TEXT,
    is_read INTEGER DEFAULT 0,
    status TEXT DEFAULT 'received',
    FOREIGN KEY (channel_id) REFERENCES channels(id) ON DELETE CASCADE,
    FOREIGN KEY (sender_id) REFERENCES contacts(id)
);

-- Indexes for channel messages
CREATE INDEX IF NOT EXISTS idx_channel_messages_channel ON channel_messages(channel_id);
CREATE INDEX IF NOT EXISTS idx_channel_messages_sender ON channel_messages(sender_id);
CREATE INDEX IF NOT EXISTS idx_channel_messages_timestamp ON channel_messages(timestamp);
CREATE INDEX IF NOT EXISTS idx_channel_messages_unread ON channel_messages(channel_id, is_read, timestamp);

-- Channel participants table
CREATE TABLE IF NOT EXISTS channel_participants (
    channel_id TEXT NOT NULL,
    contact_id TEXT NOT NULL,
    joined_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    last_read DATETIME,
    permissions TEXT,
    PRIMARY KEY (channel_id, contact_id),
    FOREIGN KEY (channel_id) REFERENCES channels(id) ON DELETE CASCADE,
    FOREIGN KEY (contact_id) REFERENCES contacts(id) ON DELETE CASCADE
);

-- Regions table
CREATE TABLE IF NOT EXISTS regions (
    name TEXT PRIMARY KEY,
    display_name TEXT,
    description TEXT,
    flood_enabled INTEGER DEFAULT 0,
    scope TEXT UNIQUE,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Indexes for regions
CREATE INDEX IF NOT EXISTS idx_regions_scope ON regions(scope);
CREATE INDEX IF NOT EXISTS idx_regions_flood ON regions(flood_enabled);

-- Channel regions table (many-to-many)
CREATE TABLE IF NOT EXISTS channel_regions (
    channel_id TEXT NOT NULL,
    region_name TEXT NOT NULL,
    PRIMARY KEY (channel_id, region_name),
    FOREIGN KEY (channel_id) REFERENCES channels(id) ON DELETE CASCADE,
    FOREIGN KEY (region_name) REFERENCES regions(name) ON DELETE CASCADE
);

-- Repeaters table
CREATE TABLE IF NOT EXISTS repeaters (
    id TEXT PRIMARY KEY,
    name TEXT,
    node_id TEXT UNIQUE,
    hardware_id TEXT,
    frequency REAL,
    latitude REAL,
    longitude REAL,
    elevation REAL,
    position_timestamp DATETIME,
    signal_strength INTEGER,
    last_seen DATETIME,
    last_contacted DATETIME,
    is_favorite INTEGER DEFAULT 0,
    path_hash_size INTEGER DEFAULT 2,
    default_region TEXT,
    capabilities TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Indexes for repeaters
CREATE INDEX IF NOT EXISTS idx_repeaters_name ON repeaters(name);
CREATE INDEX IF NOT EXISTS idx_repeaters_node_id ON repeaters(node_id);
CREATE INDEX IF NOT EXISTS idx_repeaters_position ON repeaters(latitude, longitude);
CREATE INDEX IF NOT EXISTS idx_repeaters_favorite ON repeaters(is_favorite);

-- Repeater neighbors table
CREATE TABLE IF NOT EXISTS repeater_neighbors (
    repeater_id TEXT NOT NULL,
    neighbor_id TEXT NOT NULL,
    neighbor_name TEXT,
    neighbor_node_id TEXT,
    signal_strength INTEGER,
    distance REAL,
    last_seen DATETIME,
    PRIMARY KEY (repeater_id, neighbor_id),
    FOREIGN KEY (repeater_id) REFERENCES repeaters(id) ON DELETE CASCADE
);

-- Settings table
CREATE TABLE IF NOT EXISTS settings (
    key TEXT PRIMARY KEY,
    value TEXT
);

-- Insert default settings
INSERT OR IGNORE INTO settings (key, value) VALUES ('app_version', '1.0');
INSERT OR IGNORE INTO settings (key, value) VALUES ('default_path_hash_size', '2');
INSERT OR IGNORE INTO settings (key, value) VALUES ('auto_reconnect', 'true');
INSERT OR IGNORE INTO settings (key, value) VALUES ('show_region_names', 'true');
INSERT OR IGNORE INTO settings (key, value) VALUES ('keep_screen_on', 'false');
INSERT OR IGNORE INTO settings (key, value) VALUES ('jump_to_oldest_unread', 'false');
INSERT OR IGNORE INTO settings (key, value) VALUES ('auto_focus_composer', 'true');
INSERT OR IGNORE INTO settings (key, value) VALUES ('save_draft_messages', 'true');

-- Database version
CREATE TABLE IF NOT EXISTS schema_version (
    version INTEGER PRIMARY KEY,
    applied_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Insert current version
INSERT OR IGNORE INTO schema_version (version) VALUES (1);
