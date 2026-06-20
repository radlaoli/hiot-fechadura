CREATE DATABASE IF NOT EXISTS fechadura_db;
USE fechadura_db;

CREATE TABLE IF NOT EXISTS usuarios (
    id INT AUTO_INCREMENT PRIMARY KEY,
    nome VARCHAR(100) NOT NULL
);

CREATE TABLE IF NOT EXISTS fechaduras (
    id INT AUTO_INCREMENT PRIMARY KEY,
    nome VARCHAR(100) NOT NULL
);

CREATE TABLE IF NOT EXISTS permissoes_fechadura (
    tag_nfc VARCHAR(255) PRIMARY KEY,
    usuario_id INT,
    fechadura_id INT DEFAULT 1,
    ativo INT DEFAULT 1,
    FOREIGN KEY (usuario_id) REFERENCES usuarios(id) ON DELETE CASCADE,
    FOREIGN KEY (fechadura_id) REFERENCES fechaduras(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS historico_sistema (
    id INT AUTO_INCREMENT PRIMARY KEY,
    tipo_evento VARCHAR(50) NOT NULL,
    tag_nfc VARCHAR(255),
    usuario_id INT,
    autorizado INT, 
    descricao TEXT,
    data_evento TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (usuario_id) REFERENCES usuarios(id) ON DELETE SET NULL
);