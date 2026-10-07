CREATE TABLE users (
    user_id BIGSERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    email VARCHAR(255) NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    is_admin BOOLEAN NOT NULL DEFAULT FALSE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE carts (
    cart_id BIGSERIAL PRIMARY KEY,
    user_id BIGINT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE cart_items (
    cart_item_id BIGSERIAL PRIMARY KEY,
    cart_id BIGINT NOT NULL REFERENCES carts(cart_id) ON DELETE CASCADE,
    product_id BIGINT NOT NULL,
    quantity INTEGER NOT NULL CHECK (quantity > 0),
    UNIQUE (cart_id, product_id)
);

CREATE INDEX idx_users_email ON users(email);
CREATE INDEX idx_carts_user ON carts(user_id);
CREATE INDEX idx_cart_items_cart ON cart_items(cart_id);

INSERT INTO users (name, email, password_hash, is_admin)
VALUES
('Admin User', 'admin@distribuy.local', 'demo_admin_hash', TRUE),
('Rahul Sharma', 'rahul@example.com', 'hash_123', FALSE),
('Aman Verma', 'aman@example.com', 'hash_456', FALSE),
('Priya Singh', 'priya@example.com', 'hash_789', FALSE);

INSERT INTO carts (user_id)
VALUES (2);

INSERT INTO cart_items (cart_id, product_id, quantity)
VALUES (1, 1, 1);