CREATE TABLE products (
    product_id BIGSERIAL PRIMARY KEY,
    name VARCHAR(200) NOT NULL,
    description TEXT,
    price NUMERIC(10,2) NOT NULL CHECK (price >= 0),
    category VARCHAR(100) NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE inventory (
    product_id BIGINT PRIMARY KEY
        REFERENCES products(product_id) ON DELETE CASCADE,
    quantity INTEGER NOT NULL CHECK (quantity >= 0),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_products_category
ON products(category);

INSERT INTO products
(name, description, price, category)
VALUES
('Running Shoes', 'Lightweight running shoes', 2499.00, 'Shoes'),
('Hoodie', 'Cotton oversized hoodie', 1499.00, 'Clothing'),
('Backpack', 'Water resistant backpack', 1999.00, 'Bags'),
('Smart Watch', 'Fitness tracking smartwatch', 3299.00, 'Electronics'),
('Laptop Stand', 'Adjustable aluminum laptop stand', 1799.00, 'Office'),
('Coffee Maker', 'Compact filter coffee maker', 2199.00, 'Home');

INSERT INTO inventory (product_id, quantity)
VALUES
(1, 10),
(2, 25),
(3, 15),
(4, 8),
(5, 12),
(6, 6);