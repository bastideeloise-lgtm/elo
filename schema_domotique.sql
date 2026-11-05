-- ============================================================
-- Projet Domotique - ESGI 2026
-- Base de données liant les utilisateurs de l'application
-- au simulateur SimDom (équipements, scénarios, caméras)
-- Syntaxe MySQL / MariaDB (facilement adaptable à PostgreSQL)
-- ============================================================

CREATE DATABASE IF NOT EXISTS domotique
    CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE domotique;

-- ------------------------------------------------------------
-- 1. UTILISATEURS
-- Rôles : administrator, user, restrictedUser, surveillance
-- (cf. §3.1 du sujet : droits différents selon le profil)
-- ------------------------------------------------------------
CREATE TABLE users (
    id              INT PRIMARY KEY AUTO_INCREMENT,
    username        VARCHAR(50)  NOT NULL UNIQUE,
    password_hash   VARCHAR(255) NOT NULL,
    role            ENUM('administrator', 'user', 'restrictedUser', 'surveillance')
                    NOT NULL DEFAULT 'user',
    created_at      TIMESTAMP    DEFAULT CURRENT_TIMESTAMP,
    last_login_at   TIMESTAMP    NULL
);

-- ------------------------------------------------------------
-- 2. SERVEURS (instances SimDom / serveurs domotiques)
-- Correspond à l'onglet "Serveurs" de la fenêtre Configuration
-- ------------------------------------------------------------
CREATE TABLE servers (
    id          INT PRIMARY KEY AUTO_INCREMENT,
    ip_address  VARCHAR(45) NOT NULL,          -- ex: localhost, 192.168.1.10
    port        INT NOT NULL DEFAULT 64705,
    comment     VARCHAR(255)                    -- ex: "Raspberry", "PC portable 1"
);

-- ------------------------------------------------------------
-- 3. SESSIONS (traçabilité des connexions client -> serveur)
-- ------------------------------------------------------------
CREATE TABLE sessions (
    id                  BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id             INT NOT NULL,
    server_id           INT NOT NULL,
    connected_at        TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    disconnected_at     TIMESTAMP NULL,
    FOREIGN KEY (user_id)   REFERENCES users(id)   ON DELETE CASCADE,
    FOREIGN KEY (server_id) REFERENCES servers(id) ON DELETE CASCADE
);

-- ------------------------------------------------------------
-- 4. ÉTAGES (Exterieur, Sous-sol, Rez-de-chaussee, Premier_etage, Grenier)
-- Rattachés à un serveur car chaque instance SimDom a sa propre maison
-- ------------------------------------------------------------
CREATE TABLE floors (
    id              INT PRIMARY KEY AUTO_INCREMENT,
    server_id       INT NOT NULL,
    name            VARCHAR(50) NOT NULL,
    display_order   INT DEFAULT 0,
    FOREIGN KEY (server_id) REFERENCES servers(id) ON DELETE CASCADE,
    UNIQUE (server_id, name)
);

-- ------------------------------------------------------------
-- 5. PIÈCES
-- ------------------------------------------------------------
CREATE TABLE rooms (
    id          INT PRIMARY KEY AUTO_INCREMENT,
    floor_id    INT NOT NULL,
    name        VARCHAR(100) NOT NULL,     -- ex: "Suite_parentale", "Cuisine"
    FOREIGN KEY (floor_id) REFERENCES floors(id) ON DELETE CASCADE,
    UNIQUE (floor_id, name)
);

-- ------------------------------------------------------------
-- 6. TYPES D'ÉQUIPEMENTS
-- Luminaire, Prise_commandee, Volet_roulant, Climatisation_reversible
-- ------------------------------------------------------------
CREATE TABLE equipment_types (
    id      INT PRIMARY KEY AUTO_INCREMENT,
    name    VARCHAR(50) NOT NULL UNIQUE
);

INSERT INTO equipment_types (name) VALUES
    ('Luminaire'),
    ('Prise_commandee'),
    ('Volet_roulant'),
    ('Climatisation_reversible');

-- ------------------------------------------------------------
-- 7. ÉQUIPEMENTS
-- automate_ip + automate_entry = adressage réel utilisé pour
-- piloter SimDom en TCP/IP (cf. §14.4 du sujet)
-- Message envoyé au simulateur : "<automate_ip> <automate_entry> <0|1|10>"
-- ------------------------------------------------------------
CREATE TABLE equipments (
    id              INT PRIMARY KEY AUTO_INCREMENT,
    room_id         INT NOT NULL,
    type_id         INT NOT NULL,
    name            VARCHAR(100) NOT NULL,     -- ex: "Hublot", "Lumieres_plafond_x2"
    automate_ip     VARCHAR(45) NOT NULL,      -- ex: 192.168.0.100
    automate_entry  VARCHAR(20) NOT NULL,      -- ex: 00010010
    current_state   ENUM('on', 'off', 'fault') NOT NULL DEFAULT 'off',
    current_mode    ENUM('cooling', 'heating') NULL,   -- climatisation uniquement
    power_watts     DECIMAL(6,1) NULL,
    comment         VARCHAR(255),
    FOREIGN KEY (room_id) REFERENCES rooms(id) ON DELETE CASCADE,
    FOREIGN KEY (type_id) REFERENCES equipment_types(id),
    UNIQUE (automate_ip, automate_entry)
);

