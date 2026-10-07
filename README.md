# DistriBuy Distributed Database Demo

DistriBuy is a local distributed-database demonstration for an e-commerce system. It runs three independent PostgreSQL 17 services and a C++ coordinator in Docker Compose.

This milestone demonstrates:

- Domain partitioning across three database nodes
- Seeded users, products, inventory, and order tables
- PostgreSQL logical replication in a ring
- Concurrent coordinator health checks
- Replication and node-failure evidence suitable for a project presentation

The current coordinator is a health monitor and configured ownership display. REST APIs, automatic failover, distributed query routing, and 2PC checkout are future phases and are not claimed by this demo.

## Architecture

| Service | Host port | Owns | Replica destination |
| --- | ---: | --- | --- |
| `node1` | `5433` | Users and carts | Node 2 |
| `node2` | `5434` | Products and inventory | Node 3 |
| `node3` | `5435` | Orders and payments | Node 1 |
| `coordinator` | internal | Health checks and routing display | N/A |

Each PostgreSQL service has its own database, volume, schema, indexes, WAL, and transaction manager. Logical replication is asynchronous, so allow a few seconds for a change to appear on its replica.

## Prerequisites

Install:

- Git
- Docker Desktop with Linux containers enabled
- Docker Compose v2
- PowerShell 5.1 or PowerShell 7+

Before starting, make sure Docker Desktop is running and host ports `5433`, `5434`, and `5435` are available.

## Fresh laptop setup

Clone the repository and enter its root directory:

```powershell
git clone <your-repository-url>
cd DistriBuy
docker version
docker compose version
```

Start all services and build the coordinator:

```powershell
docker compose up -d --build --wait
docker compose ps
```

The three database services should show `healthy`. The coordinator starts after all three databases are healthy.

Configure the logical-replication ring:

```powershell
powershell -ExecutionPolicy Bypass -File .\replication\setup-ring.ps1
```

The setup script can be run from any directory. It waits for PostgreSQL readiness, creates the replication role, applies replica schemas, and creates the publications and subscriptions only when they do not already exist.

Seed the understandable e-commerce demo data:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\seed-demo-data.ps1
```

This creates an admin user, customer users, a cart, six products, inventory, and one confirmed sample order. It is safe to run again.

Show the complete system state:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\show-demo-status.ps1
```

## Presentation demo

Run the status script first so the teacher can see the three healthy nodes, their ports, tables, replication settings, and coordinator output.

### 1. Show data on each primary

```powershell
docker compose exec -T node1 psql -U postgres -d ecommerce_node1 -c "SELECT user_id, name, email FROM users;"
docker compose exec -T node2 psql -U postgres -d ecommerce_node2 -c "SELECT p.product_id, p.name, p.price, i.quantity FROM products p JOIN inventory i USING (product_id);"
docker compose exec -T node3 psql -U postgres -d ecommerce_node3 -c "SELECT * FROM orders;"
```

### 2. Prove users replicate from Node 1 to Node 2

```powershell
docker compose exec -T node1 psql -U postgres -d ecommerce_node1 -c "UPDATE users SET name = 'Rahul Demo' WHERE email = 'rahul@example.com';"
Start-Sleep -Seconds 3
docker compose exec -T node2 psql -U postgres -d ecommerce_node2 -c "SELECT user_id, name, email FROM users WHERE email = 'rahul@example.com';"
```

### 3. Prove inventory replicates from Node 2 to Node 3

```powershell
docker compose exec -T node2 psql -U postgres -d ecommerce_node2 -c "UPDATE inventory SET quantity = quantity - 1 WHERE product_id = 1 AND quantity > 0;"
Start-Sleep -Seconds 3
docker compose exec -T node3 psql -U postgres -d ecommerce_node3 -c "SELECT * FROM inventory WHERE product_id = 1;"
```

### 4. Prove orders replicate from Node 3 to Node 1

```powershell
docker compose exec -T node3 psql -U postgres -d ecommerce_node3 -c "INSERT INTO orders (user_id, total_amount, status) VALUES (1, 2499.00, 'PENDING');"
Start-Sleep -Seconds 3
docker compose exec -T node1 psql -U postgres -d ecommerce_node1 -c "SELECT order_id, user_id, total_amount, status FROM orders ORDER BY order_id DESC LIMIT 5;"
```

### 5. Show coordinator activity

The coordinator checks all three nodes concurrently every five seconds:

```powershell
docker compose logs -f coordinator
```

Look for `status=ACTIVE` for `node1`, `node2`, and `node3`.

For a simple business view from the coordinator itself:

```powershell
docker compose exec -T coordinator /usr/local/bin/coordinator --demo
```

The coordinator reads each business domain from the correct node and prints counts such as customers on Node 1, products on Node 2, and orders/payments on Node 3. This is the current coordinator demonstration: it knows the node layout, checks node health, and reads the distributed store. It is not yet a web API or automatic failover service.

