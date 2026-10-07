$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
Push-Location $repoRoot

$nodes = @(
    @{ Service = 'node1'; Database = 'ecommerce_node1'; Tables = @('users', 'carts', 'cart_items') },
    @{ Service = 'node2'; Database = 'ecommerce_node2'; Tables = @('products', 'inventory') },
    @{ Service = 'node3'; Database = 'ecommerce_node3'; Tables = @('orders', 'order_items', 'payments') }
)

foreach ($node in $nodes) {
    $databaseExists = docker compose exec -T $node.Service psql -U postgres -d postgres -tAc "SELECT 1 FROM pg_database WHERE datname = '$($node.Database)'"

    if ($databaseExists.Trim() -ne '1') {
        docker compose exec -T $node.Service psql -U postgres -d postgres -c "CREATE DATABASE $($node.Database)"
    }

    $tableExists = docker compose exec -T $node.Service psql -U postgres -d $node.Database -tAc "SELECT 1 FROM pg_tables WHERE schemaname = 'public' AND tablename = '$($node.Tables[0])'"

    if ($tableExists.Trim() -ne '1') {
        docker compose exec -T $node.Service psql -U postgres -d $node.Database -f /docker-entrypoint-initdb.d/01_schema.sql
    }

    if ($node.Service -eq 'node1' -or $node.Service -eq 'node2') {
        docker compose exec -T $node.Service psql -U postgres -d $node.Database -c "ALTER TABLE users ADD COLUMN IF NOT EXISTS is_admin BOOLEAN NOT NULL DEFAULT FALSE"
    }
}

docker compose ps
Pop-Location