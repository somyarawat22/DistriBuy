$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
Push-Location $repoRoot

function Invoke-NodeSql([string]$service, [string]$database, [string]$sql) {
    docker compose exec -T $service psql -U postgres -d $database -c $sql
}

function Write-Section([string]$title) {
    Write-Host ""
    Write-Host "=== $title ===" -ForegroundColor Cyan
}

Write-Section 'Distributed E-Commerce Database Demo'
Write-Host "Node 1: localhost:5433 -> users and carts"
Write-Host "Node 2: localhost:5434 -> products and inventory"
Write-Host "Node 3: localhost:5435 -> orders and payments"

Write-Section 'Container Health and Ports'
docker compose ps

$nodes = @(
    @{ Service = 'node1'; Database = 'ecommerce_node1'; Domain = 'users, carts, cart_items' },
    @{ Service = 'node2'; Database = 'ecommerce_node2'; Domain = 'products, inventory' },
    @{ Service = 'node3'; Database = 'ecommerce_node3'; Domain = 'orders, order_items, payments' }
)

foreach ($node in $nodes) {
    Write-Section "$($node.Service): $($node.Domain)"
    docker compose port $node.Service 5432
    Invoke-NodeSql $node.Service $node.Database "SELECT current_database() AS database, current_setting('listen_addresses') AS listen_addresses, current_setting('wal_level') AS wal_level, current_setting('max_prepared_transactions') AS max_prepared_transactions"
    Invoke-NodeSql $node.Service $node.Database "SELECT tablename FROM pg_tables WHERE schemaname = 'public' ORDER BY tablename"
    switch ($node.Service) {
        'node1' {
            Invoke-NodeSql $node.Service $node.Database "SELECT 'users' AS table_name, COUNT(*) AS row_count FROM users UNION ALL SELECT 'orders', COUNT(*) FROM orders"
        }
        'node2' {
            Invoke-NodeSql $node.Service $node.Database "SELECT 'users replica' AS table_name, COUNT(*) AS row_count FROM users UNION ALL SELECT 'products', COUNT(*) FROM products UNION ALL SELECT 'inventory', COUNT(*) FROM inventory"
        }
        'node3' {
            Invoke-NodeSql $node.Service $node.Database "SELECT 'products replica' AS table_name, COUNT(*) AS row_count FROM products UNION ALL SELECT 'inventory replica', COUNT(*) FROM inventory UNION ALL SELECT 'orders', COUNT(*) FROM orders"
        }
    }
    Invoke-NodeSql $node.Service $node.Database "SELECT subname, worker_type, pid, received_lsn, latest_end_lsn, latest_end_time FROM pg_stat_subscription"
}

Write-Section 'Coordinator Health and Routing'
docker compose exec -T coordinator /usr/local/bin/coordinator --once

Write-Section 'Coordinator Store View'
docker compose exec -T coordinator /usr/local/bin/coordinator --demo

Write-Section 'Replication Slots'
foreach ($node in $nodes) {
    Invoke-NodeSql $node.Service $node.Database "SELECT slot_name, slot_type, active FROM pg_replication_slots"
}

Write-Section 'Seed Data Snapshot'
Invoke-NodeSql 'node1' 'ecommerce_node1' "SELECT user_id, name, email, CASE WHEN is_admin THEN 'ADMIN' ELSE 'CUSTOMER' END AS account_type FROM users ORDER BY user_id"
Invoke-NodeSql 'node2' 'ecommerce_node2' "SELECT p.product_id, p.name, p.price, i.quantity FROM products p JOIN inventory i USING (product_id) ORDER BY p.product_id"
Invoke-NodeSql 'node3' 'ecommerce_node3' "SELECT order_id, user_id, total_amount, status FROM orders ORDER BY order_id"

Pop-Location