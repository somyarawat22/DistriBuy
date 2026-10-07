$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
Push-Location $repoRoot
try {
    docker compose up -d --wait

    docker compose exec -T node1 psql -U postgres -d ecommerce_node1 -c @'
ALTER TABLE users ADD COLUMN IF NOT EXISTS is_admin BOOLEAN NOT NULL DEFAULT FALSE;
INSERT INTO users (name, email, password_hash, is_admin)
VALUES ('Admin User', 'admin@distribuy.local', 'demo_admin_hash', TRUE)
ON CONFLICT (email) DO UPDATE SET is_admin = EXCLUDED.is_admin;
INSERT INTO carts (user_id)
SELECT user_id FROM users
WHERE email = 'rahul@example.com'
  AND NOT EXISTS (
      SELECT 1 FROM carts existing_cart
      WHERE existing_cart.user_id = users.user_id
  );
INSERT INTO cart_items (cart_id, product_id, quantity)
SELECT carts.cart_id, 1, 1
FROM carts
JOIN users ON users.user_id = carts.user_id
WHERE users.email = 'rahul@example.com'
  AND NOT EXISTS (
      SELECT 1 FROM cart_items existing_item
      WHERE existing_item.cart_id = carts.cart_id AND existing_item.product_id = 1
  );
'@

    docker compose exec -T node2 psql -U postgres -d ecommerce_node2 -c @'
INSERT INTO products (name, description, price, category)
SELECT new_product.name, new_product.description, new_product.price, new_product.category
FROM (VALUES
    ('Smart Watch', 'Fitness tracking smartwatch', 3299.00, 'Electronics'),
    ('Laptop Stand', 'Adjustable aluminum laptop stand', 1799.00, 'Office'),
    ('Coffee Maker', 'Compact filter coffee maker', 2199.00, 'Home')
) AS new_product(name, description, price, category)
WHERE NOT EXISTS (
    SELECT 1 FROM products existing_product
    WHERE existing_product.name = new_product.name
);
INSERT INTO inventory (product_id, quantity)
SELECT product_id, product_seed.quantity
FROM products
JOIN (VALUES
    ('Smart Watch', 8),
    ('Laptop Stand', 12),
    ('Coffee Maker', 6)
) AS product_seed(name, quantity) USING (name)
ON CONFLICT (product_id) DO NOTHING;
'@

    docker compose exec -T node3 psql -U postgres -d ecommerce_node3 -c @'
DO $$
DECLARE
    demo_order_id BIGINT;
BEGIN
    IF NOT EXISTS (
        SELECT 1 FROM payments WHERE transaction_reference = 'DEMO-PAY-0001'
    ) THEN
        INSERT INTO orders (user_id, total_amount, status)
        VALUES (2, 2499.00, 'CONFIRMED')
        RETURNING order_id INTO demo_order_id;

        INSERT INTO order_items (order_id, product_id, quantity, price)
        VALUES (demo_order_id, 1, 1, 2499.00);

        INSERT INTO payments (order_id, amount, status, transaction_reference)
        VALUES (demo_order_id, 2499.00, 'PAID', 'DEMO-PAY-0001');
    END IF;
END
$$;
'@

    powershell -ExecutionPolicy Bypass -File .\replication\setup-ring.ps1
    Write-Host 'Demo data seeded. Run show-demo-status.ps1 or coordinator --demo.' -ForegroundColor Green
}
finally {
    Pop-Location
}