-- ------------------------------------------------------------
-- 8. SCÉNARIOS (langage LDSD, cf. §6.2 du sujet)
-- ------------------------------------------------------------
CREATE TABLE scenarios (
    id          INT PRIMARY KEY AUTO_INCREMENT,
    name        VARCHAR(100) NOT NULL,
    ldsd_code   TEXT NOT NULL,
    owner_id    INT NOT NULL,
    is_running  BOOLEAN DEFAULT FALSE,
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    FOREIGN KEY (owner_id) REFERENCES users(id)
);

-- ------------------------------------------------------------
-- 9. HISTORIQUE DES ÉTATS (base des statistiques, §9 du sujet)
-- user_id NULL  -> action venant d'un scénario
-- scenario_id NULL -> action manuelle d'un utilisateur
-- ------------------------------------------------------------
CREATE TABLE equipment_history (
    id              BIGINT PRIMARY KEY AUTO_INCREMENT,
    equipment_id    INT NOT NULL,
    user_id         INT NULL,
    scenario_id     INT NULL,
    old_state       ENUM('on', 'off', 'fault'),
    new_state       ENUM('on', 'off', 'fault') NOT NULL,
    changed_at      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (equipment_id) REFERENCES equipments(id) ON DELETE CASCADE,
    FOREIGN KEY (user_id)      REFERENCES users(id)       ON DELETE SET NULL,
    FOREIGN KEY (scenario_id)  REFERENCES scenarios(id)   ON DELETE SET NULL
);

-- ------------------------------------------------------------
-- 10. CAMÉRAS (cf. §7 et §10.1.4 du sujet)
-- ------------------------------------------------------------
CREATE TABLE cameras (
    id                  INT PRIMARY KEY AUTO_INCREMENT,
    name                VARCHAR(100) NOT NULL,
    ip_internal         VARCHAR(45),
    port_internal       INT,
    ip_external         VARCHAR(45),
    port_external       INT,
    video_path          VARCHAR(255) DEFAULT '/video/mjpg.cgi',
    image_path          VARCHAR(255) DEFAULT '/image/jpeg.cgi',
    login               VARCHAR(50),
    password_encrypted  VARCHAR(255),
    is_webcam           BOOLEAN DEFAULT FALSE,
    zoom_level          VARCHAR(5)  DEFAULT 'X1',
    has_audio           BOOLEAN DEFAULT FALSE,
    has_night_mode      BOOLEAN DEFAULT FALSE,
    is_motorized        BOOLEAN DEFAULT FALSE,
    initial_azimuth     DECIMAL(5,1) DEFAULT 0,
    initial_site        DECIMAL(5,1) DEFAULT 0,
    zone_threshold      DECIMAL(4,1),
    inertia             INT,
    scan_interval_ms    INT,
    pixel_threshold     INT,
    comment             VARCHAR(255)
);

-- Positions mémorisées pour une caméra motorisée ("Autres positions")
CREATE TABLE camera_positions (
    id          INT PRIMARY KEY AUTO_INCREMENT,
    camera_id   INT NOT NULL,
    name        VARCHAR(100) NOT NULL,
    azimuth     DECIMAL(5,1) NOT NULL,
    site        DECIMAL(5,1) NOT NULL,
    FOREIGN KEY (camera_id) REFERENCES cameras(id) ON DELETE CASCADE
);

-- Photos prises manuellement ou par détection de mouvement
CREATE TABLE camera_snapshots (
    id              BIGINT PRIMARY KEY AUTO_INCREMENT,
    camera_id       INT NOT NULL,
    file_path       VARCHAR(255) NOT NULL,
    taken_at        TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    triggered_by    ENUM('manual', 'motion_detection') DEFAULT 'manual',
    FOREIGN KEY (camera_id) REFERENCES cameras(id) ON DELETE CASCADE
);

-- ------------------------------------------------------------
-- INDEX UTILES (accélèrent les requêtes de pilotage / stats)
-- ------------------------------------------------------------
CREATE INDEX idx_equipments_room       ON equipments(room_id);
CREATE INDEX idx_rooms_floor           ON rooms(floor_id);
CREATE INDEX idx_floors_server         ON floors(server_id);
CREATE INDEX idx_history_equipment     ON equipment_history(equipment_id);
CREATE INDEX idx_history_changed_at    ON equipment_history(changed_at);
CREATE INDEX idx_scenarios_owner       ON scenarios(owner_id);
CREATE INDEX idx_sessions_user         ON sessions(user_id);

-- ------------------------------------------------------------
-- DONNÉES DE DÉMARRAGE (exemple, à adapter)
-- ------------------------------------------------------------
INSERT INTO users (username, password_hash, role) VALUES
    ('root',      '$2y$10$CHANGE_ME_HASH', 'administrator');

INSERT INTO servers (ip_address, port, comment) VALUES
    ('localhost', 64705, 'Serveur SimDom local');