## Add data during the presentation

For this milestone, add data to the node that owns that business domain. The coordinator observes the distributed layout, while logical replication copies rows to the next node in the ring.

Add an admin-style account on Node 1:

```powershell
docker compose exec -T node1 psql -U postgres -d ecommerce_node1 -c "INSERT INTO users (name, email, password_hash, is_admin) VALUES ('Demo Manager', 'manager@distribuy.local', 'demo_hash', TRUE) ON CONFLICT (email) DO NOTHING;"
```

Add a product and stock on Node 2:

```powershell
docker compose exec -T node2 psql -U postgres -d ecommerce_node2 -c "INSERT INTO products (name, description, price, category) VALUES ('Wireless Earbuds', 'Bluetooth earbuds for the demo', 1299.00, 'Electronics') RETURNING product_id;"
docker compose exec -T node2 psql -U postgres -d ecommerce_node2 -c "INSERT INTO inventory (product_id, quantity) SELECT product_id, 20 FROM products WHERE name = 'Wireless Earbuds' AND NOT EXISTS (SELECT 1 FROM inventory i WHERE i.product_id = products.product_id);"
```

Add an order on Node 3:

```powershell
docker compose exec -T node3 psql -U postgres -d ecommerce_node3 -c "INSERT INTO orders (user_id, total_amount, status) VALUES (2, 1299.00, 'PENDING');"
```

Wait a few seconds, then run the status report again:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\show-demo-status.ps1
```

Explain the flow as: Node 1 owns customers, Node 2 owns catalog and stock, Node 3 owns orders, and the coordinator knows those routes while PostgreSQL logical replication creates the ring copies.

### 6. Demonstrate failure detection

In one PowerShell window, stop Node 2:

```powershell
docker compose stop node2
```

In another window, watch the coordinator:

```powershell
docker compose logs -f coordinator
```

The coordinator should report Node 2 as `FAILED` while the other nodes remain active. Restart it and verify recovery:

```powershell
docker compose up -d --wait node2
docker compose ps
docker compose exec -T coordinator /usr/local/bin/coordinator --once
```

This proves health detection and recovery visibility. It does not yet perform automatic traffic failover.

## Useful inspection commands

View publications and subscriptions:

```powershell
docker compose exec -T node1 psql -U postgres -d ecommerce_node1 -c "SELECT pubname FROM pg_publication;"
docker compose exec -T node2 psql -U postgres -d ecommerce_node2 -c "SELECT subname, worker_type, pid, latest_end_time FROM pg_stat_subscription;"
```

View service logs:

```powershell
docker compose logs node1 node2 node3
docker compose logs -f coordinator
```

## Existing Docker volumes

PostgreSQL initialization SQL runs only when a volume is created for the first time. If the containers already exist but tables are missing, run:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\initialize-existing-volumes.ps1
powershell -ExecutionPolicy Bypass -File .\scripts\seed-demo-data.ps1
```

If another checkout is already using ports or old fixed container names, stop that project from its original directory before starting this checkout:

```powershell
docker compose -p distributed-ecommerce-db down
```

This does not delete its named volumes. Do not use `-v` unless you intend to delete its database data.

## Clean reset

The following permanently deletes the three named database volumes and all demo data:

```powershell
docker compose down -v
docker compose up -d --build --wait
powershell -ExecutionPolicy Bypass -File .\replication\setup-ring.ps1
powershell -ExecutionPolicy Bypass -File .\scripts\show-demo-status.ps1
```

Use this when you need a completely reproducible presentation state.

## Troubleshooting

**Port is already allocated:** stop the other Docker project using ports `5433`-`5435`, or identify it with `docker ps`.

**A service is unhealthy:** inspect its logs with `docker compose logs node1`, `node2`, or `node3`. Confirm Docker Desktop is running and the repository is in a Docker-shared location.

**Replication is not visible immediately:** logical replication is asynchronous. Wait a few seconds and run the target query again. Check `pg_stat_subscription` with the status script.

**PowerShell blocks scripts:** use the documented `-ExecutionPolicy Bypass` command for the current process invocation.

**Tables are missing after a restart:** volumes preserve old state, so initialization scripts do not rerun. Use the existing-volume script or the destructive clean reset.

## Security note

The credentials are intentionally simple for a local classroom demo. PostgreSQL ports are published on the host and the included `pg_hba.conf` is not production-safe. Do not expose this setup to an untrusted network or reuse these passwords.

## Next phases

After this presentation milestone, the project can add a web API, real coordinator request routing, a distributed checkout transaction using PostgreSQL prepared transactions, replication freshness checks, and automatic failover. Those features should be added only after this infrastructure demo is stable and repeatable.