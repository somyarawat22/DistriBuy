$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
Push-Location $repoRoot
try {

$replicationUser = 'replicator'
$replicationPassword = 'replicator'

$replicationLinks = @(
    @{
        SourceService = 'node1'
        SourceHost = 'node1'
        SourceDatabase = 'ecommerce_node1'
        SourcePublication = 'node1_users_publication'
        TargetService = 'node2'
        TargetDatabase = 'ecommerce_node2'
        Subscription = 'node1_users_subscription'
        SchemaFile = 'replication/schemas/node2_users.sql'
        Tables = 'users, carts, cart_items'
    },
    @{
        SourceService = 'node2'
        SourceHost = 'node2'
        SourceDatabase = 'ecommerce_node2'
        SourcePublication = 'node2_products_publication'
        TargetService = 'node3'
        TargetDatabase = 'ecommerce_node3'
        Subscription = 'node2_products_subscription'
        SchemaFile = 'replication/schemas/node3_products.sql'
        Tables = 'products, inventory'
    },
    @{
        SourceService = 'node3'
        SourceHost = 'node3'
        SourceDatabase = 'ecommerce_node3'
        SourcePublication = 'node3_orders_publication'
        TargetService = 'node1'
        TargetDatabase = 'ecommerce_node1'
        Subscription = 'node3_orders_subscription'
        SchemaFile = 'replication/schemas/node1_orders.sql'
        Tables = 'orders, order_items, payments'
    }
)

docker compose up -d --wait

$createRoleSql = @'
DO $$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM pg_roles WHERE rolname = 'replicator') THEN
        CREATE ROLE replicator WITH LOGIN REPLICATION PASSWORD 'replicator';
    END IF;
END
$$;
'@

foreach ($service in @('node1', 'node2', 'node3')) {
    docker compose exec -T $service psql -U postgres -d postgres -c $createRoleSql
}

foreach ($link in $replicationLinks) {
    Get-Content -Raw (Join-Path $repoRoot $link.SchemaFile) | docker compose exec -T $link.TargetService psql -U postgres -d $link.TargetDatabase

    docker compose exec -T $link.SourceService psql -U postgres -d $link.SourceDatabase -c "GRANT USAGE ON SCHEMA public TO $replicationUser; GRANT SELECT ON TABLE $($link.Tables) TO $replicationUser"

    $publicationExists = docker compose exec -T $link.SourceService psql -U postgres -d $link.SourceDatabase -tAc "SELECT 1 FROM pg_publication WHERE pubname = '$($link.SourcePublication)'"
    if ("$publicationExists".Trim() -ne '1') {
        docker compose exec -T $link.SourceService psql -U postgres -d $link.SourceDatabase -c "CREATE PUBLICATION $($link.SourcePublication) FOR TABLE $($link.Tables)"
    }

    $subscriptionExists = docker compose exec -T $link.TargetService psql -U postgres -d $link.TargetDatabase -tAc "SELECT 1 FROM pg_subscription WHERE subname = '$($link.Subscription)'"
    if ("$subscriptionExists".Trim() -ne '1') {
        $connection = "host=$($link.SourceHost) dbname=$($link.SourceDatabase) user=$replicationUser password=$replicationPassword"
        docker compose exec -T $link.TargetService psql -U postgres -d $link.TargetDatabase -c "CREATE SUBSCRIPTION $($link.Subscription) CONNECTION '$connection' PUBLICATION $($link.SourcePublication) WITH (copy_data = true)"
    }
}

docker compose ps
} finally {
    Pop-Location
}